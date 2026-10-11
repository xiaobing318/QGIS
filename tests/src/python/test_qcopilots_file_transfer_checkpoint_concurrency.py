"""Windows checkpoint readers, atomic replacement and bounded sharing retries.

All data is isolated in TemporaryDirectory. Transfers use fake SFTP and readers
may run in a separate process. No real server or credentials are accessed.
"""

from __future__ import annotations

import ctypes
import json
import os
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import uuid
from pathlib import Path
from unittest import mock


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))

from qcopilots_common.security_policy import FilesystemPolicy
from qcopilots_mcp_server_processing_general import file_transfer_storage
from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError
from qcopilots_mcp_server_processing_general.file_transfer_jobs import FileTransferJobManager, _TransferLease
from qcopilots_mcp_server_processing_general.file_transfer_storage import DiskTransferStore
from test_qcopilots_mcp_server_file_transfer_jobs import FakeBackend, MemoryConfigStore


class TestFileTransferCheckpointReplacement(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qcopilots-checkpoint-sharing-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.disk = DiskTransferStore(self.root / "checkpoints")
        self.record = {"schema_version": 2, "transfer_id": str(uuid.uuid4()),
                       "attempts": [], "revision": 1}
        self.path = self.disk.root / (self.record["transfer_id"] + ".json")
        self.disk.write_summary(self.record)

    @staticmethod
    def denied(winerror=5):
        error = PermissionError(13, "Injected checkpoint access refusal")
        error.winerror = winerror
        return error

    def assert_no_temporary(self):
        self.assertEqual(list(self.disk.root.glob("*.tmp-*")), [])

    def test_non_windows_access_error_has_no_retry(self):
        original = self.path.read_bytes()
        proxy = mock.Mock(wraps=os)
        proxy.name = "posix"
        proxy.replace.side_effect = self.denied()
        with mock.patch.object(file_transfer_storage, "os", proxy), mock.patch.object(file_transfer_storage.time, "sleep") as sleep:
            with self.assertRaises(PermissionError):
                self.disk.write_summary({**self.record, "revision": 2})
        self.assertEqual(proxy.replace.call_count, 1)
        sleep.assert_not_called()
        self.assertEqual(self.path.read_bytes(), original)
        self.assert_no_temporary()

    def test_unrelated_windows_error_has_no_retry(self):
        with mock.patch.object(file_transfer_storage.os, "replace", side_effect=self.denied(112)) as replace, \
                mock.patch.object(file_transfer_storage.time, "sleep") as sleep:
            with self.assertRaises(PermissionError):
                self.disk.write_summary({**self.record, "revision": 2})
        self.assertEqual(replace.call_count, 1)
        sleep.assert_not_called()
        self.assertEqual(self.disk.read_summary(self.path)["revision"], 1)
        self.assert_no_temporary()

    def test_json_decoding_happens_after_reader_is_closed(self):
        decode = self.disk._decode
        def decode_after_replacement(encoded, code):
            self.disk.write_summary({**self.record, "revision": 2})
            return decode(encoded, code)
        with mock.patch.object(self.disk, "_decode", side_effect=decode_after_replacement):
            self.assertEqual(self.disk.read_summary(self.path)["revision"], 1)
        self.assertEqual(self.disk.read_summary(self.path)["revision"], 2)
        self.assert_no_temporary()

    @unittest.skipUnless(os.name == "nt", "Native Windows delete sharing")
    def test_checkpoint_read_handle_shares_deletion_and_closes(self):
        lease = _TransferLease(self.disk.root / (self.record["transfer_id"] + ".lock"))
        self.assertTrue(lease.acquire())
        try:
            with self.disk._open_read(self.path) as reader:
                descriptor = reader.fileno()
                self.path.unlink()
                self.assertEqual(json.load(reader)["revision"], 1)
            with self.assertRaises(OSError):
                os.fstat(descriptor)
            self.disk.write_summary({**self.record, "revision": 2})
            self.assertEqual(self.disk.read_summary(self.path)["revision"], 2)
        finally:
            lease.close()
        self.assert_no_temporary()

    @unittest.skipUnless(os.name == "nt", "Native Windows sharing violation")
    def test_transient_external_reader_is_retried_and_paths_rechecked(self):
        descriptor = os.open(self.path, os.O_RDONLY | os.O_BINARY)
        closed = threading.Event()
        def release_reader():
            time.sleep(0.12)
            os.close(descriptor)
            closed.set()
        thread = threading.Thread(target=release_reader)
        thread.start()
        started = time.monotonic()
        try:
            with mock.patch.object(self.disk, "_guard", wraps=self.disk._guard) as guard:
                self.disk.write_summary({**self.record, "revision": 2})
            checked = [Path(call.args[0]) for call in guard.call_args_list]
            self.assertGreaterEqual(checked.count(self.path), 3)
            self.assertTrue(any(".tmp-" in path.name for path in checked))
        finally:
            thread.join(2)
            self.assertTrue(closed.is_set())
        elapsed = time.monotonic() - started
        self.assertLess(elapsed, 1.5)
        self.assertEqual(self.disk.read_summary(self.path)["revision"], 2)
        self.assert_no_temporary()

    @unittest.skipUnless(os.name == "nt", "Native Windows sharing violation")
    def test_permanent_external_reader_fails_with_original_preserved(self):
        original = self.path.read_bytes()
        descriptor = os.open(self.path, os.O_RDONLY | os.O_BINARY)
        started = time.monotonic()
        try:
            with self.assertRaises(PermissionError) as raised:
                self.disk.write_summary({**self.record, "revision": 2})
            self.assertIn(raised.exception.winerror, {5, 32, 33})
        finally:
            os.close(descriptor)
        elapsed = time.monotonic() - started
        self.assertGreaterEqual(elapsed, 0.45)
        self.assertLess(elapsed, 1.5)
        self.assertEqual(self.path.read_bytes(), original)
        self.assert_no_temporary()

    @unittest.skipUnless(os.name == "nt", "Windows reparse-point protection")
    def test_directory_junction_is_rejected_and_target_preserved(self):
        target = self.root / "outside-checkpoints"
        target.mkdir()
        payload = target / "payload.json"
        payload.write_bytes(b'{"keep":true}')
        junction = self.disk.root / "junction"
        result = subprocess.run(["cmd.exe", "/d", "/c", "mklink", "/J", str(junction), str(target)],
                                capture_output=True, text=True, timeout=10)
        if result.returncode:
            self.skipTest("Windows did not permit the isolated junction fixture")
        try:
            self.assertTrue(junction.is_junction())
            with self.assertRaises(FileTransferError) as raised:
                self.disk._open_read(junction / "payload.json")
            self.assertEqual(raised.exception.code, "invalid_checkpoint")
            self.assertEqual(payload.read_bytes(), b'{"keep":true}')
        finally:
            # Remove the link itself before recursive temporary-directory cleanup.
            os.rmdir(junction)

    @unittest.skipUnless(os.name == "nt", "Windows handle ownership")
    def test_descriptor_conversion_failure_closes_native_handle(self):
        import msvcrt
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        process = kernel.GetCurrentProcess
        process.restype = ctypes.c_void_p
        count = kernel.GetProcessHandleCount
        count.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
        count.restype = ctypes.c_int
        def handles():
            value = ctypes.c_uint32()
            self.assertTrue(count(process(), ctypes.byref(value)))
            return value.value
        before = handles()
        with mock.patch.object(msvcrt, "open_osfhandle", side_effect=OSError("Injected descriptor conversion failure")):
            for _index in range(32):
                with self.assertRaises(OSError):
                    self.disk._open_read(self.path)
        self.assertLessEqual(handles(), before)
        self.disk.write_summary({**self.record, "revision": 2})


@unittest.skipUnless(os.name == "nt", "Windows concurrent checkpoint acceptance")
class TestFileTransferConcurrentManagers(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qcopilots-manager-sharing-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "source"
        self.source.mkdir()
        for index in range(128):
            (self.source / f"{index:04d}.bin").write_bytes(bytes(range(256)) * 96)
        self.backend = FakeBackend(self.root / "remote")
        self.backend.local("/uploads").mkdir()
        self.backend.write_gate = threading.Event()
        self.config = MemoryConfigStore(self.root / "config")
        self.writer = FileTransferJobManager(self.config, self.backend, FilesystemPolicy())
        self.addCleanup(self.writer.shutdown)
        self.addCleanup(self.backend.write_gate.set)

    def start_writer(self):
        initial = self.writer.start("upload", [str(self.source)], "/uploads")
        self.assertTrue(self.backend.write_observed.wait(5))
        return initial

    def finish_writer(self, initial):
        self.backend.write_gate.set()
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            result = self.writer.get_job(initial["job_id"])
            if result["state"] in {"succeeded", "failed", "cancelled"}:
                self.assertEqual(result["state"], "succeeded", result)
                return
            time.sleep(0.01)
        self.fail("Isolated writer did not finish within 45 seconds")

    def test_second_manager_list_and_get_during_checkpoint_writes(self):
        initial = self.start_writer()
        observer = FileTransferJobManager(self.config, object(), FilesystemPolicy())
        done = threading.Event()
        results, errors = [], []
        def query():
            while not done.is_set():
                try:
                    observer.list_jobs()
                    results.append(observer.get_job(initial["job_id"], files_limit=10)["state"])
                except Exception as error:
                    errors.append((type(error).__name__, getattr(error, "code", None)))
        thread = threading.Thread(target=query)
        thread.start()
        try:
            self.finish_writer(initial)
        finally:
            done.set()
            thread.join(5)
            observer.shutdown()
        self.assertFalse(thread.is_alive())
        self.assertGreater(len(results), 5)
        self.assertEqual(errors, [])

    def test_separate_process_list_and_get_during_checkpoint_writes(self):
        initial = self.start_writer()
        stop = self.root / "stop-reader"
        ready = self.root / "reader-ready"
        code = """
import json, sys, time
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from qcopilots_common.security_policy import FilesystemPolicy
from qcopilots_mcp_server_processing_general.file_transfer_config import FileTransferConfigStore
from qcopilots_mcp_server_processing_general.file_transfer_jobs import FileTransferJobManager
manager = FileTransferJobManager(FileTransferConfigStore(sys.argv[2]), object(), FilesystemPolicy())
Path(sys.argv[5]).write_text('ready', encoding='utf-8')
count, errors = 0, []
try:
    deadline = time.monotonic() + 60
    while not Path(sys.argv[4]).exists() and time.monotonic() < deadline:
        try:
            manager.list_jobs()
            manager.get_job(sys.argv[3], files_limit=10)
            count += 1
        except Exception as error:
            errors.append([type(error).__name__, getattr(error, 'code', None)])
finally:
    manager.shutdown()
print(json.dumps({'count': count, 'errors': errors}), flush=True)
"""
        child = subprocess.Popen([sys.executable, "-B", "-c", code, str(PLUGINS_ROOT),
                                  str(self.config.root), initial["job_id"], str(stop), str(ready)],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                                 encoding="utf-8", env={**os.environ, "PYTHONIOENCODING": "utf-8"})
        try:
            deadline = time.monotonic() + 10
            while not ready.exists() and child.poll() is None and time.monotonic() < deadline:
                time.sleep(0.02)
            self.assertTrue(ready.exists(), "Checkpoint reader did not initialize within 10 seconds")
            self.finish_writer(initial)
        finally:
            try:
                stop.write_text("stop", encoding="utf-8")
            finally:
                try:
                    stdout, stderr = child.communicate(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    stdout, stderr = child.communicate(timeout=5)
        self.assertEqual(child.returncode, 0, stderr)
        result = json.loads(stdout)
        self.assertGreater(result["count"], 5)
        self.assertEqual(result["errors"], [])


if __name__ == "__main__":
    unittest.main()
