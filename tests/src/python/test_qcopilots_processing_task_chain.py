"""Real QgsTask regression for queued QCopilots Processing continuations.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

import hashlib
import inspect
import os
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest


_DLL_DIRECTORY_HANDLES = []
if sys.platform == "win32":
    prefix = os.environ.get("QGIS_PREFIX_PATH", "")
    if prefix:
        bin_root = Path(prefix).parent
        if bin_root.is_dir():
            for candidate in bin_root.iterdir():
                if (candidate / "qgis_core.dll").is_file():
                    os.environ["PATH"] = str(candidate) + os.pathsep + os.environ.get("PATH", "")
                    if hasattr(os, "add_dll_directory"):
                        _DLL_DIRECTORY_HANDLES.append(os.add_dll_directory(str(candidate)))

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python" / "plugins"))

from qgis.core import QgsApplication, QgsTask
from qgis.PyQt.QtCore import QCoreApplication, QEvent
from qgis.testing import QgisTestCase, start_app
from qcopilots_common.processing_file_worker import make_file_task
from qcopilots_common.processing_jobs import ProcessingJobManager, _ProcessingRuntime


QGIS_APP = start_app()


class _SimpleTask(QgsTask):
    def __init__(self, label, function, completed):
        super().__init__(label, QgsTask.Flag.CanCancel)
        self.function = function
        self.completed = completed
        self.value = None
        self.error = None

    def run(self):
        try:
            self.value = self.function()
            return True
        except Exception as error:
            self.error = error
            return False

    def finished(self, successful):
        error = self.error
        if not successful and error is None:
            error = RuntimeError("Task did not finish successfully")
        self.completed(error, self.value)


class TestQCopilotsProcessingTaskChain(QgisTestCase):
    def setUp(self):
        super().setUp()
        self.tasks = QgsApplication.taskManager()
        self.assertFalse(self.tasks.activeTasks())
        self.pool = self.tasks.threadPool()
        self.old_thread_count = self.pool.maxThreadCount()
        self.pool.setMaxThreadCount(1)
        self.processing = ProcessingJobManager()
        self.main_thread = threading.get_ident()
        self.gates = []
        self.owned_tasks = []
        self.runtimes = []
        self.closing = False
        self.temp = tempfile.TemporaryDirectory(prefix="qcopilots-task-chain-")
        self.root = Path(self.temp.name).resolve()
        self.root.relative_to(Path(tempfile.gettempdir()).resolve())
        self.fixture = self.root / "survey-block.bin"
        self.fixture.write_bytes(bytes(range(256)) * 256)
        self.expected_hash = hashlib.sha256(self.fixture.read_bytes()).hexdigest()

    def tearDown(self):
        try:
            self.closing = True
            for event in self.gates:
                event.set()
            for task in self.owned_tasks:
                try:
                    task.cancel()
                except RuntimeError:
                    pass
            # An old task may be gone while its queued continuation still owns
            # a runtime. Drain those callbacks before deleting the fixture.
            QCoreApplication.processEvents()
            def settled():
                return not self.tasks.activeTasks() and all(
                    not runtime.pending_callbacks and not runtime.file_callbacks
                    for runtime in self.runtimes
                )
            self.assertTrue(self._pump_until(settled, 5))
            self.assertTrue(self.pool.waitForDone(5000))
            QCoreApplication.processEvents()
            self.assertTrue(settled())
            for runtime in self.runtimes:
                runtime.file_task = None
            QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
            self.temp.cleanup()
        finally:
            self.pool.setMaxThreadCount(self.old_thread_count)
            super().tearDown()

    @staticmethod
    def _pump_until(predicate, seconds=3):
        deadline = time.monotonic() + seconds
        while not predicate():
            QCoreApplication.processEvents()
            if time.monotonic() >= deadline:
                return False
            time.sleep(0.001)
        return True

    @staticmethod
    def _finished_depth(task):
        frame = inspect.currentframe()
        count = 0
        try:
            while frame is not None:
                if frame.f_code.co_name == "finished" and frame.f_locals.get("self") is task:
                    count += 1
                frame = frame.f_back
            return count
        finally:
            del frame

    def _make_task(self, kind, function, completed):
        if kind == "file":
            task = make_file_task(function, completed)
        else:
            task = _SimpleTask("QCopilots continuation regression", function, completed)
        self.owned_tasks.append(task)
        return task

    def _run_chain(self, kind):
        self.assertFalse(self.tasks.activeTasks())
        QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
        release_predecessor = threading.Event()
        gate_entered = threading.Event()
        release_gate = threading.Event()
        self.gates.extend((release_predecessor, release_gate))
        state = {"registered": False, "successor_done": False, "gate_done": False,
                 "errors": [], "worker_threads": []}
        context_sentinel = object()
        runtime = _ProcessingRuntime(None, {}, context_sentinel, None, False, 0)
        self.runtimes.append(runtime)

        def work():
            state["worker_threads"].append(threading.get_ident())
            if kind == "file":
                return hashlib.sha256(self.fixture.read_bytes()).hexdigest()
            return "simple task result"

        def predecessor_work():
            if not release_predecessor.wait(3):
                raise TimeoutError("Predecessor barrier expired")
            return work()

        def successor_complete(error, value):
            def record_completion():
                state["successor_done"] = True
                if error is not None:
                    state["errors"].append(error)
                expected = self.expected_hash if kind == "file" else "simple task result"
                if value != expected:
                    state["errors"].append(AssertionError("Unexpected successor result"))
            self.processing._defer_continuation(runtime, record_completion, release_file_task=True)

        def register_successor():
            if self.closing:
                state["successor_done"] = True
                return
            try:
                self.assertEqual(threading.get_ident(), self.main_thread)
                self.assertEqual(self._finished_depth(state["predecessor"]), 0)
                self.assertIsNone(self.tasks.task(state["predecessor_id"]))
                self.assertTrue(gate_entered.is_set())
                self.assertFalse(release_gate.is_set())
                self.assertEqual(runtime.pending_callbacks, 0)
                self.assertEqual(runtime.file_callbacks, 1)
                self.assertIsNone(runtime.file_task)
                successor = self._make_task(kind, work, successor_complete)
                runtime.file_task = successor
                state["successor"] = successor
                self.assertTrue(self.tasks.addTask(successor))
                self.assertEqual(successor.status(), QgsTask.TaskStatus.Queued)
                state["registered"] = True
            except Exception as error:
                state["errors"].append(error)
                release_gate.set()

        def predecessor_complete(error, value):
            try:
                if error is not None:
                    raise error
                self.assertGreater(self._finished_depth(state["predecessor"]), 0)
                self.processing._defer_continuation(runtime, register_successor, release_file_task=True)
                self.assertFalse(state["registered"])
                self.assertEqual(runtime.pending_callbacks, 1)
                self.assertIsNone(runtime.file_task)
                runtime.release_completed_objects(keep_context=False)
                self.assertIs(runtime.context, context_sentinel)
            except Exception as error:
                state["errors"].append(error)
                release_gate.set()

        def gate_work():
            gate_entered.set()
            if not release_gate.wait(5):
                raise TimeoutError("Worker gate expired")
            return True

        def gate_complete(error, value):
            state["gate_done"] = True
            if error is not None:
                state["errors"].append(error)

        predecessor = self._make_task(kind, predecessor_work, predecessor_complete)
        state["predecessor"] = predecessor
        runtime.file_task = predecessor
        gate = _SimpleTask("QCopilots worker gate", gate_work, gate_complete)
        self.owned_tasks.append(gate)
        state["predecessor_id"] = self.tasks.addTask(predecessor)
        self.assertTrue(state["predecessor_id"])
        self.assertTrue(self.tasks.addTask(gate))
        release_predecessor.set()
        # The gate starts after the old runnable returns. Process its queued
        # finished signal only once the sole worker is occupied by this gate.
        self.assertTrue(gate_entered.wait(3))
        self.assertTrue(self._pump_until(lambda: state["registered"] or state["errors"]))
        self.assertFalse(state["errors"], repr(state["errors"]))
        release_gate.set()
        self.assertTrue(self._pump_until(lambda: state["successor_done"] and state["gate_done"]))
        self.assertFalse(state["errors"], repr(state["errors"]))
        self.assertTrue(self._pump_until(lambda: not self.tasks.activeTasks()))
        self.assertEqual(runtime.pending_callbacks, 0)
        self.assertEqual(runtime.file_callbacks, 0)
        self.assertIsNone(runtime.file_task)
        self.assertEqual(len(state["worker_threads"]), 2)
        self.assertTrue(all(ident != self.main_thread for ident in state["worker_threads"]))
        runtime.release_completed_objects(keep_context=False)
        self.assertIsNone(runtime.context)

    def test_simple_task_continuations_register_after_old_finished_returns(self):
        for iteration in range(20):
            with self.subTest(iteration=iteration):
                self._run_chain("simple")

    def test_file_task_continuations_register_after_old_finished_returns(self):
        for iteration in range(20):
            with self.subTest(iteration=iteration):
                self._run_chain("file")

    def test_teardown_drains_a_pending_continuation_before_removing_fixture(self):
        runtime = _ProcessingRuntime(None, {}, object(), None, False, 0)
        self.runtimes.append(runtime)
        observations = []

        def resume_during_cleanup():
            observations.append((self.closing, self.fixture.exists()))

        def verify_cleanup():
            self.assertEqual(observations, [(True, True)])
            self.assertEqual(runtime.pending_callbacks, 0)
            self.assertEqual(runtime.file_callbacks, 0)
            self.assertIsNone(runtime.file_task)
            self.assertFalse(self.fixture.exists())

        self.addCleanup(verify_cleanup)
        self.processing._defer_continuation(runtime, resume_during_cleanup)
        self.assertEqual(runtime.pending_callbacks, 1)
        self.assertFalse(observations)


if __name__ == "__main__":
    unittest.main()
