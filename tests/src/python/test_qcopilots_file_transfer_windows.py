"""Native Windows file access and loopback SSH cancellation acceptance tests.

Remote file operations use the existing isolated SFTP fixture. File sharing,
read-only attributes, DACL checks, long paths and the loopback SSH socket are
real operating-system operations. Every fixture and access change is restored.
"""

from __future__ import annotations

import ctypes
import hashlib
import logging
import os
import socket
import stat
import sys
import tempfile
import threading
import time
import unittest
from contextlib import contextmanager
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python" / "plugins"))

from qcopilots_common.security_policy import FilesystemPolicy
from qcopilots_mcp_server_processing_general.file_transfer_backend import FileTransferBackend
from qcopilots_mcp_server_processing_general.file_transfer_cancellation import CancellationController, ConnectionCancelled
from qcopilots_mcp_server_processing_general.file_transfer_jobs import FileTransferJobManager
from test_qcopilots_mcp_server_file_transfer_jobs import FakeBackend, MemoryConfigStore


@contextmanager
def windows_file_handle(path, *, share=0):
    """Hold a real Win32 handle without delete sharing until the block exits."""
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    create = kernel.CreateFileW
    create.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
                       ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    create.restype = ctypes.c_void_p
    close = kernel.CloseHandle
    close.argtypes = [ctypes.c_void_p]
    close.restype = ctypes.c_int
    handle = create(str(path), 0x80000000, share, None, 3, 0x80, None)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        yield
    finally:
        if not close(handle):
            raise ctypes.WinError(ctypes.get_last_error())


@contextmanager
def deny_file_creation(directory):
    """Restore the original DACL using a WRITE_DAC handle opened beforehand."""
    try:
        import win32con
        import win32file
        import win32security
    except ImportError as error:
        raise unittest.SkipTest("pywin32 is unavailable for a recoverable temporary-directory DACL test") from error
    handle = win32file.CreateFile(
        str(directory), win32con.READ_CONTROL | win32con.WRITE_DAC,
        win32con.FILE_SHARE_READ | win32con.FILE_SHARE_WRITE | win32con.FILE_SHARE_DELETE,
        None, win32con.OPEN_EXISTING, win32con.FILE_FLAG_BACKUP_SEMANTICS, None,
    )
    applied = False
    try:
        original = win32security.GetSecurityInfo(handle, win32security.SE_FILE_OBJECT, win32security.DACL_SECURITY_INFORMATION)
        original_acl = original.GetSecurityDescriptorDacl()
        if original_acl is None:
            raise unittest.SkipTest("Temporary directory has a NULL DACL, leave it unchanged")
        acl = win32security.ACL()
        everyone = win32security.CreateWellKnownSid(win32security.WinWorldSid, None)
        acl.AddAccessDeniedAce(win32security.ACL_REVISION, 0x2, everyone)  # FILE_ADD_FILE
        for index in range(original_acl.GetAceCount()):
            ace = original_acl.GetAce(index)
            if len(ace) != 3 or ace[0][0] not in {0, 1}:
                raise unittest.SkipTest("Temporary DACL has nonstandard ACE types, leave it unchanged")
            (kind, flags), mask, sid = ace
            add = acl.AddAccessAllowedAceEx if kind == 0 else acl.AddAccessDeniedAceEx
            add(win32security.ACL_REVISION, flags, mask, sid)
        control, _revision = original.GetSecurityDescriptorControl()
        restore_flags = win32security.DACL_SECURITY_INFORMATION | (
            0x80000000 if control & 0x1000 else 0x20000000
        )
        win32security.SetSecurityInfo(handle, win32security.SE_FILE_OBJECT,
                                     ctypes.c_long(win32security.DACL_SECURITY_INFORMATION | 0x80000000).value,
                                     None, None, acl, None)
        applied = True
        yield
    finally:
        try:
            if applied:
                win32security.SetSecurityInfo(handle, win32security.SE_FILE_OBJECT,
                                             ctypes.c_long(restore_flags).value, None, None, original_acl, None)
        finally:
            handle.Close()


