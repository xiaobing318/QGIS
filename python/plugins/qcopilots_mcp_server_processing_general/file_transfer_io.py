"""Bounded, acknowledged SFTP I/O for the service's pinned Paramiko runtime.

This adapter intentionally depends on Paramiko 5.0.0's private request dispatcher.
Its ordinary SFTPFile writes wait after each 32 KiB request. Its optional write
pipeline associates responses with ``type(None)``, so unrelated synchronous
operations can discard a delayed WRITE failure. Its prefetch worker also cannot
be cooperatively stopped while waiting for a concurrency slot.

Instead, one owning transfer worker sends a bounded vector of requests and reads
every response through a dedicated owner. There are no prefetch threads. A write
returns only after every STATUS succeeds, preserving the job manager's existing
acknowledged-offset and checkpoint ordering. Updating Paramiko requires explicit
review of these contracts and the protocol/loopback tests before changing the
version guard. No installed dependency is patched.
"""

from __future__ import annotations

import importlib
import operator
import threading

from .file_transfer_errors import FileTransferError


PARAMIKO_VERSION = "5.0.0"
REQUEST_SIZE = 32 * 1024
READ_WINDOW = 1024 * 1024
WRITE_WINDOW = 256 * 1024


def _api():
    try:
        paramiko = importlib.import_module("paramiko")
        protocol = importlib.import_module("paramiko.sftp")
    except (ImportError, OSError):
        raise FileTransferError("dependency_unavailable", "The locked SSH transfer dependency is unavailable") from None
    if getattr(paramiko, "__version__", None) != PARAMIKO_VERSION:
        raise FileTransferError("sftp_backend_incompatible", "The bounded SFTP adapter requires the reviewed Paramiko 5.0.0 runtime")
    return protocol


class _ResponseBatch:
    """Response ownership also preserves out-of-order successes and failures."""

    def __init__(self, sftp, protocol, reading):
        self.sftp = sftp
        self.protocol = protocol
        self.reading = reading
        self.pending = {}
        self.results = {}
        self.error = None

    def _async_response(self, message_type, message, request_id):
        expected = self.pending.pop(request_id, None)
        if expected is None:
            self.error = FileTransferError("sftp_protocol_error", "The server returned an unexpected transfer response")
            return
        offset, length = expected
        try:
            if self.reading and message_type == self.protocol.CMD_DATA:
                value = message.get_string()
                if len(value) > length:
                    raise FileTransferError("sftp_protocol_error", "The server returned more file data than requested")
                self.results[offset] = value
            elif message_type == self.protocol.CMD_STATUS:
                try:
                    self.sftp._convert_status(message)
                except EOFError:
                    if not self.reading:
                        raise
                    self.results[offset] = b""
                else:
                    if self.reading:
                        raise FileTransferError("sftp_protocol_error", "A file read returned a success status without data")
            else:
                raise FileTransferError("sftp_protocol_error", "The server returned an invalid file transfer response")
        except Exception as error:
            if self.error is None:
                self.error = error

    def _check_exception(self):
        if self.error is not None:
            raise self.error


