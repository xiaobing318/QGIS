"""Historical attempt receipts and current transfer recovery capabilities."""

import json
import threading
import unittest

import test_qcopilots_mcp_server_file_transfer_jobs as jobs_tests

from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError


class TestFileTransferAttemptCapabilities(unittest.TestCase):
    setUp = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.setUp
    new_manager = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.new_manager
    close_managers = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.close_managers
    assert_no_worker_exceptions = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.assert_no_worker_exceptions
    source = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.source
    wait = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.wait
    upload = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.upload
    assert_success = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.assert_success
    interrupted_upload = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.interrupted_upload

    def _cancelled_upload(self):
        source = self.source()
        self.backend.write_gate = threading.Event()
        first = self.upload([source])
        self.assertTrue(self.backend.write_observed.wait(2))
        self.manager.cancel_job(first["job_id"])
        cancelled = self.wait(first)
        self.assertEqual(cancelled["state"], "cancelled", cancelled)
        self.assertTrue(cancelled["resumable"])
        self.backend.write_gate = None
        return source, cancelled

    def _assert_capability(self, previous, expected):
        current = self.manager.get_job(previous["job_id"])
        self.assertEqual(current["resumable"], expected)
        for field in ("state", "phase", "progress", "created_at", "updated_at", "finished_at", "errors", "files"):
            self.assertEqual(current[field], previous[field], field)
        listed = next(item for item in self.manager.list_jobs()["jobs"]
                      if item["job_id"] == previous["job_id"])
        self.assertEqual(listed["resumable"], expected)
        self.assertEqual(listed["state"], previous["state"])
        # Repeated cancellation of a terminal attempt also returns current flags.
        cancelled_again = self.manager.cancel_job(previous["job_id"])
        self.assertEqual(cancelled_again["resumable"], expected)
        self.assertEqual(cancelled_again["state"], previous["state"])

    def _receipt_bytes(self, job_id):
        return {
            suffix: (self.manager.history_root / (job_id + suffix)).read_bytes()
            for suffix in (".files.jsonl", ".files.offsets.json")
        }

    def _second_failed_attempt(self, first):
        self.backend.disconnect_after_bytes = self.backend.bytes_written + 1
        second = self.wait(self.manager.resume_job(first["job_id"]))
        self.assertEqual(second["state"], "failed", second)
        self.assertTrue(second["resumable"])
        return second

    def test_old_cancelled_attempt_is_not_resumable_while_new_attempt_runs(self):
        _source, first = self._cancelled_upload()
        self.backend.write_observed.clear()
        gate = self.backend.write_gate = threading.Event()
        second = self.manager.resume_job(first["job_id"])
        self.assertTrue(self.backend.write_observed.wait(2))
        self._assert_capability(first, False)
        self.assertFalse(gate.is_set())
        with self.assertRaises(FileTransferError) as caught:
            self.manager.resume_job(first["job_id"])
        self.assertEqual(caught.exception.code, "transfer_busy")
        self.assertEqual(self.manager.get_job(second["job_id"])["state"], "running")
        gate.set()
        self.assert_success(second)

    def test_success_disables_old_cancelled_attempt_without_rewriting_history(self):
        _source, first = self._cancelled_upload()
        receipts = self._receipt_bytes(first["job_id"])
        self.assert_success(self.manager.resume_job(first["job_id"]))
        self._assert_capability(first, False)
        with self.assertRaises(FileTransferError) as caught:
            self.manager.resume_job(first["job_id"])
        self.assertEqual(caught.exception.code, "transfer_complete")
        self.assertEqual(self._receipt_bytes(first["job_id"]), receipts)
        history = json.loads((self.manager.history_root / (first["transfer_id"] + ".json")).read_text(encoding="utf-8"))
        original = next(item for item in history["attempts"] if item["job_id"] == first["job_id"])
        self.assertTrue(original["resumable"], "The historical attempt snapshot must remain untouched")
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()
        self._assert_capability(first, False)
        self.assertEqual(self._receipt_bytes(first["job_id"]), receipts)

    def test_old_attempt_can_resume_when_newer_attempt_failed_again(self):
        source, first = self.interrupted_upload()
        self._second_failed_attempt(first)
        self._assert_capability(first, True)
        recovered = self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertEqual(recovered["resumes_job_id"], first["job_id"])
        self.assertGreater(recovered["progress"]["bytes_reused"], 0)
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())
        self._assert_capability(first, False)

    def test_discard_disables_all_old_attempts_during_cleanup_retry_and_after_restart(self):
        _source, first = self.interrupted_upload()
        second = self._second_failed_attempt(first)
        receipts = self._receipt_bytes(first["job_id"])
        self.backend.fail_remove = True
        discarded = self.manager.discard_transfer(first["transfer_id"])
        self.assertTrue(discarded["cleanup_pending"])
        self._assert_capability(first, False)
        self.assertFalse(self.manager.get_job(second["job_id"])["resumable"])
        with self.assertRaises(FileTransferError) as caught:
            self.manager.resume_job(first["job_id"])
        self.assertEqual(caught.exception.code, "transfer_not_resumable")
        self.backend.fail_remove = False
        cleaned = self.manager.discard_transfer(first["transfer_id"])
        self.assertFalse(cleaned["cleanup_pending"])
        self.assertEqual(self._receipt_bytes(first["job_id"]), receipts)
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()
        self._assert_capability(first, False)
        self.assertFalse(self.manager.get_job(second["job_id"])["resumable"])
        self.assertEqual(self.backend.partials(), [])
        self.assertEqual(self._receipt_bytes(first["job_id"]), receipts)


if __name__ == "__main__":
    unittest.main()
