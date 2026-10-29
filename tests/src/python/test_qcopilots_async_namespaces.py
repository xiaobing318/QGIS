"""Regression tests for operation-scoped QCopilots job idempotency.

These storage tests do not require a QGIS application or provider registry.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

import sys
import threading
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python" / "plugins"))

from qcopilots_common.async_jobs import AsyncJobStore, IdempotencyConflictError


class TestAsyncJobNamespaces(unittest.TestCase):
    def create(
        self, store, namespace="", *, scope="vector", fingerprint="same", runtime=None
    ):
        return store.create(
            scope,
            {"state": "queued", "client_request_id": "request"},
            client_request_id="request",
            request_fingerprint=fingerprint,
            idempotency_namespace=namespace,
            runtime=runtime,
        )

    def lookup(self, store, namespace="", *, scope="vector", fingerprint="same"):
        return store.lookup_idempotent(
            scope,
            "request",
            fingerprint,
            idempotency_namespace=namespace,
        )

    def finish(self, store, snapshot):
        store.transition(snapshot["job_id"], "running")
        return store.transition(snapshot["job_id"], "succeeded")

    def test_preview_and_execute_replay_independently(self):
        store = AsyncJobStore()
        preview, preview_created = self.create(store, "preview")
        execution, execution_created = self.create(store, "execute")
        self.assertTrue(preview_created)
        self.assertTrue(execution_created)
        self.assertNotEqual(preview["job_id"], execution["job_id"])
        self.assertEqual(self.create(store, "preview"), (preview, False))
        self.assertEqual(self.create(store, "execute"), (execution, False))
        self.assertNotIn("idempotency_namespace", preview)
        self.assertNotIn("idempotency_namespace", execution)

    def test_default_namespace_and_scope_remain_independent(self):
        store = AsyncJobStore()
        original, created = store.create(
            "binary",
            {"state": "queued"},
            client_request_id="request",
            request_fingerprint="same",
        )
        self.assertTrue(created)
        self.assertEqual(
            store.lookup_idempotent("binary", "request", "same"), original
        )
        self.assertEqual(self.lookup(store, scope="binary"), original)
        other_scope, created = self.create(store)
        self.assertTrue(created)
        self.assertNotEqual(other_scope["job_id"], original["job_id"])
        other_namespace, created = self.create(store, "execute", scope="binary")
        self.assertTrue(created)
        self.assertNotEqual(other_namespace["job_id"], original["job_id"])

    def test_conflicts_are_local_to_namespace(self):
        store = AsyncJobStore()
        self.create(store, "preview")
        with self.assertRaises(IdempotencyConflictError):
            self.lookup(store, "preview", fingerprint="changed")
        with self.assertRaises(IdempotencyConflictError):
            self.create(store, "preview", fingerprint="changed")
        execution, created = self.create(store, "execute", fingerprint="changed")
        self.assertTrue(created)
        self.assertEqual(
            self.lookup(store, "execute", fingerprint="changed"), execution
        )

    def test_preview_eviction_keeps_execute_idempotency(self):
        store = AsyncJobStore(max_terminal_jobs_per_scope=1)
        preview, _ = self.create(store, "preview")
        execution, _ = self.create(store, "execute")
        self.finish(store, preview)
        terminal_execution = self.finish(store, execution)
        self.assertIsNone(self.lookup(store, "preview"))
        self.assertEqual(self.lookup(store, "execute"), terminal_execution)
        self.assertEqual(
            self.create(store, "execute"), (terminal_execution, False)
        )

    def test_execute_eviction_keeps_preview_idempotency(self):
        store = AsyncJobStore(max_terminal_jobs_per_scope=1)
        execution, _ = self.create(store, "execute")
        preview, _ = self.create(store, "preview")
        self.finish(store, execution)
        terminal_preview = self.finish(store, preview)
        self.assertIsNone(self.lookup(store, "execute"))
        self.assertEqual(self.lookup(store, "preview"), terminal_preview)

    def test_expiry_uses_original_key_not_mutable_snapshot(self):
        clock = [0.0]
        store = AsyncJobStore(terminal_ttl_seconds=10, now=lambda: clock[0])
        preview, _ = self.create(store, "preview")
        store.patch(preview["job_id"], client_request_id="unrelated")
        self.finish(store, preview)
        clock[0] = 5.0
        execution, _ = self.create(store, "execute")
        terminal_execution = self.finish(store, execution)
        clock[0] = 11.0
        store.purge()
        self.assertNotIn(("vector", "preview", "request"), store._idempotency)
        self.assertEqual(self.lookup(store, "execute"), terminal_execution)

    def test_missing_job_invalidates_only_its_namespace(self):
        store = AsyncJobStore()
        preview, _ = self.create(store, "preview")
        execution, _ = self.create(store, "execute")
        # Exercise stale-index recovery without changing the other namespace.
        store._jobs.pop(preview["job_id"])
        self.assertIsNone(self.lookup(store, "preview"))
        self.assertEqual(self.lookup(store, "execute"), execution)

    def test_concurrent_creations_have_one_winner_per_namespace(self):
        store = AsyncJobStore()
        barrier = threading.Barrier(8)
        results = []
        errors = []
        lock = threading.Lock()

        def create(namespace):
            try:
                barrier.wait(timeout=5)
                snapshot, created = self.create(store, namespace)
                with lock:
                    results.append((namespace, snapshot["job_id"], created))
            except Exception as error:
                with lock:
                    errors.append(error)

        threads = [
            threading.Thread(target=create, args=(namespace,))
            for namespace in ("preview", "execute")
            for _ in range(4)
        ]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join(timeout=10)
            self.assertFalse(thread.is_alive())
        self.assertEqual(errors, [])
        self.assertEqual(sum(created for _, _, created in results), 2)
        for namespace in ("preview", "execute"):
            self.assertEqual(
                len({job for mode, job, _ in results if mode == namespace}), 1
            )

    def test_expired_job_retains_runtime_until_recovery_worker_finishes(self):
        clock = [0.0]
        runtime = {"worker_active": True}
        store = AsyncJobStore(
            now=lambda: clock[0],
            terminal_ttl_seconds=1,
            on_evict=lambda _job_id, value: {
                "complete": not value["worker_active"],
                "defer_eviction": value["worker_active"],
            },
        )
        preview, _ = self.create(store, "preview", runtime=runtime)
        terminal = self.finish(store, preview)
        clock[0] = 2.0
        store.purge()
        self.assertIs(store.get_runtime(preview["job_id"]), runtime)
        self.assertEqual(self.lookup(store, "preview"), terminal)
        self.assertEqual(store.cleanup_audits("vector"), [])

        runtime["worker_active"] = False
        store.purge()
        self.assertIsNone(self.lookup(store, "preview"))
        self.assertNotIn(preview["job_id"], store._jobs)

    def test_terminal_limit_skips_active_recovery_without_blocking_other_evictions(self):
        active_runtime = object()
        attempts = []

        def cleanup(job_id, runtime):
            attempts.append(job_id)
            return {"complete": True, "defer_eviction": runtime is active_runtime}

        store = AsyncJobStore(max_terminal_jobs_per_scope=1, on_evict=cleanup)
        preview, _ = self.create(store, "preview", runtime=active_runtime)
        self.finish(store, preview)
        execution, _ = self.create(store, "execute")
        attempts.clear()
        self.finish(store, execution)
        self.assertEqual(attempts, [preview["job_id"], execution["job_id"]])
        self.assertIsNotNone(self.lookup(store, "preview"))
        self.assertIsNone(self.lookup(store, "execute"))
        self.assertEqual(len(store.list("vector")), 1)

    def test_existing_cleanup_failure_still_evicts_with_bounded_audit(self):
        store = AsyncJobStore(
            max_terminal_jobs_per_scope=0,
            on_evict=lambda _job_id, _runtime: {
                "complete": False,
                "residual_paths": ["locked-output"],
                "errors": ["file locked"],
            },
        )
        original, _ = self.create(store, scope="binary")
        self.finish(store, original)
        self.assertIsNone(self.lookup(store, scope="binary"))
        audit = store.cleanup_audits("binary")
        self.assertEqual(len(audit), 1)
        self.assertEqual(audit[0]["cleanup"]["residual_paths"], ["locked-output"])
        self.assertTrue(audit[0]["cleanup"]["eviction_cleanup_failed"])


if __name__ == "__main__":
    unittest.main()
