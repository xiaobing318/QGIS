"""Isolated durability and bounded-write tests for file transfer checkpoint storage.

Every test owns a TemporaryDirectory. No network connection or real transfer
destination is used, including in corruption and interrupted-write scenarios.
"""

from __future__ import annotations

import copy
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
import uuid
from pathlib import Path
from unittest import mock


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))

from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError
from qcopilots_mcp_server_processing_general.file_transfer_storage import DiskTransferStore


class TestDiskTransferStore(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qcopilots-file-transfer-storage-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.store_root = self.root / "checkpoints"
        self.store = DiskTransferStore(self.store_root)
        self.record = self.make_record(5)

    @staticmethod
    def make_record(count):
        transfer_id = str(uuid.uuid4())
        job_id = str(uuid.uuid4())
        files = []
        for index in range(count):
            name = f"{index:04d}-测量-é-🗺.gpkg"
            files.append({
                "kind": "file",
                "source": "C:/调查/" + name,
                "target": "C:/成果/" + name,
                "size": 1024,
                "mtime": 1728000000,
                "stage": f"C:/成果/.qcopilots-{transfer_id}-{index:08d}.part",
                "source_sha256": None,
                "target_before": None,
                "verified": False,
                "phase": "pending",
                "committed_offset": 0,
                "committed_prefix_sha256": hashlib.sha256(b"").hexdigest(),
            })
        return {
            "schema_version": 2,
            "transfer_id": transfer_id,
            "archived": False,
            "direction": "upload",
            "destination": "C:/成果",
            "files": files,
            "manifest_count": len(files),
            "manifest_ready": True,
            "created_directories": [],
            "directory_intents": [],
            "attempts": [{"job_id": job_id, "state": "running", "files": []}],
            "latest_job_id": job_id,
        }

    def transfer_root(self, record=None):
        return self.store_root / (record or self.record)["transfer_id"]

    def write_checkpoint(self, record=None):
        record = record or self.record
        self.store.write_manifest(record)
        self.store.write_summary(record)
        return self.store.read_summary(self.store_root / (record["transfer_id"] + ".json"))

    def assert_storage_error(self, code, callback, *args):
        with self.assertRaises(FileTransferError) as raised:
            callback(*args)
        self.assertEqual(raised.exception.code, code)

    def assert_no_private_keys(self, value):
        if isinstance(value, dict):
            self.assertFalse(any(str(key).startswith("_") for key in value))
            for item in value.values():
                self.assert_no_private_keys(item)
        elif isinstance(value, list):
            for item in value:
                self.assert_no_private_keys(item)

    def test_summary_omits_details_and_recursive_private_fields(self):
        self.record["_session"] = {"password": "must-never-persist"}
        self.record["status"] = {
            "message": "保留",
            "_secret": "must-never-persist",
            "items": [{"visible": True, "_password": "must-never-persist"}],
        }
        self.record["attempts"][0]["files"] = copy.deepcopy(self.record["files"])
        self.record["attempts"][0]["_runtime"] = "must-never-persist"
        original = copy.deepcopy(self.record)
        self.store.write_summary(self.record)
        path = self.store_root / (self.record["transfer_id"] + ".json")
        saved = self.store.read_summary(path)
        for field in ("files", "created_directories", "directory_intents"):
            self.assertNotIn(field, saved)
        self.assertNotIn("files", saved["attempts"][0])
        self.assert_no_private_keys(saved)
        self.assertEqual(saved["status"]["message"], "保留")
        self.assertNotIn(b"must-never-persist", path.read_bytes())
        self.assertEqual(self.record, original)

    def test_archived_summary_is_written_to_history(self):
        self.record["archived"] = True
        self.store.write_summary(self.record)
        path = self.store_root / "history" / (self.record["transfer_id"] + ".json")
        self.assertEqual(self.store.read_summary(path)["transfer_id"], self.record["transfer_id"])
        self.assertFalse((self.store_root / (self.record["transfer_id"] + ".json")).exists())

    def test_write_stats_count_successful_utf8_bytes(self):
        before = dict(self.store.write_stats)
        self.record["label"] = "合成地理数据 🗺"
        self.store.write_summary(self.record)
        path = self.store_root / (self.record["transfer_id"] + ".json")
        written = path.read_bytes()
        self.assertGreater(len(written), len(written.decode("utf-8")))
        self.assertEqual(self.store.write_stats["write_count"] - before["write_count"], 1)
        self.assertEqual(self.store.write_stats["bytes_written"] - before["bytes_written"], len(written))

    def test_manifest_round_trip_preserves_order_and_unicode(self):
        # A directory occupies its own index, just as in a transfer manifest.
        self.record["files"][1].update(kind="directory", size=0, stage=None)
        original = copy.deepcopy(self.record["files"])
        summary = self.write_checkpoint()
        summary = self.store.load_details(summary)
        self.assertTrue(summary["_details_loaded"])
        self.assertEqual(summary["files"], original)
        self.assertEqual(summary["created_directories"], [])
        self.assertEqual(summary["directory_intents"], [])

    def test_entry_update_leaves_static_manifest_and_other_entries_unchanged(self):
        self.write_checkpoint()
        root = self.transfer_root()
        manifest_before = (root / "manifest.jsonl").read_bytes()
        offsets_before = (root / "offsets.json").read_bytes()
        other_before = (root / "states" / "0.json").read_bytes()
        entry = copy.deepcopy(self.record["files"][2])
        entry.update(
            source_sha256=hashlib.sha256(b"source").hexdigest(),
            target_before={"size": 6, "sha256": hashlib.sha256(b"target").hexdigest()},
            verified=True,
            phase="transferring",
            committed_offset=512,
            committed_prefix_sha256=hashlib.sha256(b"prefix").hexdigest(),
        )
        self.store.write_entry(self.record, 2, entry)
        self.assertEqual((root / "manifest.jsonl").read_bytes(), manifest_before)
        self.assertEqual((root / "offsets.json").read_bytes(), offsets_before)
        self.assertEqual((root / "states" / "0.json").read_bytes(), other_before)
        entries, total = self.store.read_manifest_page(self.record, 2, 1)
        self.assertEqual(total, 5)
        self.assertEqual(entries, [entry])

    def test_manifest_pages_cover_each_entry_once(self):
        self.write_checkpoint()
        combined = []
        for offset in range(0, 5, 2):
            entries, total = self.store.read_manifest_page(self.record, offset, 2)
            self.assertEqual(total, 5)
            combined.extend(entries)
        self.assertEqual(combined, self.record["files"])
        for offset in (5, 500):
            self.assertEqual(self.store.read_manifest_page(self.record, offset, 2), ([], 5))

    def test_empty_manifest_pages_and_details(self):
        record = self.make_record(0)
        summary = self.write_checkpoint(record)
        self.assertEqual(self.store.read_manifest_page(summary, 0, 10), ([], 0))
        summary = self.store.load_details(summary)
        self.assertEqual(summary["files"], [])

    def test_manifest_pagination_rejects_invalid_arguments(self):
        self.write_checkpoint()
        for offset, limit in ((-1, 1), (0, 0), (0, -1), (True, 1), (0, True), (0.5, 1), (0, 1.5), ("0", 1)):
            with self.subTest(offset=offset, limit=limit):
                self.assert_storage_error("invalid_checkpoint", self.store.read_manifest_page, self.record, offset, limit)

    def test_manifest_missing_or_corrupt_components_fail_closed(self):
        self.write_checkpoint()
        root = self.transfer_root()
        for path in (root / "manifest.jsonl", root / "offsets.json", root / "states" / "0.json"):
            original = path.read_bytes()
            for replacement in (None, b"{invalid-json\n", b"[]"):
                with self.subTest(path=path.name, replacement=replacement):
                    if replacement is None:
                        path.unlink()
                    else:
                        path.write_bytes(replacement)
                    try:
                        self.assert_storage_error("invalid_checkpoint", self.store.read_manifest_page, self.record, 0, 1)
                    finally:
                        path.write_bytes(original)

    def test_summary_missing_corrupt_or_wrong_identity_is_rejected(self):
        self.write_checkpoint()
        path = self.store_root / (self.record["transfer_id"] + ".json")
        path.unlink()
        with self.assertRaises(FileNotFoundError):
            self.store.read_summary(path)
        for contents in (b"{broken", b"[]", json.dumps({"transfer_id": str(uuid.uuid4()), "schema_version": 2}).encode()):
            with self.subTest(contents=contents):
                path.write_bytes(contents)
                self.assert_storage_error("invalid_checkpoint", self.store.read_summary, path)

    def test_malformed_types_keep_stable_storage_error_codes(self):
        self.write_checkpoint()
        state = self.transfer_root() / "states" / "0.json"
        original_state = state.read_bytes()
        malformed = json.loads(original_state)
        malformed["values"]["phase"] = []
        state.write_text(json.dumps(malformed), encoding="utf-8")
        try:
            self.assert_storage_error("invalid_checkpoint", self.store.read_manifest_page, self.record, 0, 1)
        finally:
            state.write_bytes(original_state)

        malformed_record = self.make_record(1)
        malformed_record["files"][0]["kind"] = []
        self.assert_storage_error("invalid_checkpoint", self.store.write_manifest, malformed_record)
        manifest = self.transfer_root() / "manifest.jsonl"
        original_manifest = manifest.read_bytes()
        rows = [json.loads(row) for row in original_manifest.splitlines()]
        rows[0]["kind"] = []
        manifest.write_text("\n".join(json.dumps(row) for row in rows) + "\n", encoding="utf-8")
        try:
            self.assert_storage_error("invalid_checkpoint", self.store.read_manifest_page, self.record, 0, 1)
        finally:
            manifest.write_bytes(original_manifest)

        one_entry = self.make_record(1)
        self.write_checkpoint(one_entry)
        one_entry["manifest_count"] = True
        self.assert_storage_error("invalid_checkpoint", self.store.write_manifest, one_entry)
        self.assert_storage_error("invalid_checkpoint", self.store.read_manifest_page, one_entry, 0, 1)
        self.assert_storage_error("invalid_checkpoint", self.store.load_details, one_entry)

        job_id = self.record["latest_job_id"]
        self.store.write_receipt(self.record, job_id, self.record["files"])
        for path, code, callback, arguments in (
            (state, "invalid_checkpoint", self.store.read_manifest_page, (self.record, 0, 1)),
            (self.transfer_root() / "offsets.json", "invalid_checkpoint", self.store.read_manifest_page, (self.record, 0, 1)),
            (self.store_root / "history" / (job_id + ".files.offsets.json"), "invalid_receipt", self.store.read_receipt_page, (job_id, 0, 1)),
        ):
            with self.subTest(version_path=path.name):
                original = path.read_bytes()
                malformed = json.loads(original)
                malformed["version"] = True
                path.write_text(json.dumps(malformed), encoding="utf-8")
                try:
                    self.assert_storage_error(code, callback, *arguments)
                finally:
                    path.write_bytes(original)

        for files in ([[]], [False], [None], ["not a row"], None, 7):
            with self.subTest(receipt_rows=files):
                self.assert_storage_error("invalid_receipt", self.store.write_receipt, self.record, str(uuid.uuid4()), files)
        self.assertEqual(list(self.store_root.rglob("*.tmp-*")), [])

    def test_directory_intent_ownership_and_removal_round_trip(self):
        self.write_checkpoint()
        paths = ["C:/成果/深/层", "C:/成果", "C:/成果/深", "C:/成果/另一目录"]
        self.store.write_directory(self.record, paths[0], False)
        self.store.write_directory(self.record, paths[1], True)
        self.store.write_directory(self.record, paths[2], True)
        self.store.write_directory(self.record, paths[3], False)
        summary = self.store.read_summary(self.store_root / (self.record["transfer_id"] + ".json"))
        summary = self.store.load_details(summary)
        self.assertEqual(summary["created_directories"], [paths[1], paths[2]])
        self.assertEqual(summary["directory_intents"], [paths[3], paths[0]])
        self.store.write_directory(self.record, paths[0], True)
        self.store.write_directory(self.record, paths[3], None)
        summary.pop("_details_loaded", None)
        summary = self.store.load_details(summary)
        self.assertEqual(summary["created_directories"], [paths[1], paths[2], paths[0]])
        self.assertEqual(summary["directory_intents"], [])

    def test_directory_log_corruption_is_rejected(self):
        self.write_checkpoint()
        path = "C:/成果/目录"
        self.store.write_directory(self.record, path, False)
        directory = self.transfer_root() / "directories"
        log = directory / (hashlib.sha256(path.encode("utf-8")).hexdigest() + ".json")
        self.assertTrue(log.is_file())
        for payload in (b"{bad", b"[]", json.dumps({"path": "C:/other", "owned": True}).encode()):
            with self.subTest(payload=payload):
                log.write_bytes(payload)
                summary = self.store.read_summary(self.store_root / (self.record["transfer_id"] + ".json"))
                self.assert_storage_error("invalid_checkpoint", self.store.load_details, summary)

    def test_receipt_is_immutable_and_survives_active_data_removal(self):
        self.write_checkpoint()
        job_id = self.record["latest_job_id"]
        files = copy.deepcopy(self.record["files"])
        self.store.write_receipt(self.record, job_id, files)
        before_retry = dict(self.store.write_stats)
        self.store.write_receipt(self.record, job_id, copy.deepcopy(files))
        self.assertGreater(self.store.write_stats["write_count"], before_retry["write_count"])
        self.assertGreater(self.store.write_stats["bytes_written"], before_retry["bytes_written"])
        changed = copy.deepcopy(files)
        changed[0]["phase"] = "published"
        self.assert_storage_error("invalid_receipt", self.store.write_receipt, self.record, job_id, changed)
        self.store.remove_active_data(self.record["transfer_id"])
        self.assertFalse(self.transfer_root().exists())
        self.assertTrue((self.store_root / (self.record["transfer_id"] + ".json")).exists())
        self.assertEqual(self.store.read_receipt_page(job_id, 0, 10), (files, 5))

    def test_receipt_pages_remain_specific_to_the_attempt(self):
        first_job = self.record["latest_job_id"]
        first_files = copy.deepcopy(self.record["files"])
        self.store.write_receipt(self.record, first_job, first_files)
        second_job = str(uuid.uuid4())
        second_files = copy.deepcopy(first_files)
        second_files[0].update(phase="published", committed_offset=1024, verified=True)
        self.record["attempts"].append({"job_id": second_job, "state": "succeeded"})
        self.store.write_receipt(self.record, second_job, second_files)
        restarted = DiskTransferStore(self.store_root)
        combined = []
        for offset in (0, 2, 4):
            page, total = restarted.read_receipt_page(first_job, offset, 2)
            self.assertEqual(total, 5)
            combined.extend(page)
        self.assertEqual(combined, first_files)
        self.assertEqual(restarted.read_receipt_page(second_job, 0, 10), (second_files, 5))
        self.assertEqual(restarted.read_receipt_page(first_job, 50, 1), ([], 5))

    def test_receipt_index_recovery_preserves_interrupted_attempt_results(self):
        job_id = self.record["latest_job_id"]
        files = copy.deepcopy(self.record["files"])
        index_path = self.store_root / "history" / (job_id + ".files.offsets.json")
        receipt_path = self.store_root / "history" / (job_id + ".files.jsonl")
        write_json = self.store._write_json
        interrupted = False

        def interrupt_index(path, *args, **kwargs):
            nonlocal interrupted
            if Path(path) == index_path and not interrupted:
                interrupted = True
                raise OSError("Injected interruption before receipt index commit")
            return write_json(path, *args, **kwargs)

        with mock.patch.object(self.store, "_write_json", side_effect=interrupt_index):
            with self.assertRaises(OSError):
                self.store.write_receipt(self.record, job_id, files)
        self.assertTrue(interrupted)
        self.assertFalse(index_path.exists())
        original_receipt = receipt_path.read_bytes()
        self.assertTrue(original_receipt.endswith(b"\n"))
        self.record["files"][0].update(phase="published", committed_offset=1024, verified=True)

        restarted = DiskTransferStore(self.store_root)
        restarted.recover_receipt_index(self.record, job_id)
        self.assertEqual(restarted.read_receipt_page(job_id, 0, 10), (files, len(files)))
        self.assertEqual(receipt_path.read_bytes(), original_receipt)
        index = json.loads(index_path.read_bytes())
        self.assertEqual(index["transfer_id"], self.record["transfer_id"])
        self.assertEqual(index["job_id"], job_id)
        previous_index = index_path.read_bytes()
        previous_mtime = index_path.stat().st_mtime_ns
        previous_stats = dict(restarted.write_stats)
        restarted.recover_receipt_index(self.record, job_id)
        self.assertEqual(index_path.read_bytes(), previous_index)
        self.assertEqual(index_path.stat().st_mtime_ns, previous_mtime)
        self.assertEqual(restarted.write_stats, previous_stats)

    def test_receipt_index_recovery_does_not_repair_existing_corrupt_index(self):
        job_id = self.record["latest_job_id"]
        self.store.write_receipt(self.record, job_id, self.record["files"])
        index_path = self.store_root / "history" / (job_id + ".files.offsets.json")
        corrupt_index = b"{existing corrupt index"
        index_path.write_bytes(corrupt_index)
        previous_mtime = index_path.stat().st_mtime_ns
        previous_stats = dict(self.store.write_stats)
        self.store.recover_receipt_index(self.record, job_id)
        self.assertEqual(index_path.read_bytes(), corrupt_index)
        self.assertEqual(index_path.stat().st_mtime_ns, previous_mtime)
        self.assertEqual(self.store.write_stats, previous_stats)
        self.assert_storage_error("invalid_receipt", self.store.read_receipt_page, job_id, 0, 1)

    def test_receipt_index_recovery_rejects_incomplete_or_invalid_rows(self):
        for payload in (None, b"{}", b"\n", b"{}\n\n", b"[]\n", b"{}\n[]\n", b"null\n", b"{broken}\n"):
            with self.subTest(payload=payload):
                job_id = str(uuid.uuid4())
                receipt_path = self.store_root / "history" / (job_id + ".files.jsonl")
                index_path = self.store_root / "history" / (job_id + ".files.offsets.json")
                if payload is not None:
                    receipt_path.write_bytes(payload)
                self.assert_storage_error("invalid_receipt", self.store.recover_receipt_index, self.record, job_id)
                self.assertFalse(index_path.exists())
                if payload is not None:
                    self.assertEqual(receipt_path.read_bytes(), payload)
        self.assertEqual(list(self.store_root.rglob("*.tmp-*")), [])

    def test_receipt_index_recovery_accepts_empty_receipt(self):
        job_id = self.record["latest_job_id"]
        receipt_path = self.store_root / "history" / (job_id + ".files.jsonl")
        receipt_path.write_bytes(b"")
        self.store.recover_receipt_index(self.record, job_id)
        self.assertEqual(self.store.read_receipt_page(job_id, 0, 10), ([], 0))
        self.assertEqual(receipt_path.read_bytes(), b"")

    def test_empty_receipt_and_receipt_pagination_validation(self):
        job_id = self.record["latest_job_id"]
        self.store.write_receipt(self.record, job_id, [])
        self.assertEqual(self.store.read_receipt_page(job_id, 0, 10), ([], 0))
        for offset, limit in ((-1, 1), (0, 0), (True, 1), (0, False), (0.5, 1), (0, "2")):
            with self.subTest(offset=offset, limit=limit):
                self.assert_storage_error("invalid_receipt", self.store.read_receipt_page, job_id, offset, limit)

    def test_receipt_missing_or_corrupt_components_fail_closed(self):
        job_id = self.record["latest_job_id"]
        self.assert_storage_error("invalid_receipt", self.store.read_receipt_page, job_id, 0, 1)
        self.store.write_receipt(self.record, job_id, self.record["files"])
        for name in (job_id + ".files.jsonl", job_id + ".files.offsets.json"):
            path = self.store_root / "history" / name
            original = path.read_bytes()
            for payload in (None, b"{broken\n", b"[]"):
                with self.subTest(name=name, payload=payload):
                    if payload is None:
                        path.unlink()
                    else:
                        path.write_bytes(payload)
                    try:
                        self.assert_storage_error("invalid_receipt", self.store.read_receipt_page, job_id, 0, 1)
                    finally:
                        path.write_bytes(original)

    def test_remove_active_data_preserves_other_transfers_and_history(self):
        other = self.make_record(2)
        self.write_checkpoint()
        self.write_checkpoint(other)
        self.store.write_receipt(self.record, self.record["latest_job_id"], self.record["files"])
        other_before = (self.transfer_root(other) / "manifest.jsonl").read_bytes()
        self.store.remove_active_data(self.record["transfer_id"])
        self.assertFalse(self.transfer_root().exists())
        self.assertEqual((self.transfer_root(other) / "manifest.jsonl").read_bytes(), other_before)
        self.assertEqual(self.store.read_receipt_page(self.record["latest_job_id"], 0, 1)[1], 5)
        self.store.remove_active_data(self.record["transfer_id"])

    def test_invalid_transfer_and_job_ids_cannot_escape_the_store(self):
        sentinel = self.root / "sentinel.txt"
        sentinel.write_text("keep", encoding="utf-8")
        invalid_ids = ("../sentinel", str(self.root), "", "not-a-uuid", uuid.uuid4().hex, "AAAAAAAA-AAAA-4AAA-AAAA-AAAAAAAAAAAA")
        for identity in invalid_ids:
            record = copy.deepcopy(self.record)
            record["transfer_id"] = identity
            with self.subTest(identity=identity):
                self.assert_storage_error("invalid_checkpoint", self.store.write_summary, record)
                self.assert_storage_error("invalid_checkpoint", self.store.remove_active_data, identity)
                self.assert_storage_error("invalid_checkpoint", self.store.cleanup_temporary, identity)
                self.assert_storage_error("invalid_receipt", self.store.read_receipt_page, identity, 0, 1)
        self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")

    def test_summary_read_cannot_escape_the_store(self):
        outside = self.root / (self.record["transfer_id"] + ".json")
        outside.write_text(json.dumps(self.record), encoding="utf-8")
        self.assert_storage_error("invalid_checkpoint", self.store.read_summary, outside)

    def test_failed_atomic_replace_preserves_summary_and_cleans_temporary(self):
        self.store.write_summary(self.record)
        path = self.store_root / (self.record["transfer_id"] + ".json")
        original = path.read_bytes()
        before = dict(self.store.write_stats)
        self.record["status"] = "updated"
        with mock.patch("qcopilots_mcp_server_processing_general.file_transfer_storage.os.replace", side_effect=PermissionError("injected replace failure")):
            with self.assertRaises((OSError, FileTransferError)):
                self.store.write_summary(self.record)
        self.assertEqual(path.read_bytes(), original)
        self.assertEqual(self.store.write_stats["write_count"] - before["write_count"], 1)
        self.assertGreater(self.store.write_stats["bytes_written"], before["bytes_written"])
        self.assertEqual(list(self.store_root.rglob("*.tmp-*")), [])

    def test_failed_atomic_entry_replace_preserves_previous_progress(self):
        self.write_checkpoint()
        before = self.store.read_manifest_page(self.record, 0, 1)
        changed = copy.deepcopy(self.record["files"][0])
        changed.update(committed_offset=512, phase="transferring")
        with mock.patch("qcopilots_mcp_server_processing_general.file_transfer_storage.os.replace", side_effect=PermissionError("injected entry failure")):
            with self.assertRaises((OSError, FileTransferError)):
                self.store.write_entry(self.record, 0, changed)
        self.assertEqual(self.store.read_manifest_page(self.record, 0, 1), before)
        self.assertEqual(list(self.store_root.rglob("*.tmp-*")), [])

    def test_cleanup_temporary_is_limited_to_its_transfer(self):
        other = self.make_record(1)
        self.write_checkpoint()
        self.write_checkpoint(other)
        suffix = ".tmp-" + self.record["transfer_id"] + "-" + uuid.uuid4().hex
        other_suffix = ".tmp-" + other["transfer_id"] + "-" + uuid.uuid4().hex
        own = [
            self.store_root / (self.record["transfer_id"] + ".json" + suffix),
            self.transfer_root() / ("manifest.jsonl" + suffix),
            self.transfer_root() / "states" / ("0.json" + suffix),
            self.store_root / "history" / (self.record["latest_job_id"] + ".files.jsonl" + suffix),
        ]
        unrelated = [
            self.store_root / (other["transfer_id"] + ".json" + other_suffix),
            self.transfer_root(other) / "states" / ("0.json" + other_suffix),
            self.store_root / "history" / (other["latest_job_id"] + ".files.jsonl" + other_suffix),
            self.store_root / ("unrelated.tmp-" + uuid.uuid4().hex),
        ]
        for path in own + unrelated:
            path.write_text("interrupted", encoding="utf-8")
        self.store.cleanup_temporary(self.record["transfer_id"])
        self.assertTrue(all(not path.exists() for path in own))
        self.assertTrue(all(path.read_text(encoding="utf-8") == "interrupted" for path in unrelated))
        self.assertEqual(self.store.read_manifest_page(self.record, 0, 10), (self.record["files"], 5))

    def test_symlinked_transfer_directory_is_rejected_without_touching_target(self):
        outside = self.root / "outside"
        outside.mkdir()
        sentinel = outside / "sentinel.txt"
        sentinel.write_text("keep", encoding="utf-8")
        link = self.transfer_root()
        try:
            link.symlink_to(outside, target_is_directory=True)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"Directory symlinks unavailable: {error}")
        self.addCleanup(lambda: link.unlink(missing_ok=True))
        self.assert_storage_error("invalid_checkpoint", self.store.write_manifest, self.record)
        self.assert_storage_error("invalid_checkpoint", self.store.remove_active_data, self.record["transfer_id"])
        self.assert_storage_error("invalid_checkpoint", self.store.cleanup_temporary, self.record["transfer_id"])
        self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")

    def test_symlinked_entry_state_cannot_read_or_overwrite_outside(self):
        self.write_checkpoint()
        state = self.transfer_root() / "states" / "0.json"
        outside = self.root / "outside-state.json"
        outside.write_bytes(state.read_bytes())
        original = outside.read_bytes()
        state.unlink()
        try:
            state.symlink_to(outside)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"File symlinks unavailable: {error}")
        self.addCleanup(lambda: state.unlink(missing_ok=True))
        self.assert_storage_error("invalid_checkpoint", self.store.read_manifest_page, self.record, 0, 1)
        self.assert_storage_error("invalid_checkpoint", self.store.write_entry, self.record, 0, self.record["files"][0])
        self.assertEqual(outside.read_bytes(), original)

    @unittest.skipUnless(os.name == "nt", "Windows directory junction regression")
    def test_windows_junctions_reject_store_root_and_active_tree(self):
        target = self.root / "junction-target"
        target.mkdir()
        sentinel = target / "sentinel.txt"
        sentinel.write_text("keep", encoding="utf-8")
        for location in ("store_root", "active_root", "nested_active"):
            with self.subTest(location=location):
                if location == "store_root":
                    junction = self.root / "junction-store"
                elif location == "active_root":
                    junction = self.transfer_root()
                else:
                    self.write_checkpoint()
                    junction = self.transfer_root() / "junction-child"
                # Both paths are inside this test's temporary directory. Never
                # recursively remove a junction, even when an assertion fails.
                junction.relative_to(self.root)
                target.relative_to(self.root)
                try:
                    try:
                        result = subprocess.run(
                            ["cmd.exe", "/d", "/c", "mklink", "/J", str(junction), str(target)],
                            capture_output=True, text=True, errors="replace", timeout=10,
                        )
                    except (OSError, subprocess.TimeoutExpired) as error:
                        self.skipTest(f"Cannot create a Windows junction: {error}")
                    if result.returncode:
                        self.skipTest(f"mklink /J failed ({result.returncode}): {result.stdout} {result.stderr}")
                    self.assertTrue(junction.is_junction())
                    if location == "store_root":
                        self.assert_storage_error("invalid_checkpoint", DiskTransferStore, junction)
                    else:
                        if location == "active_root":
                            self.assert_storage_error("invalid_checkpoint", self.store.write_manifest, self.record)
                        self.assert_storage_error("invalid_checkpoint", self.store.remove_active_data, self.record["transfer_id"])
                        self.assert_storage_error("invalid_checkpoint", self.store.cleanup_temporary, self.record["transfer_id"])
                    self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")
                finally:
                    if os.path.lexists(junction):
                        os.rmdir(junction)
                self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")

    def test_writes_scale_linearly_and_progress_write_size_is_bounded(self):
        totals = []
        progress_bytes = []
        for count in (32, 256):
            store = DiskTransferStore(self.root / f"scale-{count}")
            record = self.make_record(count)
            store.write_manifest(record)
            store.write_summary(record)
            before_progress = dict(store.write_stats)
            for index, initial in enumerate(record["files"]):
                entry = copy.deepcopy(initial)
                entry.update(phase="transferring", committed_offset=512)
                store.write_entry(record, index, entry)
            totals.append(dict(store.write_stats))
            progress_bytes.append((store.write_stats["bytes_written"] - before_progress["bytes_written"]) / count)
            self.assertEqual(store.write_stats["write_count"] - before_progress["write_count"], count)
        # Eight times as many files must not approach quadratic (64x) writes.
        self.assertLess(totals[1]["bytes_written"], totals[0]["bytes_written"] * 10)
        self.assertGreater(totals[1]["bytes_written"], totals[0]["bytes_written"] * 4)
        self.assertLessEqual(totals[1]["write_count"], totals[0]["write_count"] * 9)
        self.assertLess(progress_bytes[1], progress_bytes[0] * 1.25)


if __name__ == "__main__":
    unittest.main()
