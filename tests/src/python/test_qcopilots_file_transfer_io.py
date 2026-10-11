"""Bounded SFTP protocol I/O tests using the locked Paramiko message format.

The protocol fake runs entirely in the calling thread and never opens a socket.
Its data buffers are in memory, including sparse offsets beyond four GiB.
"""

from __future__ import annotations

import errno
import io
import sys
import threading
import unittest
import weakref
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python" / "plugins"))

try:
    import paramiko
    from paramiko.sftp import (
        CMD_DATA, CMD_READ, CMD_STATUS, CMD_WRITE, SFTP_EOF, SFTP_FAILURE,
        SFTP_OK, SFTP_PERMISSION_DENIED, int64,
    )
except ImportError:
    paramiko = None

from qcopilots_mcp_server_processing_general import file_transfer_io
from qcopilots_mcp_server_processing_general.file_transfer_cancellation import (
    CancellationController, ConnectionCancelled,
)
from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError


class FakeChannel:
    def __init__(self):
        self.closed = False
        self.closes = 0

    def close(self):
        self.closed = True
        self.closes += 1


class FakeSftp:
    """Dispatch request owners exactly as Paramiko's non-waiting response path."""

    def __init__(self, data=b"", *, reverse=False, base=0):
        self.data = bytearray(data)
        self.base = base
        self.reverse = reverse
        self.request_number = 1
        self._expecting = weakref.WeakValueDictionary()
        self.pending = []
        self.requests = []
        self.responses = []
        self.thread_ids = []
        self.peak_pending = 0
        self.peak_bytes = 0
        self.short_reads = {}
        self.data_overrides = {}
        self.read_errors = {}
        self.write_errors = {}
        self.after_request = None
        self.before_response = None
        self.network_error = None
        self.response_type = None
        self.channel = FakeChannel()
        self.open_calls = []
        self.closed = False

    def _async_request(self, owner, command, *args):
        request_id = self.request_number
        self.request_number += 1
        self._enqueue(request_id, owner, command, args)
        return request_id

    def _enqueue(self, request_id, owner, command, args):
        if self.channel.closed:
            raise OSError("SFTP channel is closed")
        if command not in (CMD_READ, CMD_WRITE) or len(args) != 3:
            raise AssertionError("Unexpected file I/O request")
        handle, offset, value = args
        if handle != b"fake-handle" or not isinstance(offset, int64):
            raise AssertionError("SFTP file offsets must be encoded as uint64")
        offset = int(offset)
        if command == CMD_WRITE:
            value = bytes(value)
        elif isinstance(value, bool) or not isinstance(value, int) or value <= 0:
            raise AssertionError("READ length must be a positive integer")
        self._expecting[request_id] = owner
        request = (request_id, command, offset, value)
        self.pending.append(request)
        self.requests.append(request)
        self.thread_ids.append(threading.get_ident())
        self.peak_pending = max(self.peak_pending, len(self.pending))
        self.peak_bytes = max(self.peak_bytes, sum(
            item[3] if item[1] == CMD_READ else len(item[3]) for item in self.pending
        ))
        if command == CMD_WRITE and offset not in self.write_errors:
            relative = offset - self.base
            if relative < 0:
                raise AssertionError("Write precedes the fake sparse region")
            if relative + len(value) > len(self.data):
                self.data.extend(b"\0" * (relative + len(value) - len(self.data)))
            self.data[relative:relative + len(value)] = value
        if self.after_request is not None:
            self.after_request(self, request)

    @staticmethod
    def status(code):
        message = paramiko.Message()
        message.add_int(code)
        message.add_string("injected status")
        message.add_string("en")
        return paramiko.Message(message.asbytes())

    @staticmethod
    def content(value):
        message = paramiko.Message()
        message.add_string(value)
        return paramiko.Message(message.asbytes())

    def _read_response(self, waitfor=None):
        if waitfor is not None:
            raise AssertionError("The adapter must dispatch through the request owner")
        request_id, response, message = self._response()
        owner = self._expecting.pop(request_id)
        owner._async_response(response, message, request_id)
        return None, None

    def _response(self):
        if self.before_response is not None:
            self.before_response(self)
        if self.network_error is not None:
            raise self.network_error
        if self.channel.closed:
            raise OSError("SFTP channel is closed")
        if not self.pending:
            raise AssertionError("Read requested without an outstanding SFTP request")
        request_id, command, offset, value = self.pending.pop(-1 if self.reverse else 0)
        self.responses.append(request_id)
        self.thread_ids.append(threading.get_ident())
        if command == CMD_WRITE:
            response = CMD_STATUS
            message = self.status(self.write_errors.get(offset, SFTP_OK))
        elif offset in self.read_errors:
            response, message = CMD_STATUS, self.status(self.read_errors[offset])
        else:
            relative = offset - self.base
            if relative < 0:
                raise AssertionError("Read precedes the fake sparse region")
            length = self.short_reads.pop(offset, value)
            content = self.data_overrides.get(offset, bytes(self.data[relative:relative + min(length, value)]))
            if content or offset in self.data_overrides:
                response, message = CMD_DATA, self.content(content)
            else:
                response, message = CMD_STATUS, self.status(SFTP_EOF)
        return request_id, self.response_type or response, message

    def _convert_status(self, message):
        return paramiko.SFTPClient._convert_status(self, message)

    def get_channel(self):
        return self.channel

    def close(self):
        self.closed = True
        self.channel.close()

    def open(self, path, mode="r", bufsize=-1):
        self.open_calls.append((path, mode, bufsize))
        return FakeFile(self)

    file = open

    def stat(self, path):
        return SimpleNamespace(path=path, st_size=self.base + len(self.data))


