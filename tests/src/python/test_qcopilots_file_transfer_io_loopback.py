"""Real SSH/SFTP adapter tests restricted to an isolated IPv4 loopback server.

The locked SSH dependencies must be on PYTHONPATH. No packages are installed by
this test. Host keys stay in memory and all server files use TemporaryDirectory.
"""

from __future__ import annotations

import errno
import hashlib
import logging
import os
import random
import socket
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path, PurePosixPath
from unittest import mock


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
if str(PLUGINS_ROOT) not in sys.path:
    sys.path.insert(0, str(PLUGINS_ROOT))

try:
    import paramiko
except ImportError:
    paramiko = None

from qcopilots_mcp_server_processing_general.file_transfer_cancellation import (
    CancellationController,
)
from qcopilots_mcp_server_processing_general.file_transfer_io import (
    SftpClientAdapter,
    SftpFileAdapter,
)


class LoopbackAuthentication(paramiko.ServerInterface if paramiko else object):
    def get_allowed_auths(self, _username):
        return "password"

    def check_auth_password(self, username, password):
        if username == "isolated-test" and password == "loopback-test-only":
            return paramiko.AUTH_SUCCESSFUL
        return paramiko.AUTH_FAILED

    def check_channel_request(self, kind, _channel_id):
        return paramiko.OPEN_SUCCEEDED if kind == "session" else paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED


class LoopbackHandle(paramiko.SFTPHandle if paramiko else object):
    def __init__(self, owner, descriptor, flags):
        super().__init__(flags)
        self.owner = owner
        mode = "r+b" if flags & os.O_RDWR else "wb" if flags & os.O_WRONLY else "rb"
        stream = os.fdopen(descriptor, mode, buffering=0)
        if flags & (os.O_WRONLY | os.O_RDWR):
            self.writefile = stream
        if not flags & os.O_WRONLY:
            self.readfile = stream
        self.stream = stream

    def read(self, offset, length):
        self.owner.read_requests.append((offset, length))
        if self.owner.block_reads:
            self.owner.io_observed.set()
            if not self.owner.io_release.wait(5):
                return paramiko.SFTP_FAILURE
        if self.owner.fail_read_offset is not None and offset >= self.owner.fail_read_offset:
            return paramiko.SFTP_PERMISSION_DENIED
        if self.owner.maximum_read is not None:
            length = min(length, self.owner.maximum_read)
        return super().read(offset, length)

    def write(self, offset, data):
        self.owner.write_requests.append((offset, len(data)))
        if self.owner.block_writes:
            self.owner.io_observed.set()
            if not self.owner.io_release.wait(5):
                return paramiko.SFTP_FAILURE
        if self.owner.fail_write_offset is not None and offset == self.owner.fail_write_offset:
            return paramiko.SFTP_PERMISSION_DENIED
        return super().write(offset, data)

    def stat(self):
        return paramiko.SFTPAttributes.from_stat(os.fstat(self.stream.fileno()))


class LoopbackFilesystem(paramiko.SFTPServerInterface if paramiko else object):
    def __init__(self, server, *, owner):
        super().__init__(server)
        self.owner = owner

    def session_started(self):
        self.owner.sftp_thread = threading.current_thread()

    def local(self, path):
        path = PurePosixPath(path)
        if not path.is_absolute() or ".." in path.parts:
            raise PermissionError(errno.EACCES, "Only isolated absolute paths are permitted")
        result = self.owner.root.joinpath(*path.parts[1:])
        result.relative_to(self.owner.root)
        return result

    def open(self, path, flags, _attributes):
        try:
            descriptor = os.open(self.local(path), flags | getattr(os, "O_BINARY", 0), 0o600)
            handle = LoopbackHandle(self.owner, descriptor, flags)
            self.owner.server_handles.append(handle)
            return handle
        except OSError as error:
            return paramiko.SFTPServer.convert_errno(error.errno)

    def stat(self, path):
        try:
            return paramiko.SFTPAttributes.from_stat(self.local(path).stat())
        except OSError as error:
            return paramiko.SFTPServer.convert_errno(error.errno)

    lstat = stat

    def list_folder(self, path):
        try:
            result = []
            for child in self.local(path).iterdir():
                metadata = paramiko.SFTPAttributes.from_stat(child.stat())
                metadata.filename = child.name
                result.append(metadata)
            return result
        except OSError as error:
            return paramiko.SFTPServer.convert_errno(error.errno)


