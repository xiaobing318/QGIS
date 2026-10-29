"""File publication and recovery regression tests for QCopilots Processing.

These file-only tests do not require a QGIS application or provider registry.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

import os
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import patch


sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python" / "plugins"))

from qcopilots_common import processing_jobs as module

class TestQCopilotsProcessingPublication(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="qcopilots-publication-")
        self.root = Path(self.temp.name).resolve()
        self.root.relative_to(Path(tempfile.gettempdir()).resolve())

    def tearDown(self):
        self.temp.cleanup()

    def stage(self, name, old=None, new=b"new output"):
        target = self.root / name
        if old is not None:
            target.write_bytes(old)
        root = self.root / (name + "-stage")
        root.mkdir()
        artifact = root / name
        artifact.write_bytes(new)
        return {"parameter": name, "target": target, "staging_root": root,
                "staged_target": artifact, "family_paths": [target],
                "authorized_versions": [module._processing_file_version(target)]}

    def test_preflight_validates_all_stages_before_mutation(self):
        first = self.stage("one.bin", old=b"original one")
        second = self.stage("two.bin", old=b"original two")
        (second["staging_root"] / "invalid-dir").mkdir()
        receipt = module._new_processing_output_promotion_receipt()
        with self.assertRaisesRegex(RuntimeError, "regular file"):
            module._promote_processing_outputs([first, second], overwrite=True, receipt=receipt)
        self.assertEqual(first["target"].read_bytes(), b"original one")
        self.assertEqual(second["target"].read_bytes(), b"original two")
        self.assertEqual(receipt["published"], [])
        self.assertEqual(receipt["backups"], [])

    def test_receipt_precedes_staged_unlink(self):
        stage = self.stage("one.bin")
        receipt = module._new_processing_output_promotion_receipt()
        original_unlink = Path.unlink
        def fail_staged(path, *args, **kwargs):
            if path == stage["staged_target"]:
                raise PermissionError("injected staged unlink failure")
            return original_unlink(path, *args, **kwargs)
        with patch.object(Path, "unlink", fail_staged), self.assertRaises(PermissionError):
            module._promote_processing_outputs([stage], overwrite=False, receipt=receipt)
        self.assertEqual(receipt["published"], [stage["target"]])
        self.assertEqual(len(receipt["published_versions"]), 1)
        self.assertTrue(stage["target"].exists())
        self.assertTrue(module._rollback_processing_output_promotion(receipt)["complete"])
        self.assertFalse(stage["target"].exists())

    def test_symbolic_link_artifact_is_rejected_before_mutation(self):
        stage = self.stage("one.bin", old=b"original")
        receipt = module._new_processing_output_promotion_receipt()
        original_is_symlink = Path.is_symlink
        def is_test_link(path):
            return path == stage["staged_target"] or original_is_symlink(path)
        with patch.object(Path, "is_symlink", is_test_link), self.assertRaisesRegex(RuntimeError, "symbolic links"):
            module._promote_processing_outputs([stage], overwrite=True, receipt=receipt)
        self.assertEqual(stage["target"].read_bytes(), b"original")
        self.assertFalse(receipt["backups"])

    def test_sidecars_do_not_substitute_for_missing_requested_output(self):
        stage = self.stage("one.tif", old=b"original")
        stage["staged_target"].unlink()
        (stage["staging_root"] / "one.tif.aux.xml").write_text("sidecar", encoding="utf-8")
        receipt = module._new_processing_output_promotion_receipt()
        with self.assertRaisesRegex(RuntimeError, "requested output file"):
            module._promote_processing_outputs([stage], overwrite=True, receipt=receipt)
        self.assertEqual(stage["target"].read_bytes(), b"original")
        self.assertFalse(receipt["backups"])

    def test_publication_hash_failure_keeps_receipt_and_restores_original(self):
        stage = self.stage("one.bin", old=b"original")
        receipt = module._new_processing_output_promotion_receipt()
        original_version = module._processing_file_version
        def fail_published(path, **kwargs):
            if path == stage["target"] and receipt["published"]:
                raise RuntimeError("injected publication hash failure")
            return original_version(path, **kwargs)
        with patch.object(module, "_processing_file_version", fail_published), self.assertRaises(RuntimeError):
            module._promote_processing_outputs([stage], overwrite=True, receipt=receipt)
        self.assertEqual(len(receipt["backups"]), 1)
        self.assertEqual(len(receipt["published_versions"]), 1)
        self.assertTrue(module._rollback_processing_output_promotion(receipt)["complete"])
        self.assertEqual(stage["target"].read_bytes(), b"original")

    def test_family_change_during_preflight_prevents_any_backup(self):
        stage = self.stage("one.tif", old=b"original")
        sidecar = self.root / "one.tif.ovr"
        stage["family_paths"].append(sidecar)
        stage["authorized_versions"].append(module._processing_file_version(sidecar))
        receipt = module._new_processing_output_promotion_receipt()
        original_version = module._processing_file_version
        def create_during_hash(path, **kwargs):
            result = original_version(path, **kwargs)
            if path == stage["staged_target"]:
                sidecar.write_bytes(b"external overview")
            return result
        with patch.object(module, "_processing_file_version", create_during_hash), self.assertRaisesRegex(RuntimeError, "family changed"):
            module._promote_processing_outputs([stage], overwrite=True, receipt=receipt)
        self.assertFalse(receipt["backups"])
        self.assertEqual(stage["target"].read_bytes(), b"original")
        self.assertEqual(sidecar.read_bytes(), b"external overview")

    def test_family_change_during_publication_fails_without_deleting_external_sidecar(self):
        stage = self.stage("one.tif", old=b"original")
        sidecar = self.root / "one.tif.ovr"
        stage["family_paths"].append(sidecar)
        stage["authorized_versions"].append(module._processing_file_version(sidecar))
        receipt = module._new_processing_output_promotion_receipt()
        original_link = module._link_processing_file_no_clobber
        def create_during_link(source, target):
            original_link(source, target)
            if target == stage["target"]:
                sidecar.write_bytes(b"external overview")
        with patch.object(module, "_link_processing_file_no_clobber", create_during_link), self.assertRaisesRegex(RuntimeError, "family changed"):
            module._promote_processing_outputs([stage], overwrite=True, receipt=receipt)
        self.assertTrue(module._rollback_processing_output_promotion(receipt)["complete"])
        self.assertEqual(stage["target"].read_bytes(), b"original")
        self.assertEqual(sidecar.read_bytes(), b"external overview")

    def test_second_publication_failure_restores_both_targets(self):
        first = self.stage("one.bin", old=b"original one")
        second = self.stage("two.bin", old=b"original two")
        receipt = module._new_processing_output_promotion_receipt()
        original_link = module._link_processing_file_no_clobber
        def fail_second(source, target):
            if target == second["target"]:
                raise OSError("injected second output failure")
            return original_link(source, target)
        with patch.object(module, "_link_processing_file_no_clobber", fail_second), self.assertRaises(OSError):
            module._promote_processing_outputs([first, second], overwrite=True, receipt=receipt)
        self.assertTrue(module._rollback_processing_output_promotion(receipt)["complete"])
        self.assertEqual(first["target"].read_bytes(), b"original one")
        self.assertEqual(second["target"].read_bytes(), b"original two")

    def test_concurrent_output_is_preserved_and_reported(self):
        stage = self.stage("one.bin", old=b"original")
        receipt = module._promote_processing_outputs([stage], overwrite=True)
        stage["target"].write_bytes(b"external replacement")
        result = module._rollback_processing_output_promotion(receipt)
        self.assertFalse(result["complete"])
        self.assertEqual(stage["target"].read_bytes(), b"external replacement")
        self.assertEqual(receipt["backups"][0]["backup"].read_bytes(), b"original")
        self.assertIn(str(stage["target"]), result["residual_paths"])

    def test_changed_backup_is_not_restored_or_discarded(self):
        stage = self.stage("one.bin", old=b"original")
        receipt = module._promote_processing_outputs([stage], overwrite=True)
        backup = receipt["backups"][0]["backup"]
        backup.write_bytes(b"changed backup")
        result = module._rollback_processing_output_promotion(receipt)
        self.assertFalse(result["complete"])
        self.assertFalse(stage["target"].exists())
        self.assertEqual(backup.read_bytes(), b"changed backup")
        self.assertIn(str(stage["target"]), result["residual_paths"])
        self.assertIn(str(backup), result["residual_paths"])

    def test_residual_audit_continues_after_unreadable_target(self):
        first = self.stage("one.bin", old=b"original one")
        second = self.stage("two.bin", old=b"original two")
        receipt = module._promote_processing_outputs([first, second], overwrite=True)
        original_version = module._processing_file_version
        def fail_first(path, **kwargs):
            if path == first["target"]:
                raise PermissionError("injected unreadable target")
            return original_version(path, **kwargs)
        with patch.object(module, "_processing_file_version", fail_first):
            residuals = module._processing_receipt_residual_paths(receipt)
        self.assertIn(str(first["target"]), residuals)
        self.assertIn(str(second["target"]), residuals)
        self.assertIn(str(receipt["backups"][1]["backup"]), residuals)
        self.assertTrue(receipt["residual_errors"])

    def test_retry_resets_obsolete_errors(self):
        stage = self.stage("one.bin")
        receipt = module._promote_processing_outputs([stage], overwrite=False)
        def locked(path):
            raise PermissionError("injected locked target")
        with patch.object(module, "_remove_processing_path", locked):
            self.assertFalse(module._rollback_processing_output_promotion(receipt)["complete"])
        self.assertTrue(module._rollback_processing_output_promotion(receipt)["complete"])
        self.assertEqual(receipt["rollback_errors"], [])

    def test_commit_preserves_unknown_backup_directory_content(self):
        stage = self.stage("one.bin", old=b"original")
        receipt = module._promote_processing_outputs([stage], overwrite=True)
        root = receipt["backup_roots"][0]
        unknown = root / "external.txt"
        unknown.write_text("preserve me", encoding="utf-8")
        residuals = module._commit_processing_output_promotion(receipt)
        self.assertIn(str(root), residuals)
        self.assertEqual(unknown.read_text(encoding="utf-8"), "preserve me")
        self.assertEqual(stage["target"].read_bytes(), b"new output")

    def test_file_cleanup_retry_clears_only_its_previous_errors(self):
        stage = self.stage("one.bin")
        receipt = module._promote_processing_outputs([stage], overwrite=False)
        runtime = module._ProcessingRuntime(None, {}, None, None, False, 1,
            output_stages=[stage], output_promotion_receipt=receipt)
        runtime.cleanup_errors = ["project cleanup remains unresolved"]
        with patch.object(module, "_remove_processing_path", side_effect=PermissionError("locked")), \
             patch.object(module, "_cleanup_processing_project_layers", side_effect=AssertionError("QGIS access")):
            module._cleanup_processing_runtime_file_outputs(runtime)
        self.assertTrue(runtime.cleanup_file_errors)
        self.assertIsNotNone(runtime.output_promotion_receipt)
        module._cleanup_processing_runtime_file_outputs(runtime)
        self.assertEqual(runtime.cleanup_file_errors, [])
        self.assertEqual(runtime.cleanup_errors, ["project cleanup remains unresolved"])
        self.assertIsNone(runtime.output_promotion_receipt)
        self.assertFalse(runtime.cleanup_residual_paths)

    def test_success_cleanup_commits_backup_without_removing_output(self):
        stage = self.stage("one.bin", old=b"original")
        receipt = module._promote_processing_outputs([stage], overwrite=True)
        runtime = module._ProcessingRuntime(None, {}, None, None, False, 1,
            output_stages=[stage], output_promotion_receipt=receipt,
            output_promotion_state="published_success")
        module._cleanup_processing_runtime_file_outputs(runtime, terminal_state="succeeded")
        self.assertEqual(stage["target"].read_bytes(), b"new output")
        self.assertIsNone(runtime.output_promotion_receipt)
        self.assertTrue(module._processing_cleanup_receipt(runtime)["complete"])

    def test_file_worker_uses_large_file_policy_without_relaxing_synchronous_limit(self):
        path = self.root / "larger-than-test-cap.bin"
        path.write_bytes(b"file worker verifies this complete content")
        with patch.object(module, "MAX_PROCESSING_VERSION_FILE_BYTES", 8):
            with self.assertRaisesRegex(RuntimeError, "exceeds"):
                module._processing_file_version(path)
            with module._processing_file_work_context():
                version = module._processing_file_version(path, deadline=0)
        self.assertEqual(version["size"], path.stat().st_size)
        self.assertEqual(version["sha256"], module.hashlib.sha256(path.read_bytes()).hexdigest())

    def test_file_worker_detects_a_file_changed_during_hashing(self):
        path = self.root / "changing.bin"
        path.write_bytes(b"a" * (2 * 1024 * 1024))
        modified = False
        def modify_after_chunk(completed, total):
            nonlocal modified
            if completed and not modified:
                modified = True
                with path.open("r+b") as handle:
                    handle.write(b"external change")
        with module._processing_file_work_context(progress=modify_after_chunk):
            with self.assertRaisesRegex(RuntimeError, "changed"):
                module._processing_file_version(path)
        self.assertTrue(modified)

    def test_matching_metadata_does_not_hide_changed_content_between_hash_passes(self):
        path = self.root / "same-metadata.bin"
        path.write_bytes(b"a" * (2 * 1024 * 1024))
        metadata = path.stat()
        with path.open("rb") as handle:
            opened = os.fstat(handle.fileno())
        modified = False
        def modify_after_first_chunk(completed, total):
            nonlocal modified
            if completed and not modified:
                modified = True
                with path.open("r+b") as handle:
                    handle.write(b"external change")
        # Model a filesystem whose timestamp observations remain unchanged.
        # The content change is real and the second pass must detect it.
        with patch.object(Path, "stat", return_value=metadata), \
             patch.object(module.os, "fstat", return_value=opened), \
             module._processing_file_work_context(progress=modify_after_first_chunk):
            with self.assertRaisesRegex(RuntimeError, "changed between fingerprint observations"):
                module._processing_file_sha256(path)
        self.assertTrue(modified)

    def test_second_hash_pass_remains_cancellable_and_reports_all_read_bytes(self):
        path = self.root / "second-pass-cancel.bin"
        size = 2 * 1024 * 1024
        path.write_bytes(b"a" * size)
        cancelled = threading.Event()
        observations = []
        def cancel_during_second_pass(completed, total):
            observations.append((completed, total))
            if completed > size:
                cancelled.set()
        with module._processing_file_work_context(cancel_event=cancelled, progress=cancel_during_second_pass):
            with self.assertRaises(module.ProcessingFileCancelled):
                module._processing_file_sha256(path)
        self.assertTrue(cancelled.is_set())
        self.assertTrue(all(total == size * 2 for _, total in observations))
        self.assertGreater(observations[-1][0], size)

    def test_second_hash_pass_keeps_directory_deadline(self):
        path = self.root / "second-pass-budget.bin"
        path.write_bytes(b"a" * (2 * 1024 * 1024))
        checks = [None, None, None, RuntimeError("directory deadline")]
        with module._processing_file_work_context(), \
             patch.object(module, "_check_processing_fingerprint_deadline", side_effect=checks):
            with self.assertRaisesRegex(RuntimeError, "directory deadline"):
                module._processing_file_sha256(path, enforce_deadline=True)

    def test_publication_cancel_does_not_cancel_its_recovery(self):
        stage = self.stage("one.bin", old=b"original")
        receipt = module._new_processing_output_promotion_receipt()
        cancel_event = threading.Event()
        original_link = module._link_processing_file_no_clobber
        def cancel_after_publication(source, target):
            original_link(source, target)
            if target == stage["target"]:
                cancel_event.set()
        with module._processing_file_work_context(cancel_event=cancel_event), \
             patch.object(module, "_link_processing_file_no_clobber", cancel_after_publication):
            with self.assertRaises(module.ProcessingFileCancelled):
                module._promote_processing_outputs([stage], overwrite=True, receipt=receipt)
        self.assertTrue(cancel_event.is_set())
        self.assertEqual(receipt["published"], [stage["target"]])
        with module._processing_file_work_context(cancel_event=None):
            result = module._rollback_processing_output_promotion(receipt)
        self.assertTrue(result["complete"])
        self.assertEqual(stage["target"].read_bytes(), b"original")


if __name__ == "__main__":
    unittest.main(verbosity=2)