class SftpFileAdapter:
    """Sequential file API with at most 32 reads or 8 writes in flight.

    The temporary network buffer is bounded by READ_WINDOW. As with Python's
    file.read, a caller requesting a larger value or read(-1) owns the resulting
    return buffer. Job workers request at most one MiB at a time.
    """

    def __init__(self, raw_file, cancel_control=None, *, _lock=None):
        self._protocol = _api()
        self._raw = raw_file
        self._sftp = raw_file.sftp
        if any(not callable(getattr(self._sftp, name, None)) for name in
               ("_async_request", "_read_response", "_convert_status")):
            raise FileTransferError("sftp_backend_incompatible", "The SSH request dispatcher does not match the reviewed interface")
        self._cancel_control = cancel_control
        self._lock = _lock if _lock is not None else threading.RLock()
        self._failed = False
        self._closed = False

    def __getattr__(self, name):
        return getattr(self._raw, name)

    def _check(self):
        if self._closed or self._failed:
            raise OSError("The SFTP file is closed or its transfer failed")
        if self._cancel_control is not None:
            self._cancel_control.check()

    def _abort(self):
        self._failed = True
        # Never let a later stat/seek/close consume responses from a failed
        # batch. The attempt ends and recovery opens a fresh SSH session.
        try:
            self._sftp.get_channel().close()
        except Exception:
            pass

    def _exchange(self, offset, length, data=None):
        reading = data is None
        batch = _ResponseBatch(self._sftp, self._protocol, reading)
        requests = []
        try:
            for start in range(0, length, REQUEST_SIZE):
                self._check()
                count = min(REQUEST_SIZE, length - start)
                position = offset + start
                request_id = self._sftp._async_request(
                    batch, self._protocol.CMD_READ if reading else self._protocol.CMD_WRITE,
                    self._raw.handle, self._protocol.int64(position),
                    count if reading else data[start:start + count],
                )
                if request_id in batch.pending:
                    raise FileTransferError("sftp_protocol_error", "The request dispatcher reused an outstanding request identifier")
                batch.pending[request_id] = (position, count)
                requests.append((position, count))
            while batch.pending:
                self._check()
                # No waitfor argument: all IDs dispatch to their own callback,
                # including errors arriving before an earlier successful ACK.
                self._sftp._read_response()
                batch._check_exception()
            self._check()
        except BaseException:
            self._abort()
            raise
        if not reading:
            return None
        values = []
        for position, count in requests:
            value = batch.results[position]
            values.append(value)
            if len(value) < count:
                # SFTP may return a short READ before EOF. Never concatenate
                # later offsets across its missing range. The next read starts
                # at this contiguous prefix and requests the remainder again.
                break
        return b"".join(values)

    def read(self, size=-1):
        if size is None:
            size = -1
        size = operator.index(size)
        with self._lock:
            self._check()
            if size == 0:
                return b""
            result = bytearray()
            while size < 0 or len(result) < size:
                count = READ_WINDOW if size < 0 else min(READ_WINDOW, size - len(result))
                position = self._raw.tell()
                value = self._exchange(position, count)
                self._raw.seek(position + len(value))
                result.extend(value)
                if not value:
                    break
            return bytes(result)

    def write(self, data):
        if isinstance(data, str):
            data = data.encode("utf-8")
        value = memoryview(data).tobytes()
        with self._lock:
            self._check()
            for start in range(0, len(value), WRITE_WINDOW):
                block = value[start:start + WRITE_WINDOW]
                position = self._raw.tell()
                self._exchange(position, len(block), block)
                # Every byte in this block has a successful server ACK before
                # the apparent file position or caller's checkpoint advances.
                self._raw.seek(position + len(block))
            return None

    def seek(self, offset, whence=0):
        with self._lock:
            self._check()
            return self._raw.seek(offset, whence)

    def tell(self):
        with self._lock:
            self._check()
            return self._raw.tell()

    def flush(self):
        with self._lock:
            self._check()
            return self._raw.flush()

    def close(self):
        if not self._closed:
            self._closed = True
            self._raw.close()

    @property
    def closed(self):
        return self._closed

    def __enter__(self):
        self._check()
        return self

    def __exit__(self, exc_type, _exc, _traceback):
        if exc_type is not None:
            try:
                self.close()
            except Exception:
                pass
        else:
            self.close()


class SftpClientAdapter:
    """Leave metadata operations intact and wrap only opened transfer files."""

    def __init__(self, raw_sftp, cancel_control=None):
        self._raw = raw_sftp
        self._cancel_control = cancel_control
        self._lock = threading.RLock()

    def __getattr__(self, name):
        return getattr(self._raw, name)

    def open(self, path, mode="r", bufsize=-1):
        _api()  # Check compatibility before an open can create or truncate data.
        if self._cancel_control is not None:
            self._cancel_control.check()
        raw_file = self._raw.open(path, mode, bufsize=bufsize)
        try:
            return SftpFileAdapter(raw_file, self._cancel_control, _lock=self._lock)
        except BaseException:
            raw_file.close()
            raise

    file = open
