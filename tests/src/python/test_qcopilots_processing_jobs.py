"""QGIS unit tests for QCopilots asynchronous Processing jobs.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-08-02"
__copyright__ = "Copyright 2026, The QGIS Project"

import json
import os
import sys
import threading
import time
import unittest
import uuid
from datetime import date, datetime, timezone
from enum import Enum
from pathlib import Path
from unittest import mock


_QGIS_DLL_DIRECTORY_HANDLES = []


def _prepare_windows_qgis_dll_search_path():
    """Add the active multi-config QGIS build directory before QGIS imports."""

    if sys.platform != "win32":
        return

    prefix_path = os.environ.get("QGIS_PREFIX_PATH", "")
    bin_root = Path(prefix_path).parent if prefix_path else None
    if bin_root is None or not bin_root.is_dir():
        return

    for candidate in sorted(bin_root.iterdir()):
        if not (candidate / "qgis_core.dll").is_file():
            continue
        os.environ["PATH"] = f"{candidate}{os.pathsep}{os.environ.get('PATH', '')}"
        if hasattr(os, "add_dll_directory"):
            _QGIS_DLL_DIRECTORY_HANDLES.append(os.add_dll_directory(str(candidate)))
        return


_prepare_windows_qgis_dll_search_path()

from processing.core.Processing import Processing
from qgis.core import (
    Qgis,
    QgsApplication,
    QgsCoordinateReferenceSystem,
    QgsFeature,
    QgsFeatureSink,
    QgsFields,
    QgsGeometry,
    QgsPointXY,
    QgsProcessingAlgorithm,
    QgsProcessingException,
    QgsProcessingParameterFeatureSink,
    QgsProcessingProvider,
    QgsProject,
)
from qgis.PyQt.QtCore import QCoreApplication, QThread
from qgis.testing import QgisTestCase, start_app

from qcopilots_common.async_jobs import (
    AsyncJobStore,
    IdempotencyConflictError,
    InvalidJobTransitionError,
    JobNotFoundError,
    PUBLIC_JOB_STATES,
)
from qcopilots_common.processing_jobs import ProcessingJobManager, json_safe_value


QGIS_APP = start_app()


class FakeClock:
    def __init__(self, value=100.0):
        self.value = float(value)

    def __call__(self):
        return self.value

    def advance(self, seconds):
        self.value += float(seconds)


class FakeSignal:
    def __init__(self):
        self.callbacks = []

    def connect(self, callback):
        self.callbacks.append(callback)

    def emit(self, *arguments):
        for callback in list(self.callbacks):
            callback(*arguments)


class FakeFeedback:
    def __init__(self):
        self.progressChanged = FakeSignal()
        self.processedCountChanged = FakeSignal()
        self.progressTextChanged = FakeSignal()
        self.sinkFeatureCountChanged = FakeSignal()
        self.errorReported = FakeSignal()
        self.warningPushed = FakeSignal()
        self.infoPushed = FakeSignal()
        self.commandInfoPushed = FakeSignal()
        self.debugInfoPushed = FakeSignal()
        self.consoleInfoPushed = FakeSignal()
        self.cancelled = False

    def isCanceled(self):
        return self.cancelled


class FakeTask:
    def __init__(self, *, cancellable=True):
        self.begun = FakeSignal()
        self.executed = FakeSignal()
        self.taskTerminated = FakeSignal()
        self.cancellable = cancellable
        self.cancelled = False
        self.cancel_calls = 0
        self.wait_calls = []

    def cancel(self):
        self.cancel_calls += 1
        self.cancelled = True

    def isCanceled(self):
        return self.cancelled

    def waitForFinished(self, timeout):
        self.wait_calls.append(timeout)
        return True


class FakeTaskManager:
    def __init__(self):
        self.tasks = []

    def addTask(self, task):
        self.tasks.append(task)
        return True


class FakeProvider:
    def __init__(self, provider_id):
        self.provider_id = provider_id

    def id(self):
        return self.provider_id


class FakeAlgorithm:
    def __init__(self, algorithm_id, *, no_threading=False, cancellable=True):
        self.algorithm_id = algorithm_id
        self.no_threading = no_threading
        self.cancellable = cancellable

    def id(self):
        return self.algorithm_id

    def name(self):
        return self.algorithm_id.partition(":")[2]

    def displayName(self):
        return f"Test {self.name()}"

    def group(self):
        return "Test algorithms"

    def provider(self):
        return FakeProvider(self.algorithm_id.partition(":")[0])

    def parameterDefinitions(self):
        return []


class FakeRegistry:
    def __init__(self, algorithms):
        self.algorithms = {algorithm.id(): algorithm for algorithm in algorithms}

    def createAlgorithmById(self, algorithm_id):
        return self.algorithms.get(algorithm_id)


class FakeMapLayer:
    def id(self):
        return "layer-1"

    def name(self):
        return "Generated layer"

    def source(self):
        return "memory:generated"

    def providerType(self):
        return "memory"

    def type(self):
        return "VectorLayer"


class FakeEnum(Enum):
    VALUE = "enum-value"


class FakeProcessingEnvironment:
    def __init__(self, algorithms):
        self.registry = FakeRegistry(algorithms)
        self.task_manager = FakeTaskManager()
        self.feedback = []
        self.tasks = []
        self.single_shots = []
        self.main_thread_calls = []
        self.postprocess_calls = []
        self.initialized = 0

    def dependencies(self):
        def feedback_factory():
            feedback = FakeFeedback()
            self.feedback.append(feedback)
            return feedback

        def task_factory(algorithm, parameters, context, feedback):
            del parameters, context, feedback
            task = FakeTask(cancellable=algorithm.cancellable)
            self.tasks.append(task)
            return task

        def main_thread_runner(algorithm, parameters, context, feedback):
            self.main_thread_calls.append(
                (algorithm.id(), dict(parameters), context, feedback)
            )
            return {"OUTPUT": FakeMapLayer(), "COUNT": 4}

        def post_processor(algorithm, context, feedback, results):
            self.postprocess_calls.append(
                (algorithm.id(), context, feedback, dict(results))
            )
            return True

        return {
            "ensure_initialized": self._initialize,
            "registry": self.registry,
            "context_factory": lambda feedback: {"feedback": feedback},
            "feedback_factory": feedback_factory,
            "task_factory": task_factory,
            "task_manager": self.task_manager,
            "single_shot": lambda delay, callback: self.single_shots.append(
                (delay, callback)
            ),
            "main_thread_runner": main_thread_runner,
            "post_processor": post_processor,
            "parameter_sanitizer": lambda parameters, _: dict(parameters),
            "category_matcher": lambda algorithm, category: (
                algorithm.id().startswith(f"{category}:")
            ),
            "prepare_outputs": lambda algorithm, parameters, add_outputs: dict(
                parameters
            ),
            "is_no_threading": lambda algorithm: algorithm.no_threading,
            "is_task_cancellable": lambda task, algorithm: (
                task.cancellable and algorithm.cancellable
            ),
        }

    def _initialize(self):
        self.initialized += 1


class QCopilotsProgressAlgorithm(QgsProcessingAlgorithm):
    OUTPUT = "OUTPUT"
    progress_ready = threading.Event()
    release = threading.Event()

    @classmethod
    def reset_events(cls):
        cls.progress_ready = threading.Event()
        cls.release = threading.Event()

    def createInstance(self):
        return QCopilotsProgressAlgorithm()

    def name(self):
        return "vector_progress"

    def displayName(self):
        return "QCopilots vector progress test"

    def group(self):
        return "QCopilots vector tests"

    def groupId(self):
        return "qcopilots_vector_tests"

    def flags(self):
        return super().flags() | Qgis.ProcessingAlgorithmFlag.CanCancel

    def initAlgorithm(self, config=None):
        del config
        self.addParameter(
            QgsProcessingParameterFeatureSink(
                self.OUTPUT,
                "Generated output",
            )
        )

    def processAlgorithm(self, parameters, context, feedback):
        fields = QgsFields()
        sink, destination = self.parameterAsSink(
            parameters,
            self.OUTPUT,
            context,
            fields,
            Qgis.WkbType.Point,
            QgsCoordinateReferenceSystem("EPSG:4326"),
        )
        if sink is None:
            raise QgsProcessingException("Could not create controlled output sink")

        feedback.setProgress(42.0)
        feedback.setProgressText("Controlled progress reached")
        for index in range(3):
            feature = QgsFeature(fields)
            feature.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(index, index)))
            if not sink.addFeature(feature, QgsFeatureSink.Flag.FastInsert):
                raise QgsProcessingException("Could not add controlled feature")
            feedback.featureAddedToSink(self.OUTPUT)
        feedback.featureSinkFinalized(self.OUTPUT)
        type(self).progress_ready.set()
        type(self).release.wait(5)
        feedback.setProgress(100.0)
        return {self.OUTPUT: destination}


class QCopilotsMainThreadAlgorithm(QgsProcessingAlgorithm):
    executions = 0
    ran_on_application_thread = False

    @classmethod
    def reset_execution(cls):
        cls.executions = 0
        cls.ran_on_application_thread = False

    def createInstance(self):
        return QCopilotsMainThreadAlgorithm()

    def name(self):
        return "vector_main_thread"

    def displayName(self):
        return "QCopilots vector main thread test"

    def group(self):
        return "QCopilots vector tests"

    def groupId(self):
        return "qcopilots_vector_tests"

    def flags(self):
        return super().flags() | QgsProcessingAlgorithm.Flag.FlagNoThreading

    def initAlgorithm(self, config=None):
        del config

    def processAlgorithm(self, parameters, context, feedback):
        del parameters, context
        type(self).executions += 1
        application = QCoreApplication.instance()
        type(self).ran_on_application_thread = (
            application is not None
            and QThread.currentThread() == application.thread()
        )
        feedback.setProgress(100.0)
        return {"VALUE": "main-thread-complete"}


class QCopilotsFailingAlgorithm(QgsProcessingAlgorithm):
    def createInstance(self):
        return QCopilotsFailingAlgorithm()

    def name(self):
        return "vector_failure"

    def displayName(self):
        return "QCopilots vector failure test"

    def group(self):
        return "QCopilots vector tests"

    def groupId(self):
        return "qcopilots_vector_tests"

    def initAlgorithm(self, config=None):
        del config

    def processAlgorithm(self, parameters, context, feedback):
        del parameters, context
        feedback.setProgressText("About to fail in a controlled way")
        raise QgsProcessingException("controlled Processing failure")


class QCopilotsProcessingJobsProvider(QgsProcessingProvider):
    PROVIDER_ID = "qcopilots_processing_jobs_test"

    def id(self):
        return self.PROVIDER_ID

    def name(self):
        return "QCopilots Processing jobs tests"

    def longName(self):
        return self.name()

    def loadAlgorithms(self):
        self.addAlgorithm(QCopilotsProgressAlgorithm())
        self.addAlgorithm(QCopilotsMainThreadAlgorithm())
        self.addAlgorithm(QCopilotsFailingAlgorithm())


def job_snapshot(index, *, state="queued", cancellable=True, client_request_id=None):
    return {
        "state": state,
        "execution_mode": "qgs_task",
        "cancellable": cancellable,
        "client_request_id": client_request_id,
        "algorithm": {
            "id": f"native:test_{index}",
            "name": f"test_{index}",
            "display_name": f"Test {index}",
            "group": "tests",
            "provider": "native",
        },
        "progress_percent": None,
        "progress_text": None,
        "processed_count": None,
        "cancel_requested": False,
        "created_at": f"2026-08-02T00:00:{index:02d}.000Z",
        "started_at": None,
        "finished_at": None,
        "error": None,
        "nested": {"values": [index]},
    }


class TestAsyncJobStore(unittest.TestCase):
    def test_state_machine_returns_isolated_snapshots_and_freezes_terminal_job(self):
        runtime = object()
        store = AsyncJobStore()
        original = job_snapshot(1)
        created, is_new = store.create("vector", original, runtime=runtime)

        self.assertTrue(is_new)
        self.assertEqual(uuid.UUID(created["job_id"]).version, 4)
        self.assertEqual(created["category"], "vector")
        self.assertIs(store.get_runtime(created["job_id"], "vector"), runtime)

        original["nested"]["values"].append("caller mutation")
        created["nested"]["values"].append("response mutation")
        self.assertEqual(store.get(created["job_id"])["nested"]["values"], [1])

        running = store.transition(
            created["job_id"],
            "running",
            "vector",
            started_at="2026-08-02T00:01:00.000Z",
        )
        self.assertEqual(running["state"], "running")
        self.assertEqual(running["started_at"], "2026-08-02T00:01:00.000Z")
        progressed = store.patch(
            created["job_id"],
            "vector",
            progress_percent=37.5,
            progress_text="Working",
            processed_count=12,
        )
        self.assertEqual(
            (
                progressed["progress_percent"],
                progressed["progress_text"],
                progressed["processed_count"],
            ),
            (37.5, "Working", 12),
        )

        cancelling, cancel_runtime, requested = store.request_cancel(
            created["job_id"], "vector"
        )
        self.assertTrue(requested)
        self.assertIs(cancel_runtime, runtime)
        self.assertEqual(cancelling["state"], "cancelling")
        self.assertTrue(cancelling["cancel_requested"])

        cancelled = store.transition(
            created["job_id"],
            "cancelled",
            "vector",
            finished_at="2026-08-02T00:02:00.000Z",
        )
        self.assertEqual(cancelled["state"], "cancelled")

        # A late task callback must not rewrite a terminal snapshot.
        late_patch = store.patch(created["job_id"], "vector", progress_percent=99.0)
        late_success = store.transition(
            created["job_id"], "succeeded", "vector", result={"OUTPUT": "late"}
        )
        self.assertEqual(late_patch, cancelled)
        self.assertEqual(late_success, cancelled)
        self.assertNotIn("result", store.get(created["job_id"], "vector"))

        with self.assertRaisesRegex(ValueError, "Use transition"):
            store.patch(created["job_id"], state="failed")

    def test_public_states_invalid_transitions_and_non_cancellable_jobs(self):
        self.assertEqual(
            PUBLIC_JOB_STATES,
            {
                "queued",
                "running",
                "cancelling",
                "succeeded",
                "failed",
                "cancelled",
            },
        )
        store = AsyncJobStore()
        snapshot, _ = store.create("raster", job_snapshot(2, cancellable=False))

        with self.assertRaisesRegex(
            InvalidJobTransitionError, "queued -> succeeded"
        ):
            store.transition(snapshot["job_id"], "succeeded", "raster")
        with self.assertRaisesRegex(RuntimeError, "job_not_cancellable"):
            store.request_cancel(snapshot["job_id"], "raster")
        with self.assertRaisesRegex(ValueError, "Invalid public job state"):
            store.transition(snapshot["job_id"], "paused", "raster")
        with self.assertRaisesRegex(JobNotFoundError, "job_not_found"):
            store.get(snapshot["job_id"], "vector")

    def test_idempotency_is_scoped_by_category_and_detects_conflicts(self):
        store = AsyncJobStore()
        vector_request = job_snapshot(3, client_request_id="retry-3")
        vector, is_new = store.create(
            "vector",
            vector_request,
            client_request_id="retry-3",
            request_fingerprint="vector-fingerprint",
        )
        self.assertTrue(is_new)

        replay, replay_is_new = store.create(
            "vector",
            job_snapshot(4, client_request_id="retry-3"),
            client_request_id="retry-3",
            request_fingerprint="vector-fingerprint",
        )
        self.assertFalse(replay_is_new)
        self.assertEqual(replay["job_id"], vector["job_id"])
        self.assertEqual(replay["algorithm"], vector["algorithm"])

        with self.assertRaisesRegex(IdempotencyConflictError, "idempotency_conflict"):
            store.create(
                "vector",
                job_snapshot(5, client_request_id="retry-3"),
                client_request_id="retry-3",
                request_fingerprint="different-fingerprint",
            )

        raster, raster_is_new = store.create(
            "raster",
            job_snapshot(6, client_request_id="retry-3"),
            client_request_id="retry-3",
            request_fingerprint="raster-fingerprint",
        )
        self.assertTrue(raster_is_new)
        self.assertNotEqual(raster["job_id"], vector["job_id"])
        self.assertEqual(
            [item["job_id"] for item in store.list("vector")], [vector["job_id"]]
        )
        self.assertEqual(
            [item["job_id"] for item in store.list("raster")], [raster["job_id"]]
        )

    def test_terminal_ttl_and_count_eviction_are_scope_local(self):
        clock = FakeClock()
        evicted = []
        store = AsyncJobStore(
            terminal_ttl_seconds=10,
            max_terminal_jobs_per_scope=2,
            now=clock,
            on_evict=lambda job_id, runtime: evicted.append((job_id, runtime)),
        )

        vector_jobs = []
        for index in range(3):
            runtime = f"vector-runtime-{index}"
            item, _ = store.create(
                "vector",
                job_snapshot(index, state="failed"),
                runtime=runtime,
            )
            vector_jobs.append(item)
            clock.advance(1)
        raster, _ = store.create(
            "raster", job_snapshot(7, state="succeeded"), runtime="raster-runtime"
        )
        active, _ = store.create(
            "vector", job_snapshot(8), runtime="active-runtime"
        )

        with self.assertRaises(JobNotFoundError):
            store.get(vector_jobs[0]["job_id"])
        self.assertEqual(
            {item["job_id"] for item in store.list("vector")},
            {vector_jobs[1]["job_id"], vector_jobs[2]["job_id"], active["job_id"]},
        )
        self.assertEqual(store.get(raster["job_id"])["state"], "succeeded")
        self.assertIn(
            (vector_jobs[0]["job_id"], "vector-runtime-0"), evicted
        )

        clock.advance(11)
        store.purge()
        self.assertEqual(
            [item["job_id"] for item in store.list("vector")], [active["job_id"]]
        )
        with self.assertRaises(JobNotFoundError):
            store.get(raster["job_id"])
        self.assertEqual(store.get_runtime(active["job_id"]), "active-runtime")

    def test_cancel_completion_race_keeps_exactly_one_terminal_result(self):
        store = AsyncJobStore()
        job, _ = store.create("vector", job_snapshot(9))
        store.transition(job["job_id"], "running", "vector")
        barrier = threading.Barrier(3)
        errors = []

        def finish(state, marker):
            try:
                barrier.wait()
                store.transition(
                    job["job_id"],
                    state,
                    "vector",
                    finished_at=f"2026-08-02T00:03:0{marker}.000Z",
                    terminal_marker=marker,
                )
            except Exception as err:  # pragma: no cover - asserted below
                errors.append(err)

        cancelled_thread = threading.Thread(
            target=finish, args=("cancelled", 1), daemon=True
        )
        failed_thread = threading.Thread(
            target=finish, args=("failed", 2), daemon=True
        )
        cancelled_thread.start()
        failed_thread.start()
        barrier.wait()
        cancelled_thread.join(2)
        failed_thread.join(2)

        self.assertFalse(cancelled_thread.is_alive())
        self.assertFalse(failed_thread.is_alive())
        self.assertEqual(errors, [])
        terminal = store.get(job["job_id"], "vector")
        self.assertIn(terminal["state"], {"cancelled", "failed"})
        self.assertIn(terminal["terminal_marker"], {1, 2})
        self.assertEqual(
            terminal,
            store.transition(
                job["job_id"],
                "succeeded",
                "vector",
                terminal_marker=3,
                result={"OUTPUT": "too late"},
            ),
        )

    def test_list_filters_sorts_limits_and_shutdown_rejects_new_jobs(self):
        store = AsyncJobStore()
        first, _ = store.create("vector", job_snapshot(10))
        second_snapshot = job_snapshot(11, state="failed")
        second_snapshot["created_at"] = "2026-08-02T00:01:00.000Z"
        second, _ = store.create("vector", second_snapshot)
        third_snapshot = job_snapshot(12, state="succeeded")
        third_snapshot["created_at"] = "2026-08-02T00:02:00.000Z"
        third, _ = store.create("vector", third_snapshot)

        self.assertEqual(
            [item["job_id"] for item in store.list("vector", limit=2)],
            [third["job_id"], second["job_id"]],
        )
        self.assertEqual(
            [item["job_id"] for item in store.list("vector", states=["queued"])],
            [first["job_id"]],
        )
        with self.assertRaisesRegex(ValueError, "Invalid public job states"):
            store.list("vector", states=["paused"])
        with self.assertRaisesRegex(ValueError, "1 through 200"):
            store.list("vector", limit=201)

        store.shutdown()
        self.assertFalse(store.accepting)
        self.assertEqual(store.get(first["job_id"])["state"], "queued")
        with self.assertRaisesRegex(RuntimeError, "stopping"):
            store.create("vector", job_snapshot(13))

        # Public snapshots must always be strict JSON serializable.
        json.dumps(store.list("vector"), allow_nan=False)


class TestProcessingJobManager(unittest.TestCase):
    def test_qgs_task_lifecycle_progress_idempotency_and_summary(self):
        environment = FakeProcessingEnvironment(
            [FakeAlgorithm("vector:buffer", cancellable=True)]
        )
        manager = ProcessingJobManager(dependencies=environment.dependencies())
        request = {
            "category": "vector",
            "algorithm_id": "vector:buffer",
            "parameters": {"DISTANCE": 10},
            "add_outputs_to_project": True,
            "client_request_id": "buffer-request",
        }

        started = manager.start(request)
        self.assertEqual(started["state"], "queued")
        self.assertEqual(started["execution_mode"], "qgs_task")
        self.assertTrue(started["cancellable"])
        self.assertEqual(started["algorithm"]["id"], "vector:buffer")
        self.assertEqual(started["algorithm"]["provider"], "vector")
        self.assertIsNone(started["started_at"])
        self.assertIsNone(started["finished_at"])
        self.assertIsNone(started["error"])
        self.assertNotIn("result", started)
        self.assertEqual(len(environment.task_manager.tasks), 1)

        replay = manager.start(dict(request))
        self.assertEqual(replay["job_id"], started["job_id"])
        self.assertEqual(len(environment.task_manager.tasks), 1)
        self.assertEqual(environment.initialized, 1)
        conflicting = dict(request)
        conflicting["parameters"] = {"DISTANCE": 20}
        with self.assertRaisesRegex(
            IdempotencyConflictError, "idempotency_conflict"
        ):
            manager.start(conflicting)

        task = environment.tasks[0]
        feedback = environment.feedback[0]
        task.begun.emit()
        feedback.progressChanged.emit(35.5)
        feedback.progressTextChanged.emit("Buffering")
        feedback.processedCountChanged.emit(7)
        feedback.sinkFeatureCountChanged.emit("OUTPUT", 4)
        feedback.sinkFeatureCountChanged.emit("REJECTED", 2)
        running = manager.get(
            {"job_id": started["job_id"], "category": "vector"}
        )
        self.assertEqual(running["state"], "running")
        self.assertIsNotNone(running["started_at"])
        self.assertEqual(running["progress_percent"], 35.5)
        self.assertEqual(running["progress_text"], "Buffering")
        self.assertEqual(running["processed_count"], 6)

        with mock.patch.dict(sys.modules, {"qgis.core": None}):
            task.executed.emit(
                True,
                {
                    "OUTPUT": FakeMapLayer(),
                    "NON_FINITE": float("nan"),
                    "WHEN": datetime(2026, 8, 2, 12, 30, tzinfo=timezone.utc),
                },
            )
        succeeded = manager.get(started["job_id"])
        self.assertEqual(succeeded["state"], "succeeded")
        self.assertEqual(succeeded["progress_percent"], 100.0)
        self.assertIsNotNone(succeeded["finished_at"])
        self.assertIsNone(succeeded["error"])
        self.assertIsNone(succeeded["result"]["NON_FINITE"])
        self.assertEqual(
            succeeded["result"]["OUTPUT"],
            {
                "layer_id": "layer-1",
                "name": "Generated layer",
                "source": "memory:generated",
                "provider": "memory",
                "layer_type": "VectorLayer",
            },
        )
        self.assertEqual(len(environment.postprocess_calls), 1)
        self.assertEqual(
            set(environment.postprocess_calls[0][3]),
            {"OUTPUT", "NON_FINITE", "WHEN"},
        )
        json.dumps(succeeded, allow_nan=False)

        listed = manager.list(
            {"category": "vector", "states": ["succeeded"], "limit": 10}
        )
        self.assertEqual(len(listed["jobs"]), 1)
        self.assertEqual(listed["jobs"][0]["job_id"], started["job_id"])
        self.assertNotIn("result", listed["jobs"][0])
        self.assertEqual(listed["jobs"][0]["error"], None)
        self.assertEqual(manager.list({"category": "raster"}), {"jobs": []})
        with self.assertRaises(JobNotFoundError):
            manager.get({"job_id": started["job_id"], "category": "raster"})
        with self.assertRaisesRegex(ValueError, "1 through 200"):
            manager.list({"category": "vector", "limit": 201})

    def test_failed_task_has_structured_error_and_list_omits_error_details(self):
        environment = FakeProcessingEnvironment([FakeAlgorithm("raster:warp")])
        manager = ProcessingJobManager(dependencies=environment.dependencies())
        started = manager.start(
            {
                "category": "raster",
                "algorithm_id": "raster:warp",
                "parameters": {},
                "add_outputs_to_project": False,
            }
        )
        task = environment.tasks[0]
        feedback = environment.feedback[0]
        task.begun.emit()
        feedback.warningPushed.emit("Input is unusual")
        feedback.errorReported.emit("Provider rejected a feature")
        task.executed.emit(False, {"partial": float("inf")})

        failed = manager.get(started["job_id"])
        self.assertEqual(failed["state"], "failed")
        self.assertEqual(
            failed["error"]["code"], "algorithm_execution_failed"
        )
        self.assertEqual(failed["error"]["stage"], "execute")
        self.assertIsNone(failed["error"]["details"]["results"]["partial"])
        self.assertEqual(
            failed["error"]["feedback_messages"],
            [
                "warning: Input is unusual",
                "error: Provider rejected a feature",
            ],
        )
        self.assertNotIn("result", failed)

        summary = manager.list({"category": "raster"})["jobs"][0]
        self.assertEqual(
            set(summary["error"]), {"code", "stage", "message"}
        )
        self.assertNotIn("details", summary["error"])
        self.assertNotIn("feedback_messages", summary["error"])

    def test_output_postprocessing_failure_is_structured(self):
        environment = FakeProcessingEnvironment([FakeAlgorithm("vector:buffer")])
        dependencies = environment.dependencies()
        dependencies["post_processor"] = lambda *arguments: False
        manager = ProcessingJobManager(dependencies=dependencies)
        started = manager.start(
            {
                "category": "vector",
                "algorithm_id": "vector:buffer",
                "add_outputs_to_project": True,
            }
        )

        environment.tasks[0].begun.emit()
        environment.tasks[0].executed.emit(True, {"OUTPUT": "memory:result"})
        failed = manager.get(started["job_id"])
        self.assertEqual(failed["state"], "failed")
        self.assertEqual(
            failed["error"]["code"], "output_postprocessing_failed"
        )
        self.assertEqual(failed["error"]["stage"], "postprocess")
        self.assertIn("could not be loaded", failed["error"]["message"])
        self.assertNotIn("result", failed)

    def test_outputs_disabled_retains_context_without_postprocessing(self):
        environment = FakeProcessingEnvironment([FakeAlgorithm("raster:warp")])
        manager = ProcessingJobManager(dependencies=environment.dependencies())
        started = manager.start(
            {
                "category": "raster",
                "algorithm_id": "raster:warp",
                "parameters": {"OUTPUT": "memory:"},
                "add_outputs_to_project": False,
            }
        )
        runtime = manager.store.get_runtime(started["job_id"], "raster")
        retained_context = runtime.context

        environment.tasks[0].begun.emit()
        environment.tasks[0].executed.emit(
            True, {"OUTPUT": "memory:retained-output"}
        )
        succeeded = manager.get(started["job_id"])
        self.assertEqual(succeeded["state"], "succeeded")
        self.assertEqual(
            succeeded["result"]["OUTPUT"], "memory:retained-output"
        )
        self.assertEqual(environment.postprocess_calls, [])
        retained_runtime = manager.store.get_runtime(
            started["job_id"], "raster"
        )
        self.assertIs(retained_runtime, runtime)
        self.assertIs(retained_runtime.context, retained_context)

    def test_task_construction_and_task_manager_failures_become_failed_jobs(self):
        algorithm = FakeAlgorithm("vector:buffer")

        preparation_environment = FakeProcessingEnvironment([algorithm])
        preparation_dependencies = preparation_environment.dependencies()

        def fail_output_preparation(*arguments):
            del arguments
            raise RuntimeError("output preparation failed")

        preparation_dependencies["prepare_outputs"] = fail_output_preparation
        preparation_manager = ProcessingJobManager(
            dependencies=preparation_dependencies
        )
        preparation = preparation_manager.start(
            {"category": "vector", "algorithm_id": "vector:buffer"}
        )
        self.assertEqual(preparation["state"], "failed")
        self.assertEqual(
            preparation["error"]["code"], "output_preparation_failed"
        )
        self.assertEqual(preparation["error"]["stage"], "prepare")
        self.assertIn("output preparation failed", preparation["error"]["message"])
        preparation_runtime = preparation_manager.store.get_runtime(
            preparation["job_id"], "vector"
        )
        self.assertIsNone(preparation_runtime.task)
        self.assertIsNone(preparation_runtime.context)
        self.assertIsNone(preparation_runtime.feedback)

        constructor_environment = FakeProcessingEnvironment([algorithm])
        constructor_dependencies = constructor_environment.dependencies()

        def fail_task_construction(*arguments):
            del arguments
            raise RuntimeError("task construction failed")

        constructor_dependencies["task_factory"] = fail_task_construction
        constructor_manager = ProcessingJobManager(
            dependencies=constructor_dependencies
        )
        construction = constructor_manager.start(
            {"category": "vector", "algorithm_id": "vector:buffer"}
        )
        self.assertEqual(construction["state"], "failed")
        self.assertEqual(construction["error"]["code"], "task_start_failed")
        self.assertEqual(construction["error"]["stage"], "schedule")
        self.assertIn("task construction failed", construction["error"]["message"])
        construction_runtime = constructor_manager.store.get_runtime(
            construction["job_id"], "vector"
        )
        self.assertIsNone(construction_runtime.task)
        self.assertIsNone(construction_runtime.context)
        self.assertIsNone(construction_runtime.feedback)

        schedule_environment = FakeProcessingEnvironment([algorithm])
        schedule_dependencies = schedule_environment.dependencies()

        class FailingTaskManager:
            @staticmethod
            def addTask(task):
                del task
                raise RuntimeError("task manager rejected task")

        schedule_dependencies["task_manager"] = FailingTaskManager()
        schedule_manager = ProcessingJobManager(dependencies=schedule_dependencies)
        scheduling = schedule_manager.start(
            {"category": "vector", "algorithm_id": "vector:buffer"}
        )
        self.assertEqual(scheduling["state"], "failed")
        self.assertEqual(scheduling["error"]["code"], "task_start_failed")
        self.assertEqual(scheduling["error"]["stage"], "schedule")
        self.assertIn("task manager rejected task", scheduling["error"]["message"])
        scheduling_runtime = schedule_manager.store.get_runtime(
            scheduling["job_id"], "vector"
        )
        self.assertIsNone(scheduling_runtime.task)
        self.assertIsNone(scheduling_runtime.context)
        self.assertIsNone(scheduling_runtime.feedback)

    def test_shutdown_never_falls_back_to_an_unbounded_no_argument_wait(self):
        environment = FakeProcessingEnvironment([FakeAlgorithm("vector:slow")])
        dependencies = environment.dependencies()

        class NoArgumentWaitTask(FakeTask):
            def __init__(self):
                super().__init__()
                self.unbounded_wait_calls = 0

            def waitForFinished(self):
                self.unbounded_wait_calls += 1
                raise AssertionError("unbounded wait must not be called")

        task = NoArgumentWaitTask()
        dependencies["task_factory"] = lambda *arguments: task
        manager = ProcessingJobManager(dependencies=dependencies)
        started = manager.start(
            {"category": "vector", "algorithm_id": "vector:slow"}
        )
        task.begun.emit()

        manager.shutdown(timeout_seconds=0.01)
        self.assertEqual(task.cancel_calls, 1)
        self.assertEqual(task.unbounded_wait_calls, 0)
        self.assertEqual(manager.get(started["job_id"])["state"], "cancelling")
        task.executed.emit(True, {"OUTPUT": "memory:completed-after-shutdown"})
        self.assertEqual(manager.get(started["job_id"])["state"], "succeeded")

    def test_shutdown_timeout_leaves_running_task_for_signals_to_finish(self):
        environment = FakeProcessingEnvironment([FakeAlgorithm("vector:slow")])
        dependencies = environment.dependencies()

        class StillRunningTask(FakeTask):
            def waitForFinished(self, timeout):
                self.wait_calls.append(timeout)
                return False

        task = StillRunningTask()
        dependencies["task_factory"] = lambda *arguments: task
        manager = ProcessingJobManager(dependencies=dependencies)
        started = manager.start(
            {"category": "vector", "algorithm_id": "vector:slow"}
        )
        task.begun.emit()

        manager.shutdown(timeout_seconds=0.01)
        pending = manager.get(started["job_id"])
        self.assertEqual(pending["state"], "cancelling")
        self.assertTrue(pending["cancel_requested"])
        self.assertEqual(task.cancel_calls, 1)
        self.assertEqual(len(task.wait_calls), 1)

        task.executed.emit(True, {"OUTPUT": "memory:race-won-by-completion"})
        succeeded = manager.get(started["job_id"])
        self.assertEqual(succeeded["state"], "succeeded")
        self.assertTrue(succeeded["cancel_requested"])
        task.taskTerminated.emit()
        self.assertEqual(manager.get(started["job_id"]), succeeded)

    def test_shutdown_and_cancel_cannot_race_task_registration(self):
        for operation in ("shutdown", "cancel"):
            with self.subTest(operation=operation):
                environment = FakeProcessingEnvironment(
                    [FakeAlgorithm("vector:slow")]
                )
                dependencies = environment.dependencies()
                factory_entered = threading.Event()
                release_factory = threading.Event()
                task = FakeTask()

                def blocking_task_factory(*arguments):
                    del arguments
                    factory_entered.set()
                    release_factory.wait(2)
                    return task

                dependencies["task_factory"] = blocking_task_factory
                manager = ProcessingJobManager(dependencies=dependencies)
                outcome = {}

                def start_job():
                    try:
                        outcome["snapshot"] = manager.start(
                            {
                                "category": "vector",
                                "algorithm_id": "vector:slow",
                            }
                        )
                    except Exception as error:
                        outcome["error"] = error

                thread = threading.Thread(target=start_job)
                thread.start()
                self.assertTrue(factory_entered.wait(2))
                active = manager.store.list("vector", limit=1)[0]
                if operation == "shutdown":
                    manager.shutdown(timeout_seconds=0)
                else:
                    cancelling = manager.cancel(active["job_id"])
                    self.assertEqual(cancelling["state"], "cancelling")
                release_factory.set()
                thread.join(2)
                self.assertFalse(thread.is_alive())
                self.assertNotIn("error", outcome)
                self.assertEqual(outcome["snapshot"]["state"], "cancelled")
                self.assertTrue(outcome["snapshot"]["cancel_requested"])
                self.assertEqual(environment.task_manager.tasks, [])
                self.assertEqual(task.cancel_calls, 0)
                if manager.accepting:
                    manager.shutdown(timeout_seconds=0)

    def test_shutdown_cannot_race_main_thread_callback_registration(self):
        environment = FakeProcessingEnvironment(
            [FakeAlgorithm("vector:main", no_threading=True)]
        )
        dependencies = environment.dependencies()
        preparation_entered = threading.Event()
        release_preparation = threading.Event()

        def blocking_prepare_outputs(algorithm, parameters, add_outputs):
            del algorithm, add_outputs
            preparation_entered.set()
            release_preparation.wait(2)
            return dict(parameters)

        dependencies["prepare_outputs"] = blocking_prepare_outputs
        manager = ProcessingJobManager(dependencies=dependencies)
        outcome = {}

        def start_job():
            try:
                outcome["snapshot"] = manager.start(
                    {"category": "vector", "algorithm_id": "vector:main"}
                )
            except Exception as error:
                outcome["error"] = error

        thread = threading.Thread(target=start_job)
        thread.start()
        self.assertTrue(preparation_entered.wait(2))
        manager.shutdown(timeout_seconds=0)
        release_preparation.set()
        thread.join(2)
        self.assertFalse(thread.is_alive())
        self.assertNotIn("error", outcome)
        self.assertEqual(outcome["snapshot"]["state"], "cancelled")
        self.assertTrue(outcome["snapshot"]["cancel_requested"])
        self.assertEqual(environment.single_shots, [])
        self.assertEqual(environment.main_thread_calls, [])

    def test_cancel_and_external_task_termination_are_race_safe(self):
        environment = FakeProcessingEnvironment(
            [FakeAlgorithm("vector:slow", cancellable=True)]
        )
        manager = ProcessingJobManager(dependencies=environment.dependencies())

        first = manager.start(
            {"category": "vector", "algorithm_id": "vector:slow"}
        )
        first_task = environment.tasks[0]
        first_task.begun.emit()
        cancelling = manager.cancel(
            {"job_id": first["job_id"], "category": "vector"}
        )
        self.assertEqual(cancelling["state"], "cancelling")
        self.assertTrue(cancelling["cancel_requested"])
        self.assertEqual(first_task.cancel_calls, 1)
        first_task.taskTerminated.emit()
        cancelled = manager.get(first["job_id"])
        self.assertEqual(cancelled["state"], "cancelled")
        self.assertTrue(cancelled["cancel_requested"])

        second = manager.start(
            {"category": "vector", "algorithm_id": "vector:slow"}
        )
        second_task = environment.tasks[1]
        second_task.begun.emit()
        # This simulates cancellation from QGIS's external task panel.
        second_task.cancel()
        second_task.taskTerminated.emit()
        externally_cancelled = manager.get(second["job_id"])
        self.assertEqual(externally_cancelled["state"], "cancelled")
        self.assertTrue(externally_cancelled["cancel_requested"])

        third = manager.start(
            {"category": "vector", "algorithm_id": "vector:slow"}
        )
        third_task = environment.tasks[2]
        third_task.begun.emit()
        manager.cancel(third["job_id"])
        # Completion can win the cancellation race and must remain terminal.
        third_task.executed.emit(True, {"OUTPUT": "memory:complete"})
        raced = manager.get(third["job_id"])
        self.assertEqual(raced["state"], "succeeded")
        self.assertTrue(raced["cancel_requested"])
        third_task.taskTerminated.emit()
        self.assertEqual(manager.get(third["job_id"]), raced)

    def test_main_thread_job_is_deferred_non_cancellable_and_stopped_safely(self):
        environment = FakeProcessingEnvironment(
            [FakeAlgorithm("vector:main", no_threading=True)]
        )
        manager = ProcessingJobManager(dependencies=environment.dependencies())

        started = manager.start(
            {"category": "vector", "algorithm_id": "vector:main"}
        )
        self.assertEqual(started["state"], "queued")
        self.assertEqual(started["execution_mode"], "main_thread")
        self.assertFalse(started["cancellable"])
        self.assertEqual(environment.main_thread_calls, [])
        self.assertEqual(len(environment.single_shots), 1)
        with self.assertRaisesRegex(RuntimeError, "job_not_cancellable"):
            manager.cancel(started["job_id"])

        delay, callback = environment.single_shots.pop(0)
        self.assertEqual(delay, 0)
        callback()
        succeeded = manager.get(started["job_id"])
        self.assertEqual(succeeded["state"], "succeeded")
        self.assertEqual(succeeded["execution_mode"], "main_thread")
        self.assertEqual(len(environment.main_thread_calls), 1)

        pending = manager.start(
            {"category": "vector", "algorithm_id": "vector:main"}
        )
        _, pending_callback = environment.single_shots.pop(0)
        manager.shutdown(timeout_seconds=0)
        stopped = manager.get(pending["job_id"])
        self.assertEqual(stopped["state"], "cancelled")
        self.assertTrue(stopped["cancel_requested"])
        pending_callback()
        self.assertEqual(manager.get(pending["job_id"]), stopped)
        with self.assertRaisesRegex(RuntimeError, "stopping"):
            manager.start(
                {"category": "vector", "algorithm_id": "vector:main"}
            )

        fresh_manager = ProcessingJobManager(
            dependencies=environment.dependencies()
        )
        fresh = fresh_manager.start(
            {"category": "vector", "algorithm_id": "vector:main"}
        )
        self.assertNotEqual(fresh["job_id"], pending["job_id"])
        with self.assertRaises(JobNotFoundError):
            fresh_manager.get(pending["job_id"])
        fresh_manager.shutdown(timeout_seconds=0)

    def test_bridge_restart_replaces_processing_job_manager(self):
        from qcopilots_common.bridge import QgisBridgeController

        controller = QgisBridgeController(
            None, port=0, auth_token="processing-jobs-test-token"
        )
        try:
            controller.start()
            first_manager = controller._get_processing_job_manager()
            self.assertTrue(first_manager.accepting)
            controller.stop(timeout_seconds=2)
            self.assertFalse(first_manager.accepting)
            self.assertIsNone(controller._processing_job_manager)

            controller.start()
            second_manager = controller._get_processing_job_manager()
            self.assertIsNot(second_manager, first_manager)
            self.assertTrue(second_manager.accepting)
        finally:
            controller.stop(timeout_seconds=2)


class TestProcessingJobManagerQgisIntegration(QgisTestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        Processing.initialize()
        cls.registry = QgsApplication.processingRegistry()
        existing = cls.registry.providerById(
            QCopilotsProcessingJobsProvider.PROVIDER_ID
        )
        if existing is not None:
            cls.registry.removeProvider(existing)
        cls.provider = QCopilotsProcessingJobsProvider()
        if not cls.registry.addProvider(cls.provider):
            raise AssertionError("Could not register Processing jobs test provider")

    @classmethod
    def tearDownClass(cls):
        QCopilotsProgressAlgorithm.release.set()
        if getattr(cls, "registry", None) is not None:
            provider = cls.registry.providerById(
                QCopilotsProcessingJobsProvider.PROVIDER_ID
            )
            if provider is not None:
                cls.registry.removeProvider(provider)
        super().tearDownClass()

    def setUp(self):
        super().setUp()
        self.project = QgsProject.instance()
        self.initial_layer_ids = set(self.project.mapLayers())
        self.manager = ProcessingJobManager()
        QCopilotsProgressAlgorithm.reset_events()
        QCopilotsMainThreadAlgorithm.reset_execution()

    def tearDown(self):
        QCopilotsProgressAlgorithm.release.set()
        if getattr(self, "manager", None) is not None:
            self.manager.shutdown(timeout_seconds=5)
        QCoreApplication.processEvents()
        added_layer_ids = set(self.project.mapLayers()) - self.initial_layer_ids
        if added_layer_ids:
            self.project.removeMapLayers(list(added_layer_ids))
        super().tearDown()

    def _wait_for(self, job_id, predicate, timeout_seconds=10):
        deadline = time.monotonic() + timeout_seconds
        snapshot = self.manager.get(job_id)
        while time.monotonic() < deadline:
            QCoreApplication.processEvents()
            snapshot = self.manager.get(job_id)
            if predicate(snapshot):
                return snapshot
            time.sleep(0.005)
        self.fail(
            f"Timed out waiting for Processing job {job_id}: "
            f"{snapshot['state']} {snapshot.get('error')}"
        )

    def _wait_for_terminal(self, job_id):
        return self._wait_for(
            job_id,
            lambda snapshot: snapshot["state"]
            in {"succeeded", "failed", "cancelled"},
        )

    def test_real_qgs_task_progress_and_output_ownership(self):
        algorithm_id = (
            f"{QCopilotsProcessingJobsProvider.PROVIDER_ID}:vector_progress"
        )
        project_before = set(self.project.mapLayers())
        with_outputs = self.manager.start(
            {
                "category": "vector",
                "algorithm_id": algorithm_id,
                "parameters": {"OUTPUT": "memory:"},
                "add_outputs_to_project": True,
            }
        )
        self.assertEqual(with_outputs["execution_mode"], "qgs_task")
        self.assertTrue(with_outputs["cancellable"])
        self.assertTrue(QCopilotsProgressAlgorithm.progress_ready.wait(5))
        running = self._wait_for(
            with_outputs["job_id"],
            lambda snapshot: snapshot["state"] == "running"
            and snapshot["progress_percent"] >= 42,
        )
        self.assertEqual(running["progress_text"], "Controlled progress reached")
        self.assertGreaterEqual(running["processed_count"], 3)
        QCopilotsProgressAlgorithm.release.set()
        succeeded = self._wait_for_terminal(with_outputs["job_id"])
        self.assertEqual(succeeded["state"], "succeeded")
        self.assertEqual(succeeded["progress_percent"], 100.0)
        added_layer_ids = set(self.project.mapLayers()) - project_before
        self.assertEqual(len(added_layer_ids), 1)
        loaded_layer = self.project.mapLayer(next(iter(added_layer_ids)))
        self.assertIsNotNone(loaded_layer)
        self.assertEqual(loaded_layer.featureCount(), 3)

        self.project.removeMapLayers(list(added_layer_ids))
        project_without_outputs = set(self.project.mapLayers())
        QCopilotsProgressAlgorithm.reset_events()
        without_outputs = self.manager.start(
            {
                "category": "vector",
                "algorithm_id": algorithm_id,
                "parameters": {"OUTPUT": "memory:"},
                "add_outputs_to_project": False,
            }
        )
        runtime = self.manager.store.get_runtime(
            without_outputs["job_id"], "vector"
        )
        retained_context = runtime.context
        self.assertTrue(QCopilotsProgressAlgorithm.progress_ready.wait(5))
        QCopilotsProgressAlgorithm.release.set()
        retained = self._wait_for_terminal(without_outputs["job_id"])
        self.assertEqual(retained["state"], "succeeded")
        self.assertEqual(set(self.project.mapLayers()), project_without_outputs)
        retained_runtime = self.manager.store.get_runtime(
            without_outputs["job_id"], "vector"
        )
        self.assertIs(retained_runtime.context, retained_context)
        self.assertGreaterEqual(retained_context.temporaryLayerStore().count(), 1)

    def test_real_flag_no_threading_is_deferred_to_qt_main_thread(self):
        algorithm_id = (
            f"{QCopilotsProcessingJobsProvider.PROVIDER_ID}:vector_main_thread"
        )
        started = self.manager.start(
            {"category": "vector", "algorithm_id": algorithm_id}
        )
        self.assertEqual(started["state"], "queued")
        self.assertEqual(started["execution_mode"], "main_thread")
        self.assertFalse(started["cancellable"])
        self.assertEqual(QCopilotsMainThreadAlgorithm.executions, 0)

        succeeded = self._wait_for_terminal(started["job_id"])
        self.assertEqual(succeeded["state"], "succeeded")
        self.assertEqual(QCopilotsMainThreadAlgorithm.executions, 1)
        self.assertTrue(
            QCopilotsMainThreadAlgorithm.ran_on_application_thread
        )
        self.assertEqual(
            succeeded["result"], {"VALUE": "main-thread-complete"}
        )

    def test_real_qgs_task_slow_cancellation_reaches_cancelled(self):
        algorithm_id = (
            f"{QCopilotsProcessingJobsProvider.PROVIDER_ID}:vector_progress"
        )
        started = self.manager.start(
            {
                "category": "vector",
                "algorithm_id": algorithm_id,
                "parameters": {"OUTPUT": "memory:"},
                "add_outputs_to_project": False,
            }
        )
        self.assertTrue(QCopilotsProgressAlgorithm.progress_ready.wait(5))
        cancelling = self.manager.cancel(
            {"job_id": started["job_id"], "category": "vector"}
        )
        self.assertEqual(cancelling["state"], "cancelling")
        self.assertTrue(cancelling["cancel_requested"])
        QCopilotsProgressAlgorithm.release.set()

        cancelled = self._wait_for_terminal(started["job_id"])
        self.assertEqual(cancelled["state"], "cancelled")
        self.assertTrue(cancelled["cancel_requested"])
        self.assertNotIn("result", cancelled)

    def test_real_processing_exception_becomes_structured_failure(self):
        algorithm_id = (
            f"{QCopilotsProcessingJobsProvider.PROVIDER_ID}:vector_failure"
        )
        started = self.manager.start(
            {"category": "vector", "algorithm_id": algorithm_id}
        )
        failed = self._wait_for_terminal(started["job_id"])
        self.assertEqual(failed["state"], "failed")
        self.assertEqual(
            failed["error"]["code"], "algorithm_execution_failed"
        )
        self.assertEqual(failed["error"]["stage"], "execute")
        self.assertTrue(
            any(
                "controlled Processing failure" in message
                for message in failed["error"]["feedback_messages"]
            )
        )


class TestProcessingJsonSafety(unittest.TestCase):
    def test_recursive_qgis_and_python_values_are_strict_json_safe(self):
        recursive = []
        recursive.append(recursive)
        with mock.patch.dict(sys.modules, {"qgis.core": None}):
            safe = json_safe_value(
                {
                    7: {
                        "nan": float("nan"),
                        "positive_infinity": float("inf"),
                        "negative_infinity": float("-inf"),
                    },
                    "bytes": b"QGIS\xff",
                    "path": Path("relative/output.gpkg"),
                    "datetime": datetime(
                        2026, 8, 2, 18, 30, 5, tzinfo=timezone.utc
                    ),
                    "date": date(2026, 8, 2),
                    "enum": FakeEnum.VALUE,
                    "layer": FakeMapLayer(),
                    "recursive": recursive,
                }
            )

        self.assertEqual(
            safe["7"],
            {
                "nan": None,
                "positive_infinity": None,
                "negative_infinity": None,
            },
        )
        self.assertEqual(safe["bytes"], "QGIS\ufffd")
        self.assertEqual(safe["path"], str(Path("relative/output.gpkg")))
        self.assertEqual(safe["datetime"], "2026-08-02T18:30:05Z")
        self.assertEqual(safe["date"], "2026-08-02")
        self.assertEqual(safe["enum"], "enum-value")
        self.assertEqual(safe["layer"]["layer_id"], "layer-1")
        self.assertEqual(safe["recursive"], ["<recursive>"])
        encoded = json.dumps(safe, ensure_ascii=False, allow_nan=False)
        self.assertIn("Generated layer", encoded)


if __name__ == "__main__":
    unittest.main()