@unittest.skipUnless(os.name == "nt", "Native Windows filesystem tests")
class TestFileTransferWindowsFilesystem(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qcopilots-file-transfer-win-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.local = self.root / "local"
        self.local.mkdir()
        self.store = MemoryConfigStore(self.root / "config")
        self.backend = FakeBackend(self.root / "remote")
        self.backend.local("/uploads").mkdir()
        self.manager = FileTransferJobManager(self.store, self.backend, FilesystemPolicy())
        self.addCleanup(self.manager.shutdown, 5)
        self.payload = bytes(range(256)) * 64

    def wait(self, started):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            snapshot = self.manager.get_job(started["job_id"])
            if snapshot["state"] in {"succeeded", "failed", "cancelled"}:
                return snapshot
            time.sleep(0.01)
        self.fail("Local Windows transfer did not finish within 10 seconds")

    def remote_source(self):
        self.backend.local("/uploads/result.bin").write_bytes(self.payload)
        return "/uploads/result.bin"

    def assert_denied(self, snapshot):
        self.assertEqual(snapshot["state"], "failed", snapshot)
        self.assertTrue(any(error["code"] == "permission_denied" for error in snapshot["errors"]), snapshot)

    def discard(self, snapshot):
        result = self.manager.discard_transfer(snapshot["transfer_id"])
        self.assertTrue(result["cleanup"]["complete"], result)
        self.assertFalse(result["cleanup_pending"], result)

    def test_real_exclusive_source_lock_denies_upload_then_resume_succeeds(self):
        source = self.local / "locked.bin"
        source.write_bytes(self.payload)
        with windows_file_handle(source):
            with self.assertRaises(PermissionError):
                source.read_bytes()
            failed = self.wait(self.manager.start("upload", [str(source)], "/uploads"))
            self.assert_denied(failed)
            self.assertFalse(self.backend.local("/uploads/locked.bin").exists())
        finished = self.wait(self.manager.resume_job(failed["job_id"]))
        self.assertEqual(finished["state"], "succeeded", finished)
        self.assertEqual(self.backend.local("/uploads/locked.bin").read_bytes(), self.payload)

    def test_real_destination_delete_sharing_lock_preserves_original(self):
        remote = self.remote_source()
        destination = self.local / "result.bin"
        destination.write_bytes(b"original")
        with windows_file_handle(destination, share=1):  # FILE_SHARE_READ, no FILE_SHARE_DELETE
            failed = self.wait(self.manager.start("download", [remote], str(self.local), overwrite=True))
            self.assert_denied(failed)
            self.assertEqual(destination.read_bytes(), b"original")
        self.discard(failed)
        self.assertEqual(destination.read_bytes(), b"original")

    def test_real_readonly_target_rejects_atomic_replacement(self):
        remote = self.remote_source()
        destination = self.local / "result.bin"
        destination.write_bytes(b"original-readonly")
        os.chmod(destination, stat.S_IREAD)
        try:
            self.assertTrue(destination.stat().st_file_attributes & stat.FILE_ATTRIBUTE_READONLY)
            failed = self.wait(self.manager.start("download", [remote], str(self.local), overwrite=True))
            self.assert_denied(failed)
            self.assertEqual(destination.read_bytes(), b"original-readonly")
        finally:
            os.chmod(destination, stat.S_IREAD | stat.S_IWRITE)
        self.discard(failed)

    def test_real_directory_dacl_denies_creation_and_is_restored(self):
        remote = self.remote_source()
        destination = self.local / "acl-destination"
        destination.mkdir()
        with deny_file_creation(destination):
            with self.assertRaises(PermissionError):
                (destination / "access-probe.bin").write_bytes(b"denied")
            failed = self.wait(self.manager.start("download", [remote], str(destination)))
            self.assert_denied(failed)
            self.assertFalse((destination / "result.bin").exists())
        probe = destination / "restored-access.bin"
        probe.write_bytes(b"restored")
        self.assertEqual(probe.read_bytes(), b"restored")
        probe.unlink()
        self.discard(failed)

    def test_ordinary_paths_beyond_max_path_roundtrip_when_windows_supports_them(self):
        source = self.local.joinpath(*[(f"level-{index}-" + "x" * 40) for index in range(6)], "result.bin")
        self.assertGreater(len(str(source)), 260)
        try:
            source.parent.mkdir(parents=True)
            source.write_bytes(self.payload)
        except OSError as error:
            if getattr(error, "winerror", None) == 206:
                self.skipTest("Windows/Python rejects ordinary paths beyond MAX_PATH, system policy was not changed")
            raise
        uploaded = self.wait(self.manager.start("upload", [str(source)], "/uploads"))
        self.assertEqual(uploaded["state"], "succeeded", uploaded)
        destination = self.local.joinpath(*[(f"download-{index}-" + "y" * 40) for index in range(6)])
        self.assertGreater(len(str(destination)), 260)
        downloaded = self.wait(self.manager.start("download", ["/uploads/result.bin"], str(destination)))
        self.assertEqual(downloaded["state"], "succeeded", downloaded)
        self.assertEqual(hashlib.sha256((destination / "result.bin").read_bytes()).digest(), hashlib.sha256(self.payload).digest())


@unittest.skipUnless(os.name == "nt", "Native Windows loopback socket test")
class TestFileTransferWindowsLoopback(unittest.TestCase):
    def test_real_numeric_ip_ssh_handshake_cancellation_finishes_within_two_seconds(self):
        try:
            import paramiko  # noqa: F401 - require the real locked transport
        except (ImportError, OSError) as error:
            self.skipTest("Paramiko unavailable, rerun this test in the locked file transfer uv environment")
        with tempfile.TemporaryDirectory(prefix="qcopilots-file-transfer-loopback-") as temporary:
            root = Path(temporary)
            listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.addCleanup(listener.close)
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            listener.settimeout(5)
            observed, stop = threading.Event(), threading.Event()
            results, server_errors = [], []
            control = CancellationController()
            logger = logging.getLogger("paramiko.transport")
            old_disabled, logger.disabled = logger.disabled, True
            self.addCleanup(setattr, logger, "disabled", old_disabled)
            def silent_server():
                try:
                    connection, _address = listener.accept()
                    with connection:
                        connection.settimeout(5)
                        banner = connection.recv(4096)
                        if not banner.startswith(b"SSH-"):
                            raise AssertionError("Client did not start a real SSH handshake")
                        observed.set()
                        stop.wait(5)
                except Exception as error:
                    if not stop.is_set():
                        server_errors.append(error)
            config = {"host": "127.0.0.1", "port": listener.getsockname()[1], "username": "loopback-only",
                      "password": "never-authenticated", "connect_timeout": 30, "io_timeout": 30,
                      "known_hosts_file": str(root / "known_hosts")}
            backend = FileTransferBackend(SimpleNamespace(root=root), FilesystemPolicy())
            def connect():
                try:
                    results.append(backend.connect(config, cancel_control=control))
                except Exception as error:
                    results.append(error)
            server_thread, client_thread = threading.Thread(target=silent_server), threading.Thread(target=connect)
            server_thread.start()
            client_thread.start()
            try:
                self.assertTrue(observed.wait(5), server_errors)
                before = time.monotonic()
                control.cancel()
                client_thread.join(2)
                elapsed = time.monotonic() - before
                self.assertFalse(client_thread.is_alive())
                self.assertLess(elapsed, 2)
                self.assertEqual(len(results), 1)
                self.assertIsInstance(results[0], ConnectionCancelled)
                self.assertEqual(server_errors, [])
            finally:
                control.cancel()
                stop.set()
                listener.close()
                client_thread.join(5)
                server_thread.join(5)
                logger.disabled = old_disabled
                self.assertFalse(client_thread.is_alive())
                self.assertFalse(server_thread.is_alive())


if __name__ == "__main__":
    unittest.main()
