"""Regression tests for the public asynchronous file/approval lifecycle."""

import tempfile
import threading
import unittest
import uuid
from pathlib import Path
from unittest import mock

from test_qcopilots_processing_jobs import (
    FakeAlgorithm, FakeClock, FakeProcessingDefinition, FakeProcessingEnvironment,
)
from qcopilots_common import processing_jobs as jobs
from qcopilots_common.processing_file_worker import file_work_context
from qcopilots_common.async_jobs import AsyncJobStore, JobNotFoundError


class ManualFiles:
    def __init__(self):
        self.pending = []

    def __call__(self, function, completed, cancel, progress):
        self.pending.append((function, completed, cancel, progress))

    def run(self):
        function, completed, cancel, progress = self.pending.pop(0)
        try:
            with file_work_context(cancel, progress):
                result = function()
        except Exception as error:
            completed(error, None)
        else:
            completed(None, result)

    def drain(self):
        for _ in range(20):
            if not self.pending:
                return
            self.run()
        raise AssertionError("file lifecycle did not settle")


class TestProcessingAsyncFiles(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / "input.txt"
        self.target = self.root / "output.txt"
        self.source.write_text("survey input", encoding="utf-8")
        algorithm = FakeAlgorithm("vector:copy", parameter_definitions=[
            FakeProcessingDefinition("INPUT", destination=False),
            FakeProcessingDefinition("OUTPUT", destination=True),
        ])
        self.environment = FakeProcessingEnvironment([algorithm])
        self.files = ManualFiles()
        dependencies = self.environment.dependencies()
        dependencies["file_runner"] = self.files
        self.manager = jobs.ProcessingJobManager(dependencies=dependencies)

    def arguments(self, **changes):
        args = {"category": "vector", "algorithm_id": "vector:copy",
                "parameters": {"INPUT": str(self.source), "OUTPUT": str(self.target)},
                "add_outputs_to_project": False, "client_request_id": "survey"}
        args.update(changes)
        return args

    def bounded_manager(self, capacity=1):
        clock = FakeClock()
        original_hook = mock.Mock(return_value={"complete": True})
        store = AsyncJobStore(terminal_ttl_seconds=10,
                              max_terminal_jobs_per_scope=capacity,
                              now=clock, on_evict=original_hook)
        self.manager = jobs.ProcessingJobManager(
            store=store, dependencies=self.manager._dependencies)
        return store, clock, original_hook

    def retained_publication(self, store):
        self.target.write_bytes(b"original survey")
        original = jobs._processing_file_version(self.target)
        backup_root = self.root / "backup"
        backup_root.mkdir()
        backup = backup_root / "original.txt"
        self.target.replace(backup)
        self.target.write_bytes(b"interrupted publication")
        receipt = jobs._new_processing_output_promotion_receipt()
        receipt.update(published=[self.target],
                       published_versions=[{"target": self.target,
                                            "version": jobs._processing_file_version(self.target)}],
                       backups=[{"target": self.target, "backup": backup,
                                 "original_version": original}],
                       backup_roots=[backup_root])
        runtime = jobs._ProcessingRuntime(
            algorithm=None, parameters={}, context=None, feedback=None,
            add_outputs_to_project=False, generation=0,
            output_promotion_receipt=receipt, output_promotion_state="promoted",
            cleanup_residual_paths=[str(self.target), str(backup)],
            cleanup_errors=["temporarily locked"], cleanup_file_errors=["temporarily locked"])
        job_id = str(uuid.uuid4())
        store.create("vector", {"job_id": job_id, "state": "failed",
                                "cleanup": jobs._processing_cleanup_receipt(runtime)},
                     client_request_id="retained", request_fingerprint="retained-fingerprint",
                     idempotency_namespace="execute", runtime=runtime)
        return job_id, runtime, original

    def preview(self):
        started = self.manager.start(self.arguments(overwrite_outputs=True))
        self.assertEqual(started["operation"], "preview")
        self.assertEqual(started["phase"], "preflight")
        self.assertFalse(started["algorithm_executed"])
        self.assertNotIn("overwrite_confirmation_token", started)
        self.files.drain()
        return self.manager.get(started["job_id"])

    def finish_algorithm(self, started, content="completed survey"):
        self.files.drain()
        runtime = self.manager.store.get_runtime(started["job_id"], "vector")
        task = self.environment.tasks[-1]
        task.begun.emit()
        Path(runtime.parameters["OUTPUT"]).write_text(content, encoding="utf-8")
        task.executed.emit(True, {"OUTPUT": runtime.parameters["OUTPUT"]})
        return task, runtime

    def test_preview_no_collision_never_runs_and_same_id_executes_separately(self):
        preview = self.preview()
        self.assertEqual(preview["state"], "succeeded")
        self.assertFalse(preview["confirmation_required"])
        self.assertFalse(preview["algorithm_executed"])
        self.assertFalse(self.target.exists())
        self.assertEqual(self.environment.tasks, [])
        started = self.manager.start(self.arguments())
        self.assertNotEqual(started["job_id"], preview["job_id"])
        task, runtime = self.finish_algorithm(started)
        self.files.drain()
        result = self.manager.get(started["job_id"])
        self.assertEqual(result["state"], "succeeded")
        self.assertEqual(result["operation"], "execute")
        self.assertTrue(result["algorithm_executed"])
        self.assertTrue(result["cleanup"]["complete"])
        self.assertEqual(self.target.read_text(), "completed survey")
        task.destroyed.emit()
        self.assertIsNone(runtime.context)

    def test_preview_ttl_starts_after_hash_and_claim_survives_slow_recheck(self):
        self.target.write_text("previous survey")
        clock = FakeClock()
        self.manager._overwrite_confirmations = jobs.ProcessingOverwriteConfirmationStore(clock=clock)
        started = self.manager.start(self.arguments(overwrite_outputs=True))
        clock.advance(1000)
        self.files.drain()
        preview = self.manager.get(started["job_id"])
        clock.advance(299)
        execution = self.manager.start(self.arguments(overwrite_outputs=True,
            overwrite_confirmation_token=preview["overwrite_confirmation_token"]))
        clock.advance(1000)
        self.finish_algorithm(execution)
        self.files.drain()
        self.assertEqual(self.manager.get(execution["job_id"])["state"], "succeeded")
        self.assertEqual(self.manager.start(self.arguments(overwrite_outputs=True,
            overwrite_confirmation_token=preview["overwrite_confirmation_token"]))["job_id"], execution["job_id"])

    def test_claim_is_one_use_and_parameter_changes_are_rejected(self):
        self.target.write_text("previous")
        preview = self.preview()
        token = preview["overwrite_confirmation_token"]
        first = self.manager.start(self.arguments(overwrite_outputs=True, overwrite_confirmation_token=token))
        second = self.manager.start(self.arguments(client_request_id="another", overwrite_outputs=True,
            overwrite_confirmation_token=token))
        self.assertEqual(second["state"], "failed")
        self.assertIn("already used", second["error"]["message"])
        self.manager.cancel(first["job_id"])
        self.files.drain()
        self.assertEqual(self.target.read_text(), "previous")

    def test_preview_target_and_input_binding_remain_exact(self):
        self.target.write_text("previous")
        preview = self.preview()
        self.target.write_text("external update")
        execution = self.manager.start(self.arguments(overwrite_outputs=True,
            overwrite_confirmation_token=preview["overwrite_confirmation_token"]))
        self.files.drain()
        result = self.manager.get(execution["job_id"])
        self.assertEqual(result["state"], "failed")
        self.assertIn("changed after preview", result["error"]["message"])
        self.assertEqual(self.target.read_text(), "external update")
        self.assertEqual(self.environment.tasks, [])

    def test_cancel_preflight_and_output_verification(self):
        started = self.manager.start(self.arguments())
        self.assertEqual(self.manager.cancel(started["job_id"])["state"], "cancelling")
        self.files.drain()
        self.assertEqual(self.manager.get(started["job_id"])["state"], "cancelled")
        second = self.manager.start(self.arguments(client_request_id="second"))
        self.finish_algorithm(second)
        self.manager.cancel(second["job_id"])
        self.files.drain()
        result = self.manager.get(second["job_id"])
        self.assertEqual(result["state"], "cancelled")
        self.assertTrue(result["cleanup"]["complete"])
        self.assertFalse(self.target.exists())

    def test_destroyed_algorithm_during_verify_does_not_release_context_early(self):
        started = self.manager.start(self.arguments())
        task, runtime = self.finish_algorithm(started)
        task.destroyed.emit()
        self.assertIsNone(runtime.task)
        self.assertIsNotNone(runtime.context)
        self.files.drain()
        self.assertEqual(self.manager.get(started["job_id"])["state"], "succeeded")
        self.assertIsNone(runtime.context)

    def test_task_terminated_without_executed_does_not_hang(self):
        started = self.manager.start(self.arguments())
        self.files.drain()
        self.environment.tasks[-1].taskTerminated.emit()
        self.files.drain()
        result = self.manager.get(started["job_id"])
        self.assertEqual(result["state"], "failed")
        self.assertEqual(result["error"]["code"], "task_terminated")

    def test_unexpected_recovery_error_can_be_retried_successfully(self):
        started = self.manager.start(self.arguments())
        task, runtime = self.finish_algorithm(started)
        self.files.run()  # verify and publish, recovery remains queued
        with mock.patch.object(jobs, "_cleanup_processing_runtime_file_outputs", side_effect=OSError("disk temporarily unavailable")):
            self.files.run()
        failed_cleanup = self.manager.store.get(started["job_id"], "vector")["cleanup"]
        self.assertFalse(failed_cleanup["complete"])
        self.assertIn("disk temporarily unavailable", failed_cleanup["errors"])
        self.manager.get(started["job_id"])
        self.files.drain()
        completed = self.manager.get(started["job_id"])
        self.assertTrue(completed["cleanup"]["complete"])
        self.assertEqual(completed["cleanup"]["errors"], [])
        self.assertEqual(self.target.read_text(), "completed survey")

    def test_shutdown_does_not_delete_active_worker_stages(self):
        started = self.manager.start(self.arguments())
        self.files.drain()
        runtime = self.manager.store.get_runtime(started["job_id"], "vector")
        stage = runtime.output_stages[0]["staging_root"]
        task, _ = self.finish_algorithm(started)
        self.manager.shutdown(timeout_seconds=0)
        self.assertTrue(stage.exists())
        self.assertIsNotNone(runtime.context)
        self.files.drain()
        result = self.manager.get(started["job_id"])
        self.assertEqual(result["state"], "cancelled")
        self.assertTrue(result["cleanup"]["complete"])
        self.assertFalse(stage.exists())

    def test_background_directory_member_still_has_deadline(self):
        folder = self.root / "directory"
        folder.mkdir()
        (folder / "data").write_bytes(b"abc")
        with file_work_context(), mock.patch.object(jobs, "_check_processing_fingerprint_deadline", side_effect=RuntimeError("deadline reached")):
            with self.assertRaisesRegex(RuntimeError, "deadline reached"):
                jobs._processing_directory_manifest(folder)

    def test_shutdown_includes_terminal_recovery_and_repeated_calls(self):
        started = self.manager.start(self.arguments())
        task, runtime = self.finish_algorithm(started)
        self.files.run()
        with mock.patch.object(jobs, "_cleanup_processing_runtime_file_outputs", side_effect=OSError("locked backup")):
            self.files.run()
        task.destroyed.emit()  # schedules a recovery attempt for terminal job
        self.assertIsNotNone(runtime.file_task)
        first = self.manager.shutdown(timeout_seconds=0)
        second = self.manager.shutdown(timeout_seconds=0)
        self.assertEqual(first, second)
        self.assertFalse(first["complete"])
        self.assertIn(started["job_id"], first["pending_job_ids"])
        self.files.drain()
        self.assertEqual(self.manager.shutdown(timeout_seconds=0), {"complete": True, "pending_job_ids": []})

    def test_recovery_schedule_failure_reports_public_targets_and_backups(self):
        started = self.manager.start(self.arguments())
        task, runtime = self.finish_algorithm(started)
        self.files.run()
        # Discard the queued cleanup callback to simulate a scheduling failure
        # before ownership can be transferred to a worker.
        self.files.pending.clear()
        runtime.file_task = None
        backup = self.root / "backup-original"
        runtime.output_promotion_receipt["backups"].append({
            "target": self.target, "backup": backup, "original_version": {"exists": False}})
        with mock.patch.object(self.manager, "_submit_file_work", side_effect=RuntimeError("task queue unavailable")):
            self.manager._recover_runtime(started["job_id"], "vector", runtime)
        receipt = self.manager.store.get(started["job_id"], "vector")["cleanup"]
        self.assertFalse(receipt["complete"])
        self.assertIn(str(self.target), receipt["residual_paths"])
        self.assertIn(str(backup), receipt["residual_paths"])

    def test_failed_receipt_survives_ttl_limit_and_recovers_from_injected_store(self):
        store, clock, original_hook = self.bounded_manager()
        job_id, runtime, original = self.retained_publication(store)
        clock.advance(11)
        store.purge()
        self.assertIs(store.get_runtime(job_id, "vector"), runtime)
        self.assertEqual(runtime.output_promotion_receipt["backups"][0]["original_version"], original)
        self.assertEqual(store.lookup_idempotent("vector", "retained", "retained-fingerprint",
                                                idempotency_namespace="execute")["job_id"], job_id)
        clean_id = str(uuid.uuid4())
        store.create("vector", {"job_id": clean_id, "state": "succeeded"})
        self.assertIs(store.get_runtime(job_id, "vector"), runtime)
        self.assertEqual(store.cleanup_audits("vector"), [])
        self.assertNotIn(mock.call(job_id, runtime), original_hook.call_args_list)
        with self.assertRaisesRegex(RuntimeError, "processing_recovery_capacity_exceeded"):
            self.manager.start(self.arguments())
        self.assertEqual(self.files.pending, [])
        # Query schedules recovery but does not read/hash files synchronously.
        with mock.patch.object(jobs, "_processing_file_version", side_effect=AssertionError("synchronous hash")):
            snapshot = self.manager.get(job_id)
        self.assertFalse(snapshot["cleanup"]["complete"])
        self.assertEqual(len(self.files.pending), 1)
        self.files.drain()
        self.assertEqual(self.target.read_bytes(), b"original survey")
        self.assertIsNone(runtime.output_promotion_receipt)
        self.assertTrue(self.manager.get(job_id)["cleanup"]["complete"])
        clock.advance(11)
        store.purge()
        with self.assertRaises(JobNotFoundError):
            store.get(job_id)
        # Successfully recovered records release their admission reservation.
        self.target.unlink()
        started = self.manager.start(self.arguments())
        self.manager.cancel(started["job_id"])
        self.files.drain()

    def test_capacity_rejects_new_request_before_claim_but_allows_replay(self):
        self.bounded_manager()
        started = self.manager.start(self.arguments())
        self.assertEqual(self.manager.start(self.arguments())["job_id"], started["job_id"])
        with mock.patch.object(self.manager._overwrite_confirmations, "claim") as claim:
            with self.assertRaisesRegex(RuntimeError, "processing_recovery_capacity_exceeded"):
                self.manager.start(self.arguments(client_request_id="new", overwrite_outputs=True,
                                                  overwrite_confirmation_token="not-consumed"))
            claim.assert_not_called()
        self.manager.cancel(started["job_id"])
        self.files.drain()
        replacement = self.manager.start(self.arguments(client_request_id="replacement"))
        self.manager.cancel(replacement["job_id"])
        self.files.drain()

    def test_concurrent_admission_reserves_only_one_recovery_slot(self):
        self.bounded_manager()
        barrier = threading.Barrier(2)
        results, errors = [], []
        def metadata(_):
            barrier.wait(timeout=5)
            return {}
        def submit(request_id):
            try:
                results.append(self.manager.start(self.arguments(client_request_id=request_id)))
            except Exception as error:
                errors.append(error)
        with mock.patch.object(jobs, "_processing_raster_band_counts", side_effect=metadata), \
             mock.patch.object(jobs, "_processing_input_versions", return_value=[]):
            threads = [threading.Thread(target=submit, args=(value,)) for value in ("one", "two")]
            for thread in threads:
                thread.start()
            for thread in threads:
                thread.join(timeout=5)
                self.assertFalse(thread.is_alive())
        self.assertEqual(len(results), 1)
        self.assertEqual(len(errors), 1)
        self.assertIn("processing_recovery_capacity_exceeded", str(errors[0]))
        self.manager.cancel(results[0]["job_id"])
        self.files.drain()

    def test_completed_task_owner_survives_expiry_until_destroyed(self):
        store, clock, _ = self.bounded_manager()
        started = self.manager.start(self.arguments())
        task, runtime = self.finish_algorithm(started)
        self.files.drain()
        clock.advance(11)
        store.purge()
        self.assertIs(store.get_runtime(started["job_id"], "vector"), runtime)
        self.assertIs(runtime.task, task)
        self.assertIsNotNone(runtime.context)
        task.destroyed.emit()
        store.purge()
        with self.assertRaises(JobNotFoundError):
            store.get(started["job_id"])

    def test_zero_terminal_history_still_has_one_admission_slot(self):
        self.bounded_manager(capacity=0)
        first = self.manager.start(self.arguments())
        with self.assertRaisesRegex(RuntimeError, "processing_recovery_capacity_exceeded"):
            self.manager.start(self.arguments(client_request_id="second"))
        self.manager.cancel(first["job_id"])
        self.files.drain()

    def test_recovery_capacity_is_category_local(self):
        self.bounded_manager()
        self.environment.registry.algorithms["raster:copy"] = FakeAlgorithm(
            "raster:copy", parameter_definitions=[
                FakeProcessingDefinition("INPUT", destination=False),
                FakeProcessingDefinition("OUTPUT", destination=True),
            ])
        vector = self.manager.start(self.arguments())
        raster = self.manager.start(self.arguments(
            category="raster", algorithm_id="raster:copy", client_request_id="raster",
            parameters={"INPUT": str(self.source), "OUTPUT": str(self.root / "raster.txt")}))
        self.assertNotEqual(vector["job_id"], raster["job_id"])
        for snapshot in (vector, raster):
            self.manager.cancel(snapshot["job_id"])
        self.files.drain()

    def drain_continuations(self, continuations):
        for _ in range(30):
            if continuations:
                continuations.pop(0)()
            elif self.files.pending:
                self.files.run()
            else:
                return
        self.fail("queued continuation lifecycle did not settle")

    def test_cancel_between_file_completion_and_queued_continuation(self):
        continuations = []
        self.manager._dependencies["continuation_scheduler"] = continuations.append
        started = self.manager.start(self.arguments())
        runtime = self.manager.store.get_runtime(started["job_id"], "vector")
        self.files.run()
        self.assertIsNone(runtime.file_task)
        self.assertEqual(runtime.pending_callbacks, 1)
        self.assertEqual(self.environment.tasks, [])
        stage = runtime.output_stages[0]["staging_root"]
        self.assertEqual(self.manager.cancel(started["job_id"])["state"], "cancelling")
        self.assertTrue(stage.exists())
        self.assertEqual(self.files.pending, [])
        self.drain_continuations(continuations)
        self.assertEqual(self.manager.get(started["job_id"])["state"], "cancelled")
        self.assertFalse(stage.exists())
        self.assertEqual(self.environment.tasks, [])

    def test_shutdown_retains_queued_algorithm_result_without_calling_old_task(self):
        continuations = []
        started = self.manager.start(self.arguments())
        self.files.drain()
        runtime = self.manager.store.get_runtime(started["job_id"], "vector")
        task = self.environment.tasks[-1]
        self.manager._dependencies["continuation_scheduler"] = continuations.append
        Path(runtime.parameters["OUTPUT"]).write_text("algorithm result")
        task.executed.emit(True, {"OUTPUT": runtime.parameters["OUTPUT"]})
        self.assertEqual(runtime.pending_callbacks, 1)
        self.assertEqual(self.files.pending, [])
        with mock.patch.object(task, "cancel", side_effect=AssertionError("completed wrapper cancel")), \
             mock.patch.object(task, "waitForFinished", side_effect=AssertionError("completed wrapper wait")):
            stopped = self.manager.shutdown(timeout_seconds=0)
        self.assertFalse(stopped["complete"])
        self.assertIn(started["job_id"], stopped["pending_job_ids"])
        self.assertIsNotNone(runtime.context)
        task.destroyed.emit()
        self.assertIsNotNone(runtime.context)
        self.drain_continuations(continuations)
        final = self.manager.get(started["job_id"])
        self.assertEqual(final["state"], "cancelled")
        self.assertTrue(final["cleanup"]["complete"])
        self.assertIsNone(runtime.context)
        self.assertFalse(self.target.exists())

    def test_expired_terminal_survives_until_queued_recovery_receipt_is_patched(self):
        store, clock, _ = self.bounded_manager()
        job_id, runtime, _ = self.retained_publication(store)
        continuations = []
        self.manager._dependencies["continuation_scheduler"] = continuations.append
        clock.advance(11)
        self.manager.get(job_id)
        self.files.run()
        self.assertIsNone(runtime.file_task)
        self.assertEqual(runtime.pending_callbacks, 1)
        self.assertIsNone(runtime.output_promotion_receipt)
        store.purge()
        self.assertIs(store.get_runtime(job_id, "vector"), runtime)
        self.assertFalse(store.get(job_id)["cleanup"]["complete"])
        self.drain_continuations(continuations)
        self.assertTrue(store.get(job_id)["cleanup"]["complete"])

    def test_duplicate_file_completion_cannot_clear_the_successor_task(self):
        continuations = []
        self.manager._dependencies["continuation_scheduler"] = continuations.append
        started = self.manager.start(self.arguments())
        runtime = self.manager.store.get_runtime(started["job_id"], "vector")
        _, first_done, _, _ = self.files.pending[0]
        self.files.run()
        continuations.pop(0)()
        task = self.environment.tasks[-1]
        Path(runtime.parameters["OUTPUT"]).write_text("algorithm result")
        task.executed.emit(True, {"OUTPUT": runtime.parameters["OUTPUT"]})
        continuations.pop(0)()
        successor = runtime.file_task
        self.assertIsNotNone(successor)
        first_done(None, None)
        self.assertIs(runtime.file_task, successor)
        self.drain_continuations(continuations)
        self.assertEqual(self.manager.get(started["job_id"])["state"], "succeeded")

    def test_rejected_terminal_continuation_preserves_state_and_retries_receipt(self):
        store, _, _ = self.bounded_manager()
        job_id, runtime, _ = self.retained_publication(store)
        self.manager._dependencies["continuation_scheduler"] = mock.Mock(
            side_effect=RuntimeError("event loop unavailable"))
        self.manager.get(job_id)
        self.files.run()
        snapshot = store.get(job_id)
        self.assertEqual(snapshot["state"], "failed")
        self.assertFalse(snapshot["cleanup"]["complete"])
        self.assertIn("event loop unavailable", str(snapshot["cleanup"]["errors"]))
        self.assertEqual(runtime.pending_callbacks, 0)
        self.assertIsNone(runtime.file_task)
        self.assertEqual(self.files.pending, [])
        self.assertEqual(self.target.read_bytes(), b"original survey")
        self.manager._dependencies["continuation_scheduler"] = lambda callback: callback()
        self.assertTrue(self.manager.get(job_id)["cleanup"]["complete"])

    def test_shutdown_after_commit_preserves_queued_success_result(self):
        started = self.manager.start(self.arguments())
        task, runtime = self.finish_algorithm(started)
        continuations = []
        self.manager._dependencies["continuation_scheduler"] = continuations.append
        self.files.run()  # Publish files and queue the main-thread continuation.
        continuations.pop(0)()  # Finish loading and schedule committed cleanup.
        task.destroyed.emit()
        self.files.run()  # Commit finished, success snapshot is still queued.
        self.assertEqual(runtime.pending_callbacks, 1)
        self.assertIsNone(runtime.task)
        self.assertIsNone(runtime.file_task)
        self.assertEqual(runtime.pending_terminal["state"], "succeeded")
        self.assertIsNone(runtime.output_promotion_receipt)
        self.assertEqual(self.target.read_text(), "completed survey")
        # Cleanup is irreversible and no longer accepts ordinary cancellation.
        with self.assertRaisesRegex(RuntimeError, "job_not_cancellable"):
            self.manager.cancel(started["job_id"])
        self.assertFalse(self.manager.store.get(started["job_id"])["cancel_requested"])
        stopped = self.manager.shutdown(timeout_seconds=0)
        self.assertFalse(stopped["complete"])
        self.assertIn(started["job_id"], stopped["pending_job_ids"])
        self.assertEqual(runtime.pending_terminal["state"], "succeeded")
        self.drain_continuations(continuations)
        final = self.manager.get(started["job_id"])
        self.assertEqual(final["state"], "succeeded")
        self.assertIsNotNone(final["result"])
        self.assertTrue(final["cleanup"]["complete"])
        self.assertEqual(self.target.read_text(), "completed survey")
        self.assertEqual(self.manager.shutdown(timeout_seconds=0),
                         {"complete": True, "pending_job_ids": []})


if __name__ == "__main__":
    unittest.main()