class WireSftp(FakeSftp):
    """Exercise Paramiko's real dispatcher with in-memory SFTP wire packets."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._lock = threading.Lock()
        self.ultra_debug = False

    def _async_request(self, owner, command, *args):
        return paramiko.SFTPClient._async_request(self, owner, command, *args)

    def _read_response(self, waitfor=None):
        return paramiko.SFTPClient._read_response(self, waitfor)

    def _send_packet(self, command, message):
        packet = paramiko.Message(message.asbytes())
        request_id = packet.get_int()
        handle = packet.get_string()
        offset = packet.get_int64()
        value = packet.get_int() if command == CMD_READ else packet.get_string()
        if packet.get_remainder():
            raise AssertionError("Unexpected trailing SFTP request bytes")
        self._enqueue(request_id, self._expecting[request_id], command,
                      (handle, int64(offset), value))

    def _read_packet(self):
        request_id, response, message = self._response()
        packet = paramiko.Message()
        packet.add_int(request_id)
        packet.add_bytes(message.asbytes())
        return response, packet.asbytes()

    def _log(self, *_args):
        pass


class FakeFile:
    def __init__(self, sftp):
        self.sftp = sftp
        self.handle = b"fake-handle"
        self.position = 0
        self._pos = 0
        self._realpos = 0
        self.closed = False
        self.closes = 0
        self.flushes = 0
        self.flush_error = None
        self.seeks = []

    def tell(self):
        return self.position

    def seek(self, offset, whence=0):
        if whence == 0:
            position = offset
        elif whence == 1:
            position = self.position + offset
        elif whence == 2:
            position = self.sftp.base + len(self.sftp.data) + offset
        else:
            raise ValueError("Invalid seek origin")
        if position < 0:
            raise OSError(errno.EINVAL, "Negative seek offset")
        self.position = self._pos = self._realpos = position
        self.seeks.append((offset, whence))

    def read(self, _size=-1):
        raise AssertionError("Adapter must not use the synchronous raw READ path")

    def write(self, _value):
        raise AssertionError("Adapter must not use the synchronous raw WRITE path")

    def flush(self):
        self.flushes += 1
        if self.flush_error is not None:
            raise self.flush_error

    def close(self):
        self.closed = True
        self.closes += 1

    def stat(self):
        return SimpleNamespace(st_size=self.sftp.base + len(self.sftp.data))


@unittest.skipUnless(paramiko is not None, "The locked Paramiko dependency is unavailable")
class TestFileTransferSftpIo(unittest.TestCase):
    def make_file(self, data=b"", *, reverse=False, base=0, control=None, wire=False):
        protocol = (WireSftp if wire else FakeSftp)(data, reverse=reverse, base=base)
        raw = FakeFile(protocol)
        adapter = file_transfer_io.SftpFileAdapter(raw, cancel_control=control)
        self.addCleanup(adapter.close)
        return protocol, raw, adapter

    @staticmethod
    def payload(size):
        return (bytes(range(251)) * ((size + 250) // 251))[:size]

    def assert_failed(self, protocol, adapter):
        self.assertTrue(protocol.closed or protocol.channel.closed)
        issued = len(protocol.requests)
        for operation in (lambda: adapter.read(1), lambda: adapter.write(b"x"),
                          lambda: adapter.seek(0), adapter.flush):
            with self.assertRaises(OSError):
                operation()
        self.assertEqual(len(protocol.requests), issued)
        adapter.close()
        adapter.close()

    def test_empty_read_write_do_not_issue_protocol_requests(self):
        protocol, _raw, adapter = self.make_file(b"unchanged")
        self.assertEqual(adapter.read(0), b"")
        adapter.write(b"")
        self.assertEqual(adapter.tell(), 0)
        self.assertEqual(protocol.requests, [])

    def test_write_rejects_non_buffers_without_creating_zero_filled_data(self):
        protocol, _raw, adapter = self.make_file()
        for value in (3, True, None, 1.5, [1, 2, 3]):
            with self.subTest(value=value):
                with self.assertRaises(TypeError):
                    adapter.write(value)
        self.assertEqual(protocol.requests, [])
        self.assertEqual(bytes(protocol.data), b"")
        self.assertEqual(adapter.tell(), 0)

    def test_read_rejects_non_integer_size_without_protocol_activity(self):
        protocol, _raw, adapter = self.make_file(b"content")
        for value in (1.5, "3", object()):
            with self.subTest(value=type(value).__name__):
                with self.assertRaises(TypeError):
                    adapter.read(value)
        self.assertEqual(protocol.requests, [])

    def test_text_write_uses_utf8_byte_offsets(self):
        text = "测绘成果 🗺"
        protocol, _raw, adapter = self.make_file()
        self.assertIsNone(adapter.write(text))
        self.assertEqual(bytes(protocol.data), text.encode("utf-8"))
        self.assertEqual(adapter.tell(), len(text.encode("utf-8")))

    def test_write_waits_for_all_out_of_order_acknowledgements(self):
        data = self.payload(256 * 1024)
        protocol, _raw, adapter = self.make_file(reverse=True)
        adapter.write(data)
        self.assertEqual(bytes(protocol.data), data)
        self.assertEqual(adapter.tell(), len(data))
        self.assertEqual(protocol.pending, [])
        self.assertEqual(dict(protocol._expecting), {})
        self.assertEqual(len(protocol.responses), 8)
        self.assertNotEqual(protocol.responses, sorted(protocol.responses))
        self.assertLessEqual(protocol.peak_pending, 8)
        self.assertLessEqual(protocol.peak_bytes, 256 * 1024)

    def test_write_error_on_early_request_is_not_lost_to_later_successes(self):
        protocol, _raw, adapter = self.make_file(reverse=True)
        protocol.write_errors[0] = SFTP_PERMISSION_DENIED
        with self.assertRaises(OSError):
            adapter.write(self.payload(256 * 1024))
        self.assert_failed(protocol, adapter)

    def test_write_error_on_late_request_fails_before_issuing_next_batch(self):
        protocol, _raw, adapter = self.make_file()
        protocol.write_errors[7 * 32768] = SFTP_FAILURE
        with self.assertRaises(OSError):
            adapter.write(self.payload(768 * 1024))
        self.assertEqual(len(protocol.requests), 8)
        self.assert_failed(protocol, adapter)

    def test_large_write_respects_chunk_and_window_boundaries(self):
        data = self.payload(3 * 256 * 1024 + 17)
        protocol, _raw, adapter = self.make_file(reverse=True)
        adapter.write(memoryview(data))
        self.assertEqual(bytes(protocol.data), data)
        self.assertEqual(adapter.tell(), len(data))
        self.assertLessEqual(protocol.peak_pending, 8)
        self.assertLessEqual(protocol.peak_bytes, 256 * 1024)
        self.assertEqual([len(item[3]) for item in protocol.requests], [32768] * 24 + [17])
        self.assertEqual(protocol.pending, [])

    def test_read_reassembles_out_of_order_data(self):
        data = self.payload(1024 * 1024)
        protocol, _raw, adapter = self.make_file(data, reverse=True)
        self.assertEqual(adapter.read(len(data)), data)
        self.assertEqual(adapter.tell(), len(data))
        self.assertEqual(protocol.pending, [])
        self.assertLessEqual(protocol.peak_pending, 32)
        self.assertLessEqual(protocol.peak_bytes, 1024 * 1024)
        self.assertNotEqual(protocol.responses, sorted(protocol.responses))

    def test_large_read_uses_bounded_windows_and_preserves_final_tail(self):
        data = self.payload(3 * 1024 * 1024 + 53)
        protocol, _raw, adapter = self.make_file(data, reverse=True)
        result = bytearray()
        while len(result) < len(data):
            piece = adapter.read(len(data) - len(result))
            self.assertTrue(piece)
            result.extend(piece)
        self.assertEqual(bytes(result), data)
        self.assertEqual(adapter.read(1), b"")
        self.assertLessEqual(protocol.peak_pending, 32)
        self.assertLessEqual(protocol.peak_bytes, 1024 * 1024)
        self.assertTrue(all(0 < item[3] <= 32768 for item in protocol.requests))
        self.assertEqual(protocol.pending, [])

    def test_short_read_before_later_chunks_preserves_contiguous_bytes(self):
        data = self.payload(3 * 32768 + 7)
        protocol, _raw, adapter = self.make_file(data, reverse=True)
        protocol.short_reads[0] = 123
        result = bytearray()
        while len(result) < len(data):
            piece = adapter.read(len(data) - len(result))
            self.assertTrue(piece)
            result.extend(piece)
        self.assertEqual(bytes(result), data)
        self.assertEqual(adapter.tell(), len(data))
        self.assertEqual(protocol.pending, [])

    def test_eof_status_and_final_short_chunk_are_normal(self):
        data = self.payload(32768 + 29)
        protocol, _raw, adapter = self.make_file(data, reverse=True)
        self.assertEqual(adapter.read(4 * 32768), data)
        self.assertEqual(adapter.tell(), len(data))
        self.assertEqual(adapter.read(32768), b"")
        self.assertFalse(protocol.closed or protocol.channel.closed)

    def test_readall_uses_bounded_requests_until_eof(self):
        data = self.payload(2 * 1024 * 1024 + 11)
        protocol, _raw, adapter = self.make_file(data, reverse=True)
        self.assertEqual(adapter.read(), data)
        self.assertEqual(adapter.tell(), len(data))
        self.assertLessEqual(protocol.peak_pending, 32)
        self.assertLessEqual(protocol.peak_bytes, 1024 * 1024)

    def test_readall_continues_after_non_eof_short_read(self):
        data = self.payload(2 * 32768 + 11)
        protocol, _raw, adapter = self.make_file(data, reverse=True)
        protocol.short_reads[0] = 13
        self.assertEqual(adapter.read(), data)
        self.assertEqual(adapter.tell(), len(data))

    def test_real_paramiko_wire_dispatch_preserves_offsets_and_response_owners(self):
        base = (1 << 32) + 91
        data = self.payload(256 * 1024 + 7)
        protocol, _raw, adapter = self.make_file(base=base, reverse=True, wire=True)
        adapter.seek(base)
        adapter.write(data)
        adapter.seek(base)
        self.assertEqual(adapter.read(len(data)), data)
        self.assertEqual(adapter.tell(), base + len(data))
        self.assertTrue(all(request[2] >= base for request in protocol.requests))
        self.assertEqual(dict(protocol._expecting), {})
        self.assertEqual(protocol.pending, [])

    def test_real_paramiko_wire_dispatch_does_not_discard_failed_write_status(self):
        protocol, _raw, adapter = self.make_file(reverse=True, wire=True)
        protocol.write_errors[0] = SFTP_PERMISSION_DENIED
        with self.assertRaises(OSError) as raised:
            adapter.write(self.payload(256 * 1024))
        self.assertEqual(raised.exception.errno, errno.EACCES)
        self.assert_failed(protocol, adapter)

    def test_read_error_is_not_reported_as_eof_or_partial_success(self):
        protocol, _raw, adapter = self.make_file(self.payload(4 * 32768), reverse=True)
        protocol.read_errors[32768] = SFTP_PERMISSION_DENIED
        with self.assertRaises(OSError):
            adapter.read(4 * 32768)
        self.assert_failed(protocol, adapter)

    def test_network_failure_poisoned_handle_cannot_continue(self):
        protocol, _raw, adapter = self.make_file(self.payload(32768))
        protocol.network_error = EOFError("Injected closed SSH transport")
        with self.assertRaises((OSError, EOFError)):
            adapter.read(32768)
        self.assert_failed(protocol, adapter)

    def test_send_failure_after_registration_does_not_advance_file_position(self):
        protocol, raw, adapter = self.make_file(wire=True)

        def lose_response(_sftp, _request):
            raise OSError("Injected send failure after remote receipt")

        protocol.after_request = lose_response
        with self.assertRaises(OSError):
            adapter.write(b"accepted remotely")
        self.assertEqual(bytes(protocol.data), b"accepted remotely")
        self.assertEqual(raw.tell(), 0)
        self.assertEqual(len(protocol.requests), 1)
        self.assert_failed(protocol, adapter)

    def test_oversized_data_response_is_rejected(self):
        protocol, _raw, adapter = self.make_file(b"abc")
        protocol.data_overrides[0] = b"too many bytes"
        with self.assertRaises(FileTransferError) as raised:
            adapter.read(3)
        self.assertEqual(raised.exception.code, "sftp_protocol_error")
        self.assert_failed(protocol, adapter)

    def test_read_success_status_without_data_is_rejected(self):
        protocol, _raw, adapter = self.make_file(b"abc")
        protocol.read_errors[0] = SFTP_OK
        with self.assertRaises(FileTransferError) as raised:
            adapter.read(3)
        self.assertEqual(raised.exception.code, "sftp_protocol_error")
        self.assert_failed(protocol, adapter)

    def test_write_eof_status_is_failure(self):
        protocol, _raw, adapter = self.make_file()
        protocol.write_errors[0] = SFTP_EOF
        with self.assertRaises((EOFError, OSError)):
            adapter.write(b"data")
        self.assert_failed(protocol, adapter)

    def test_empty_data_response_does_not_spin_or_advance_position(self):
        protocol, _raw, adapter = self.make_file(b"abc")
        protocol.data_overrides[0] = b""
        self.assertEqual(adapter.read(), b"")
        self.assertEqual(adapter.tell(), 0)
        self.assertEqual(len(protocol.requests), 32)
        self.assertEqual(protocol.pending, [])

    def test_cancel_before_first_request_performs_no_io(self):
        control = CancellationController()
        protocol, _raw, adapter = self.make_file(b"data", control=control)
        control.cancel()
        with self.assertRaises((ConnectionCancelled, OSError)):
            adapter.read(4)
        self.assertEqual(protocol.requests, [])

    def test_cancel_during_write_window_stops_new_requests(self):
        control = CancellationController()
        protocol, _raw, adapter = self.make_file(control=control)
        protocol.after_request = lambda _sftp, _request: control.cancel()
        with self.assertRaises((ConnectionCancelled, OSError)):
            adapter.write(self.payload(512 * 1024))
        self.assertLessEqual(len(protocol.requests), 1)
        self.assert_failed(protocol, adapter)

    def test_cancel_while_reading_responses_stops_before_next_window(self):
        control = CancellationController()
        protocol, _raw, adapter = self.make_file(self.payload(2 * 1024 * 1024), control=control)
        protocol.before_response = lambda _sftp: control.cancel()
        with self.assertRaises((ConnectionCancelled, OSError)):
            adapter.read(2 * 1024 * 1024)
        self.assertLessEqual(len(protocol.requests), 32)
        self.assert_failed(protocol, adapter)

    def test_seek_tell_cover_absolute_relative_and_end_offsets(self):
        protocol, raw, adapter = self.make_file(b"0123456789")
        adapter.seek(2)
        self.assertEqual(adapter.tell(), 2)
        self.assertEqual(adapter.read(2), b"23")
        adapter.seek(1, io.SEEK_CUR)
        self.assertEqual(adapter.tell(), 5)
        adapter.write(b"AB")
        adapter.seek(-2, io.SEEK_END)
        self.assertEqual(adapter.read(2), b"89")
        self.assertEqual(bytes(protocol.data), b"01234AB789")
        self.assertTrue(raw.seeks)
        self.assertEqual(protocol.pending, [])

    def test_offsets_above_four_gib_use_uint64_wire_encoding(self):
        base = (1 << 32) + 123
        protocol, _raw, adapter = self.make_file(b"abc", base=base)
        adapter.seek(base)
        self.assertEqual(adapter.read(3), b"abc")
        adapter.write(b"DEF")
        self.assertEqual(adapter.tell(), base + 6)
        self.assertEqual(bytes(protocol.data), b"abcDEF")
        self.assertEqual([item[2] for item in protocol.requests], [base, base + 3])

    def test_all_protocol_activity_stays_on_calling_thread(self):
        protocol, _raw, adapter = self.make_file()
        thread_id = threading.get_ident()
        with mock.patch.object(threading.Thread, "start", side_effect=AssertionError("Background thread created")):
            adapter.write(self.payload(512 * 1024))
            adapter.seek(0)
            self.assertEqual(adapter.read(512 * 1024), self.payload(512 * 1024))
        self.assertEqual(set(protocol.thread_ids), {thread_id})

    def test_close_is_idempotent_and_context_manager_closes_raw_file(self):
        _protocol, raw, adapter = self.make_file()
        with adapter as opened:
            self.assertIs(opened, adapter)
            adapter.write(b"value")
            adapter.flush()
        adapter.close()
        self.assertTrue(raw.closed)
        self.assertEqual(raw.closes, 1)

    def test_close_failure_is_reported_once_without_repeating_cleanup(self):
        _protocol, raw, adapter = self.make_file()
        raw.close = mock.Mock(side_effect=OSError("Injected file close failure"))
        with self.assertRaises(OSError):
            adapter.close()
        adapter.close()
        raw.close.assert_called_once_with()

    def test_context_cleanup_does_not_mask_original_operation_error(self):
        _protocol, raw, adapter = self.make_file()
        raw.close = mock.Mock(side_effect=OSError("Injected file close failure"))
        with self.assertRaisesRegex(ValueError, "original operation"):
            with adapter:
                raise ValueError("original operation")
        raw.close.assert_called_once_with()

    def test_client_wraps_files_and_delegates_metadata(self):
        protocol = FakeSftp(b"abc")
        client = file_transfer_io.SftpClientAdapter(protocol)
        self.addCleanup(client.close)
        with client.open("/target.bin", "rb", bufsize=0) as handle:
            self.assertIsInstance(handle, file_transfer_io.SftpFileAdapter)
            self.assertEqual(handle.read(3), b"abc")
        self.assertEqual(protocol.open_calls, [("/target.bin", "rb", 0)])
        self.assertEqual(client.stat("/target.bin").st_size, 3)

    def test_client_propagates_cancellation_to_opened_file(self):
        control = CancellationController()
        protocol = FakeSftp(b"abc")
        client = file_transfer_io.SftpClientAdapter(protocol, cancel_control=control)
        self.addCleanup(client.close)
        with client.open("/target.bin", "rb") as handle:
            control.cancel()
            with self.assertRaises((ConnectionCancelled, OSError)):
                handle.read(3)
        self.assertEqual(protocol.requests, [])

    def test_unsupported_paramiko_version_fails_before_protocol_io(self):
        protocol = FakeSftp(b"abc")
        with mock.patch.object(paramiko, "__version__", "5.1.0"):
            with self.assertRaises(FileTransferError) as error:
                adapter = file_transfer_io.SftpFileAdapter(FakeFile(protocol))
                try:
                    adapter.read(1)
                finally:
                    adapter.close()
        self.assertIn(error.exception.code, {"dependency_unavailable", "sftp_backend_incompatible"})
        self.assertEqual(protocol.requests, [])

    def test_client_checks_version_before_open_can_create_or_truncate(self):
        protocol = FakeSftp(b"original")
        client = file_transfer_io.SftpClientAdapter(protocol)
        self.addCleanup(client.close)
        with mock.patch.object(paramiko, "__version__", "5.1.0"):
            with self.assertRaises(FileTransferError) as raised:
                client.open("/protected.bin", "wb")
        self.assertEqual(raised.exception.code, "sftp_backend_incompatible")
        self.assertEqual(protocol.open_calls, [])
        self.assertEqual(bytes(protocol.data), b"original")

    def test_missing_private_dispatcher_is_a_controlled_incompatibility(self):
        protocol = FakeSftp()
        protocol._async_request = None
        with self.assertRaises(FileTransferError) as raised:
            file_transfer_io.SftpFileAdapter(FakeFile(protocol))
        self.assertEqual(raised.exception.code, "sftp_backend_incompatible")

    def test_missing_paramiko_is_a_controlled_dependency_error(self):
        with mock.patch.object(file_transfer_io.importlib, "import_module", side_effect=ImportError("injected missing dependency")):
            with self.assertRaises(FileTransferError) as raised:
                file_transfer_io.SftpFileAdapter(FakeFile(FakeSftp()))
        self.assertEqual(raised.exception.code, "dependency_unavailable")


if __name__ == "__main__":
    unittest.main()
