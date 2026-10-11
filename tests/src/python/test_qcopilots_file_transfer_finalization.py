"""Publication checkpoint crash boundaries and isolated local I/O failures.

All transfers use fake SFTP in automatically cleaned temporary directories.
Hard-exit children use the same fixtures and never access a real server.
"""

from __future__ import annotations

import errno
import json
import os
import subprocess
import sys
import threading
import unittest
import uuid
from pathlib import Path
from unittest import mock


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))

import test_qcopilots_mcp_server_file_transfer_jobs as job_fixtures
import test_qcopilots_file_transfer_storage as storage_fixtures
from qcopilots_common.security_policy import FilesystemPolicy
from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError
from qcopilots_mcp_server_processing_general.file_transfer_jobs import FileTransferJobManager


class TestFileTransferPublicationFinalization(unittest.TestCase):
    def setUp(self):
        self.case = job_fixtures.TestQCopilotsMcpServerFileTransferJobs()
        self.case.setUp()
        self.addCleanup(self.case.doCleanups)

    def archive_with_active_unlink_failure(self):
        case = self.case
        original_unlink = Path.unlink
        refused = []
        def refuse(path, *args, **kwargs):
            if path.parent == case.manager.root and path.suffix == ".json":
                refused.append(path)
                raise PermissionError("Injected active summary unlink failure after history commit")
            return original_unlink(path, *args, **kwargs)
        source = case.source(data=b"published result remains immutable")
        with mock.patch.object(Path, "unlink", refuse):
            result = case.wait(case.upload([source]))
        self.assertEqual(result["state"], "succeeded")
        self.assertEqual(len(refused), 1)
        return source, result

    def assert_archive_retired(self, result):
        case = self.case
        self.assertFalse((case.manager.root / (result["transfer_id"] + ".json")).exists())
        self.assertFalse((case.manager.root / result["transfer_id"]).exists())
        self.assertTrue((case.manager.history_root / (result["transfer_id"] + ".json")).exists())

    def test_restart_retires_stale_active_header_before_active_data(self):
        source, result = self.archive_with_active_unlink_failure()
        case = self.case
        receipt = case.manager.history_root / (result["job_id"] + ".files.jsonl")
        receipt_before = receipt.read_bytes()
        case.manager = case.new_manager()
        self.assert_archive_retired(result)
        self.assertEqual(case.manager.get_job(result["job_id"])["state"], "succeeded")
        for _repeat in range(2):
            discarded = case.manager.discard_transfer(result["transfer_id"])
            self.assertTrue(discarded["cleanup"]["complete"])
            self.assertFalse(discarded["cleanup_pending"])
        self.assertEqual(receipt.read_bytes(), receipt_before)
        self.assertEqual(case.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())

    def test_details_are_loaded_only_from_newest_durable_header(self):
        source, result = self.archive_with_active_unlink_failure()
        case = self.case
        # Reproduce the state left by an older startup: the archive is durable,
        # but the stale active header survives after its mutable tree is gone.
        case.manager._disk.remove_active_data(result["transfer_id"])
        self.assertTrue((case.manager.root / (result["transfer_id"] + ".json")).exists())
        discarded = case.manager.discard_transfer(result["transfer_id"])
        self.assertTrue(discarded["cleanup"]["complete"])
        self.assert_archive_retired(result)
        self.assertEqual(case.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())

    def test_active_header_delete_failure_does_not_remove_its_data(self):
        _source, result = self.archive_with_active_unlink_failure()
        case = self.case
        active = case.manager.root / (result["transfer_id"] + ".json")
        original_unlink = Path.unlink
        def refuse(path, *args, **kwargs):
            if path == active:
                raise PermissionError("Injected persistent active-header refusal")
            return original_unlink(path, *args, **kwargs)
        with mock.patch.object(Path, "unlink", refuse):
            case.manager = case.new_manager()
        self.assertTrue(active.exists())
        self.assertTrue((case.manager.root / result["transfer_id"]).exists())
        self.assertEqual(case.manager.get_job(result["job_id"])["state"], "succeeded")
        case.manager = case.new_manager()
        self.assert_archive_retired(result)

    def test_valid_summary_does_not_authorize_unrelated_temp_names(self):
        case = self.case
        result = case.assert_success(case.upload([case.source(data=b"valid summary")]))
        owner = result["transfer_id"]
        suffix = ".tmp-" + owner + "-" + uuid.uuid4().hex
        paths = [directory / (name + suffix)
                 for directory in (case.manager.root, case.manager.history_root)
                 for name in ("notes.json", owner + ".json.bak")]
        for path in paths:
            path.write_bytes(b"unrelated content must survive")
        case.manager = case.new_manager()
        for path in paths:
            self.assertEqual(path.read_bytes(), b"unrelated content must survive")
        self.assertEqual(case.manager.get_job(result["job_id"])["state"], "succeeded")

    def test_planning_failure_receipt_recovers_despite_old_zero_count(self):
        case = self.case
        source = case.source(data=b"new conflicting input")
        target = case.backend.local("/uploads/data.bin")
        target.write_bytes(b"existing target must remain")
        original = case.manager._disk._write_json
        failed = []
        def fail_receipt_index(path, *args, **kwargs):
            if Path(path).name.endswith(".files.offsets.json") and not failed:
                failed.append(path)
                raise OSError(errno.ENOSPC, "Injected planning-failure receipt index failure")
            return original(path, *args, **kwargs)
        with mock.patch.object(case.manager._disk, "_write_json", fail_receipt_index):
            result = case.wait(case.upload([source]))
        self.assertEqual(result["state"], "failed")
        self.assertEqual(len(failed), 1)
        persisted = json.loads((case.manager.root / (result["transfer_id"] + ".json")).read_bytes())
        self.assertFalse(persisted["manifest_ready"])
        self.assertEqual(persisted["attempts"][-1]["files_total"], 0)
        self.assertEqual(len(result["files"]), 1)
        case.manager = case.new_manager()
        recovered = case.manager.get_job(result["job_id"])
        self.assertEqual(recovered["files"], result["files"])
        self.assertFalse(any(error["code"] == "receipt_recovery_failed" for error in recovered["errors"]))
        self.assertTrue(case.manager.discard_transfer(result["transfer_id"])["cleanup"]["complete"])
        self.assertEqual(target.read_bytes(), b"existing target must remain")

    def test_hard_exits_after_publication_recover_both_directions(self):
        payload = bytes(range(256)) * 256
        boundaries = ("receipt_data", "receipt_index", "history_summary", "active_header_delete", "active_data_delete")
        for direction in ("upload", "download"):
            for boundary in boundaries:
                with self.subTest(direction=direction, boundary=boundary):
                    root = self.case.root / (direction + "-" + boundary)
                    root.mkdir()
                    (root / "source.bin").write_bytes(payload)
                    child = subprocess.run([sys.executable, "-B", __file__, "--crash-worker",
                                            str(root), direction, boundary],
                                           capture_output=True, text=True, encoding="utf-8", timeout=20,
                                           env={**os.environ, "PYTHONIOENCODING": "utf-8"})
                    self.assertEqual(child.returncode, 74, child.stdout + child.stderr)
                    config = job_fixtures.MemoryConfigStore(root / "config")
                    backend = job_fixtures.FakeBackend(root / "remote")
                    target = backend.local("/uploads/source.bin") if direction == "upload" else root / "download" / "source.bin"
                    self.assertEqual(target.read_bytes(), payload)
                    history = config.root / "file_transfers" / "history"
                    receipts = list(history.glob("*.files.jsonl"))
                    self.assertEqual(len(receipts), 1)
                    receipt_bytes = receipts[0].read_bytes()
                    manager = FileTransferJobManager(config, backend, FilesystemPolicy())
                    try:
                        snapshot = manager.list_jobs()["jobs"][0]
                        original_job = snapshot["job_id"]
                        transfer = snapshot["transfer_id"]
                        result = manager.get_job(original_job)
                        self.assertEqual(result["files_total"], 1)
                        self.assertEqual(len(result["files"]), 1)
                        self.assertEqual(result["files"][0]["state"], "published")
                        if boundary.startswith("receipt_"):
                            self.assertTrue(result["resumable"])
                            resumed = manager.resume_job(original_job)
                            self.case.managers.append(manager)
                            previous_manager = self.case.manager
                            self.case.manager = manager
                            try:
                                finished = self.case.wait(resumed)
                            finally:
                                self.case.manager = previous_manager
                            self.assertEqual(finished["state"], "succeeded", finished)
                            self.assertEqual(finished["progress"]["bytes_transferred"], 0)
                            self.assertEqual(finished["progress"]["bytes_reused"], len(payload))
                        else:
                            self.assertEqual(result["state"], "succeeded")
                        self.assertFalse((manager.root / (transfer + ".json")).exists())
                        self.assertFalse((manager.root / transfer).exists())
                        for _repeat in range(2):
                            self.assertTrue(manager.discard_transfer(transfer)["cleanup"]["complete"])
                        self.assertEqual(target.read_bytes(), payload)
                        self.assertEqual(receipts[0].read_bytes(), receipt_bytes)
                    finally:
                        manager.shutdown()