class ReorderingSftpServer(paramiko.SFTPServer if paramiko else object):
    """Reverse complete eight-packet WRITE windows when requested by a test."""

    def __init__(self, *args, **kwargs):
        self.owner = kwargs["owner"]
        self.processing_write = False
        self.pending_status = []
        super().__init__(*args, **kwargs)

    def _process(self, packet_type, request_number, message):
        self.processing_write = packet_type == paramiko.sftp.CMD_WRITE
        try:
            return super()._process(packet_type, request_number, message)
        finally:
            self.processing_write = False

    def _send_status(self, request_number, code, description=None):
        if self.processing_write and self.owner.reverse_write_responses:
            self.pending_status.append((request_number, code, description))
            if len(self.pending_status) == 8:
                batch, self.pending_status = self.pending_status, []
                for response in reversed(batch):
                    super()._send_status(*response)
            return
        return super()._send_status(request_number, code, description)


@unittest.skipUnless(paramiko is not None, "The locked Paramiko SSH dependencies are unavailable")
class TestFileTransferIoLoopback(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if paramiko.__version__ != "5.0.0":
            raise unittest.SkipTest("This transport integration suite targets locked Paramiko 5.0.0")
        cls.host_key = paramiko.RSAKey.generate(2048)

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qcopilots-file-transfer-loopback-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.stop_server = threading.Event()
        self.io_observed = threading.Event()
        self.io_release = threading.Event()
        self.block_reads = self.block_writes = False
        self.fail_read_offset = self.fail_write_offset = None
        self.maximum_read = None
        self.reverse_write_responses = False
        self.read_requests, self.write_requests = [], []
        self.server_errors, self.thread_errors, self.server_handles = [], [], []
        self.client_transport = self.server_transport = None
        self.client_socket = self.server_socket = None
        self.raw_sftp = self.server_channel = self.sftp_thread = None
        self.controller = CancellationController()
        self.workers = []
        exception_hook = mock.patch(
            "threading.excepthook",
            side_effect=lambda args: self.thread_errors.append(
                f"{args.thread.name}: {args.exc_type.__name__}: {args.exc_value}"
            ),
        )
        exception_hook.start()
        self.addCleanup(exception_hook.stop)
        logger = logging.getLogger("paramiko.transport")
        old_level = logger.level
        logger.setLevel(logging.CRITICAL)
        self.addCleanup(logger.setLevel, old_level)
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.settimeout(5)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(1)
        self.addCleanup(self.close_fixture)
        self.server_thread = threading.Thread(target=self.serve, name="file-transfer-loopback-server", daemon=True)
        self.server_thread.start()
        address = self.listener.getsockname()
        self.assertEqual(address[0], "127.0.0.1")
        self.client_socket = socket.create_connection(address, timeout=5)
        self.controller.register(self.client_socket, kind="socket")
        self.client_transport = paramiko.Transport(self.client_socket)
        self.controller.register(self.client_transport, kind="transport")
        self.client_transport.start_client(timeout=5)
        self.assertEqual(self.client_transport.get_remote_server_key(), self.host_key)
        self.client_transport.auth_password("isolated-test", "loopback-test-only")
        self.raw_sftp = paramiko.SFTPClient.from_transport(self.client_transport)
        self.raw_sftp.get_channel().settimeout(3)
        self.controller.register(self.raw_sftp, kind="sftp")
        self.sftp = SftpClientAdapter(self.raw_sftp, cancel_control=self.controller)

    def serve(self):
        try:
            self.server_socket, address = self.listener.accept()
            if address[0] != "127.0.0.1":
                raise AssertionError("The test server accepted a non-loopback peer")
            self.server_transport = paramiko.Transport(self.server_socket)
            self.server_transport.add_server_key(self.host_key)
            self.server_transport.set_subsystem_handler(
                "sftp", ReorderingSftpServer, LoopbackFilesystem, owner=self,
            )
            self.server_transport.start_server(server=LoopbackAuthentication())
            self.server_channel = self.server_transport.accept(5)
            if self.server_channel is None:
                raise AssertionError("The client did not open an SFTP channel")
            self.stop_server.wait()
        except Exception as error:
            if not self.stop_server.is_set():
                self.server_errors.append(error)

    def close_fixture(self):
        self.io_release.set()
        self.stop_server.set()
        self.controller.cancel()
        for value in (self.server_transport, self.server_socket, self.listener):
            if value is not None:
                value.close()
        threads = [*self.workers, self.sftp_thread, self.client_transport,
                   self.server_transport, self.server_thread]
        remaining = []
        try:
            for thread in threads:
                if thread is not None and thread.ident is not None:
                    thread.join(3)
                    if thread.is_alive():
                        remaining.append(thread.name)
        finally:
            # Paramiko's subsystem teardown closes the channel before its file
            # table. An injected channel failure can interrupt that teardown.
            for handle in self.server_handles:
                handle.close()
        self.assertEqual(remaining, [], "Test threads did not stop")
        self.assertEqual(self.server_errors, [])
        self.assertEqual(self.thread_errors, [])

    @staticmethod
    def payload(size):
        pattern = bytes(range(251))
        return (pattern * (size // len(pattern) + 1))[:size]

    def upload(self, path, payload, chunk=256 * 1024):
        with self.sftp.open(path, "wx") as target:
            self.assertIsInstance(target, SftpFileAdapter)
            for offset in range(0, len(payload), chunk):
                target.write(payload[offset:offset + chunk])
                target.flush()
            self.assertEqual(target.tell(), len(payload))

    def test_real_upload_download_hash_and_metadata(self):
        payload = self.payload(3 * 1024 * 1024 + 127)
        self.upload("/地图 tile.bin", payload)
        self.assertEqual((self.root / "地图 tile.bin").read_bytes(), payload)
        self.assertEqual(self.sftp.stat("/地图 tile.bin").st_size, len(payload))
        self.assertEqual(self.sftp.lstat("/地图 tile.bin").st_size, len(payload))
        self.assertEqual([value.filename for value in self.sftp.listdir_attr("/")], ["地图 tile.bin"])
        with self.sftp.open("/地图 tile.bin", "rb") as source:
            digest = hashlib.sha256()
            total = 0
            while True:
                data = source.read(1024 * 1024)
                if not data:
                    break
                digest.update(data)
                total += len(data)
            self.assertEqual(total, len(payload))
            self.assertEqual(digest.digest(), hashlib.sha256(payload).digest())
            self.assertEqual(source.tell(), len(payload))

    def test_direct_file_adapter_readall_zero_and_boundary_sizes(self):
        payload = self.payload(2 * 1024 * 1024 + 1)
        (self.root / "source.bin").write_bytes(payload)
        with SftpFileAdapter(self.raw_sftp.open("/source.bin", "rb"), cancel_control=self.controller) as source:
            self.assertEqual(source.read(0), b"")
            self.assertEqual(source.tell(), 0)
            for size in (1, 32767, 32768, 32769, 262143, 262144, 262145, 1048575, 1048576, 1048577):
                with self.subTest(size=size):
                    source.seek(0)
                    self.assertEqual(source.read(size), payload[:size])
                    self.assertEqual(source.tell(), size)
            source.seek(0)
            self.assertEqual(source.read(), payload)
            self.assertEqual(source.read(), b"")

    def test_empty_file_eof_and_large_read(self):
        self.upload("/empty.bin", b"")
        with self.sftp.open("/empty.bin", "rb") as source:
            self.assertEqual(source.read(3 * 1024 * 1024), b"")
            self.assertEqual(source.read(), b"")
            self.assertEqual(source.tell(), 0)
        self.assertEqual(self.sftp.stat("/empty.bin").st_size, 0)

    def test_random_seeks_and_relative_and_end_offsets(self):
        payload = self.payload(2 * 1024 * 1024 + 117)
        (self.root / "random.bin").write_bytes(payload)
        randomizer = random.Random(45017)
        with self.sftp.open("/random.bin", "rb") as source:
            for _ in range(25):
                offset = randomizer.randrange(len(payload))
                length = randomizer.randrange(1, 98305)
                source.seek(offset)
                self.assertEqual(source.tell(), offset)
                self.assertEqual(source.read(length), payload[offset:offset + length])
            source.seek(100)
            source.seek(23, os.SEEK_CUR)
            self.assertEqual(source.tell(), 123)
            self.assertEqual(source.read(40000), payload[123:40123])
            source.seek(-53, os.SEEK_END)
            self.assertEqual(source.tell(), len(payload) - 53)
            self.assertEqual(source.read(100), payload[-53:])
            source.seek(len(payload) + 1)
            self.assertEqual(source.read(1), b"")

    def test_resume_upload_and_overwrite_an_existing_interval(self):
        payload = self.payload(1024 * 1024 + 217)
        offset = 3 * 32768 + 11
        (self.root / "resume.bin").write_bytes(payload[:offset])
        with self.sftp.open("/resume.bin", "r+") as target:
            target.seek(offset)
            for start in range(offset, len(payload), 256 * 1024):
                target.write(payload[start:start + 256 * 1024])
            target.flush()
            self.assertEqual(target.tell(), len(payload))
            target.seek(0)
            self.assertEqual(target.read(), payload)
            target.seek(123)
            target.write(b"replacement")
            target.flush()
            target.seek(123)
            self.assertEqual(target.read(11), b"replacement")
        expected = payload[:123] + b"replacement" + payload[134:]
        self.assertEqual((self.root / "resume.bin").read_bytes(), expected)

    def test_server_short_reads_fill_requested_ranges_without_holes(self):
        payload = self.payload(3 * 32768 + 133)
        (self.root / "short.bin").write_bytes(payload)
        self.maximum_read = 7001
        with self.sftp.open("/short.bin", "rb") as source:
            self.assertEqual(source.read(2 * 32768), payload[:2 * 32768])
            self.assertEqual(source.read(1024 * 1024), payload[2 * 32768:])
            self.assertEqual(source.read(1), b"")
            source.seek(0)
            self.assertEqual(source.read(), payload)

    def test_growth_after_observed_eof_is_visible(self):
        initial = self.payload(32768)
        path = self.root / "growing.bin"
        path.write_bytes(initial)
        with self.sftp.open("/growing.bin", "rb") as source:
            self.assertEqual(source.read(len(initial) + 1), initial)
            self.assertEqual(source.read(1), b"")
            with path.open("ab") as local:
                local.write(b"new bytes")
            self.assertEqual(source.read(9), b"new bytes")

    def test_reads_and_writes_have_bounded_requests_and_create_no_threads(self):
        payload = self.payload(2 * 1024 * 1024 + 17)
        maxima = {paramiko.sftp.CMD_READ: 0, paramiko.sftp.CMD_WRITE: 0}
        original_request = self.raw_sftp._async_request
        original_start = threading.Thread.start
        new_threads = []

        def request(owner, kind, *arguments):
            result = original_request(owner, kind, *arguments)
            if kind in maxima:
                maxima[kind] = max(maxima[kind], len(self.raw_sftp._expecting))
            return result

        def start(thread, *args, **kwargs):
            new_threads.append(thread.name)
            return original_start(thread, *args, **kwargs)

        with mock.patch.object(self.raw_sftp, "_async_request", side_effect=request), mock.patch.object(threading.Thread, "start", new=start):
            self.upload("/bounded.bin", payload)
            with self.sftp.open("/bounded.bin", "rb") as source:
                self.assertEqual(source.read(), payload)
        self.assertGreater(maxima[paramiko.sftp.CMD_READ], 1)
        self.assertLessEqual(maxima[paramiko.sftp.CMD_READ], 32)
        self.assertGreater(maxima[paramiko.sftp.CMD_WRITE], 1)
        self.assertLessEqual(maxima[paramiko.sftp.CMD_WRITE], 8)
        self.assertEqual(new_threads, [])
        self.assertTrue(all(length <= 32768 for _, length in self.read_requests))
        self.assertTrue(all(length <= 32768 for _, length in self.write_requests))

    def test_out_of_order_successful_write_acknowledgements(self):
        payload = self.payload(256 * 1024)
        self.reverse_write_responses = True
        with self.sftp.open("/reverse.bin", "wx") as target:
            target.write(payload)
            target.flush()
            self.assertEqual(target.tell(), len(payload))
        self.assertEqual((self.root / "reverse.bin").read_bytes(), payload)

    def test_out_of_order_failed_write_acknowledgement_is_not_lost(self):
        self.reverse_write_responses = True
        self.fail_write_offset = 3 * 32768
        with self.sftp.open("/denied.bin", "wx") as target:
            with self.assertRaises(OSError) as caught:
                target.write(self.payload(256 * 1024))
                target.flush()
            self.assertEqual(caught.exception.errno, errno.EACCES)

    def test_read_failure_is_not_treated_as_eof(self):
        (self.root / "denied-read.bin").write_bytes(self.payload(1024 * 1024))
        self.fail_read_offset = 32768
        with self.sftp.open("/denied-read.bin", "rb") as source:
            with self.assertRaises(OSError) as caught:
                source.read(1024 * 1024)
            self.assertEqual(caught.exception.errno, errno.EACCES)

    def run_cancelled_io(self, direction):
        errors = []
        if direction == "read":
            (self.root / "blocked.bin").write_bytes(self.payload(2 * 1024 * 1024))
            self.block_reads = True
            handle = self.sftp.open("/blocked.bin", "rb")
        else:
            self.block_writes = True
            handle = self.sftp.open("/blocked.bin", "wx")

        def operation():
            try:
                if direction == "read":
                    handle.read(2 * 1024 * 1024)
                else:
                    handle.write(self.payload(256 * 1024))
                    handle.flush()
            except Exception as error:
                errors.append(error)

        worker = threading.Thread(target=operation, name="file-transfer-loopback-cancel-test", daemon=True)
        self.workers.append(worker)
        worker.start()
        try:
            self.assertTrue(self.io_observed.wait(2), "The test did not reach a blocked server operation")
            started = time.monotonic()
            self.controller.cancel()
            worker.join(2)
            elapsed = time.monotonic() - started
            self.assertFalse(worker.is_alive(), "Cancellation did not unblock client I/O")
            self.assertLess(elapsed, 2)
            self.assertEqual(len(errors), 1, "Interrupted I/O must report failure")
            self.assertTrue(self.controller.cancelled)
            self.assertFalse(any(
                getattr(getattr(thread, "_target", None), "__name__", "") == "_prefetch_thread"
                for thread in threading.enumerate()
            ))
        finally:
            self.io_release.set()
            try:
                handle.close()
            except (OSError, EOFError, paramiko.SSHException):
                pass

    def test_cancel_interrupts_blocked_read_without_prefetch_workers(self):
        self.run_cancelled_io("read")

    def test_cancel_interrupts_blocked_write_without_prefetch_workers(self):
        self.run_cancelled_io("write")


if __name__ == "__main__":
    unittest.main()