class FaultyWriteFile:
    def __init__(self, stream, failure):
        self.stream, self.failure = stream, failure

    def __getattr__(self, name):
        return getattr(self.stream, name)

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return self.stream.__exit__(*args)

    def write(self, encoded):
        if self.failure == "short":
            return self.stream.write(encoded[:max(1, len(encoded) // 2)])
        if self.failure == "zero":
            return 0
        if self.failure == "write":
            self.stream.write(encoded[:min(5, len(encoded))])
            raise OSError(errno.ENOSPC, "Injected disk full during checkpoint write")
        return self.stream.write(encoded)

    def flush(self):
        if self.failure == "flush":
            raise OSError(errno.ENOSPC, "Injected disk full during checkpoint flush")
        return self.stream.flush()


class TestFileTransferFinalizationStorage(unittest.TestCase):
    def setUp(self):
        self.case = storage_fixtures.TestDiskTransferStore()
        self.case.setUp()
        self.addCleanup(self.case.doCleanups)
        self.case.write_checkpoint()

    def patch_write(self, failure):
        original = Path.open
        def open_file(path, *args, **kwargs):
            stream = original(path, *args, **kwargs)
            if ".tmp-" in path.name and args and args[0] == "xb":
                return FaultyWriteFile(stream, failure)
            return stream
        return mock.patch.object(Path, "open", open_file)

    def test_short_writes_finish_complete_json_and_jsonl(self):
        case = self.case
        record = case.record
        changed = {**record["files"][0], "phase": "transferring", "committed_offset": 5}
        with self.patch_write("short"):
            case.store.write_summary({**record, "revision": 2})
            case.store.write_entry(record, 0, changed)
            case.store.write_receipt(record, record["latest_job_id"], record["files"])
        self.assertEqual(case.store.read_summary(case.store_root / (record["transfer_id"] + ".json"))["revision"], 2)
        self.assertEqual(case.store.read_manifest_page(record, 0, 1)[0][0], changed)
        self.assertEqual(case.store.read_receipt_page(record["latest_job_id"], 0, 10), (record["files"], 5))
        self.assertEqual(list(case.store_root.rglob("*.tmp-*")), [])

    def test_write_flush_fsync_and_zero_progress_preserve_previous_checkpoint(self):
        case = self.case
        record = case.record
        summary = case.store_root / (record["transfer_id"] + ".json")
        entry = case.store_root / record["transfer_id"] / "states" / "0.json"
        before = {path: path.read_bytes() for path in (summary, entry)}
        for failure in ("write", "flush", "fsync", "zero"):
            for operation in ("summary", "entry", "receipt"):
                with self.subTest(failure=failure, operation=operation):
                    job = str(uuid.uuid4())
                    patch = (mock.patch("qcopilots_mcp_server_processing_general.file_transfer_storage.os.fsync",
                                        side_effect=OSError(errno.ENOSPC, "Injected fsync disk full"))
                             if failure == "fsync" else self.patch_write(failure))
                    with patch, self.assertRaises(OSError) as raised:
                        if operation == "summary":
                            case.store.write_summary({**record, "revision": 99})
                        elif operation == "entry":
                            case.store.write_entry(record, 0, {**record["files"][0], "committed_offset": 5})
                        else:
                            case.store.write_receipt(record, job, record["files"])
                    self.assertEqual(raised.exception.errno, errno.EIO if failure == "zero" else errno.ENOSPC)
                    for path, original in before.items():
                        self.assertEqual(path.read_bytes(), original)
                    self.assertFalse((case.store.history_root / (job + ".files.jsonl")).exists())
                    self.assertEqual(list(case.store_root.rglob("*.tmp-*")), [])

    def test_receipt_recovery_rejects_truncation_at_complete_row_boundary(self):
        case = self.case
        record = case.record
        job = record["latest_job_id"]
        record["attempts"][0]["files_total"] = 5
        case.store.write_receipt(record, job, record["files"])
        receipt = case.store.history_root / (job + ".files.jsonl")
        index = case.store.history_root / (job + ".files.offsets.json")
        index.unlink()
        truncated = b"".join(receipt.read_bytes().splitlines(keepends=True)[:-1])
        receipt.write_bytes(truncated)
        with self.assertRaises(FileTransferError) as raised:
            case.store.recover_receipt_index(record, job)
        self.assertEqual(raised.exception.code, "invalid_receipt")
        self.assertFalse(index.exists())
        self.assertEqual(receipt.read_bytes(), truncated)

    def test_committed_manifest_count_overrides_stale_attempt_zero(self):
        case = self.case
        record = case.record
        job = record["latest_job_id"]
        record["attempts"][0]["files_total"] = 0
        case.store.write_receipt(record, job, record["files"])
        index = case.store.history_root / (job + ".files.offsets.json")
        index.unlink()
        case.store.recover_receipt_index(record, job)
        self.assertEqual(case.store.read_receipt_page(job, 0, 10), (record["files"], 5))

    def test_temporary_cleanup_checks_exact_basename_and_parent(self):
        case = self.case
        owner, job = case.record["transfer_id"], case.record["latest_job_id"]
        active, history = case.store_root / owner, case.store.history_root
        (active / "directories").mkdir()
        (active / "extra").mkdir()
        suffix = ".tmp-" + owner + "-" + uuid.uuid4().hex
        owned_bases = [case.store_root / (owner + ".json"), history / (owner + ".json"),
                       history / (job + ".files.jsonl"), history / (job + ".files.offsets.json"),
                       active / "manifest.jsonl", active / "offsets.json", active / "states" / "0.json",
                       active / "directories" / ("a" * 64 + ".json")]
        unrelated_bases = [case.store_root / "notes.json", case.store_root / (owner + ".json.bak"),
                           case.store_root / (job + ".files.jsonl"), history / "notes.json",
                           history / (owner + ".json.bak"), history / "not-a-uuid.files.jsonl",
                           active / "notes.json", active / "states" / "01.json",
                           active / "states" / "0.json.bak", active / "directories" / ("a" * 63 + ".json"),
                           active / "extra" / "manifest.jsonl"]
        owned = [path.with_name(path.name + suffix) for path in owned_bases]
        owned.extend(directory / (owner + ".tmp-" + uuid.uuid4().hex)
                     for directory in (case.store_root, history))
        unrelated = [path.with_name(path.name + suffix) for path in unrelated_bases]
        for path in owned + unrelated:
            path.write_bytes(b"interrupted or unrelated")
        case.store.cleanup_temporary(owner)
        self.assertTrue(all(not path.exists() for path in owned))
        self.assertTrue(all(path.read_bytes() == b"interrupted or unrelated" for path in unrelated))


def run_crash_worker(root, direction, boundary):
    root = Path(root)
    backend = job_fixtures.FakeBackend(root / "remote")
    config = job_fixtures.MemoryConfigStore(root / "config")
    manager = FileTransferJobManager(config, backend, FilesystemPolicy())
    original_replace, original_unlink = os.replace, Path.unlink
    original_remove = manager._disk.remove_active_data
    def replace_then_exit(source, target):
        result = original_replace(source, target)
        target = Path(target)
        is_history = target.parent == manager.history_root
        if is_history and ((boundary == "receipt_data" and target.name.endswith(".files.jsonl"))
                           or (boundary == "receipt_index" and target.name.endswith(".files.offsets.json"))
                           or (boundary == "history_summary" and len(target.name) == 41 and target.suffix == ".json")):
            os._exit(74)
        return result
    def unlink_then_exit(path, *args, **kwargs):
        result = original_unlink(path, *args, **kwargs)
        if boundary == "active_header_delete" and path.parent == manager.root and path.suffix == ".json":
            os._exit(74)
        return result
    def remove_then_exit(transfer):
        if boundary == "active_data_delete" and (manager.history_root / (transfer + ".json")).exists():
            state = next((manager.root / transfer / "states").glob("*.json"))
            state.unlink()
            os._exit(74)
        return original_remove(transfer)
    os.replace, Path.unlink = replace_then_exit, unlink_then_exit
    manager._disk.remove_active_data = remove_then_exit
    if direction == "upload":
        backend.local("/uploads").mkdir()
        manager.start(direction, [str(root / "source.bin")], "/uploads")
    else:
        backend.local("/source.bin").write_bytes((root / "source.bin").read_bytes())
        manager.start(direction, ["/source.bin"], str(root / "download"))
    threading.Event().wait(15)
    raise RuntimeError("Crash boundary was not reached")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--crash-worker":
        run_crash_worker(*sys.argv[2:])
    else:
        unittest.main()
