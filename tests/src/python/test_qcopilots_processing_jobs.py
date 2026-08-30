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
import sqlite3
import sys
import tempfile
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
        self.destroyed = FakeSignal()
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


class FakeProcessingDefinition:
    def __init__(self, name, *, destination, definition_type=None):
        self._name = name
        self._destination = destination
        self._definition_type = definition_type

    def name(self):
        return self._name

    def isDestination(self):
        return self._destination

    def type(self):
        return self._definition_type or (
            "sink" if self._destination else "source"
        )


class FakeAlgorithm:
    def __init__(
        self,
        algorithm_id,
        *,
        no_threading=False,
        cancellable=True,
        parameter_definitions=None,
        group_id="",
    ):
        self.algorithm_id = algorithm_id
        self.no_threading = no_threading
        self.cancellable = cancellable
        self.parameter_definitions = list(parameter_definitions or [])
        self.group_id = group_id

    def id(self):
        return self.algorithm_id

    def name(self):
        return self.algorithm_id.partition(":")[2]

    def displayName(self):
        return f"Test {self.name()}"

    def group(self):
        return "Test algorithms"

    def groupId(self):
        return self.group_id

    def provider(self):
        return FakeProvider(self.algorithm_id.partition(":")[0])

    def parameterDefinitions(self):
        return self.parameter_definitions


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


class FakeLoadContext:
    def __init__(self):
        self.layers = {}

    def layersToLoadOnCompletion(self):
        return dict(self.layers)

    def setLayersToLoadOnCompletion(self, layers):
        self.layers = dict(layers)


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
            "output_authorizer": lambda *args: None,
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
        self.addParameter(
            QgsProcessingParameterFeatureSink(
                "OUTPUT",
                "Generated output",
                optional=True,
            )
        )

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
        self.addParameter(
            QgsProcessingParameterFeatureSink(
                "OUTPUT",
                "Generated output",
                optional=True,
            )
        )

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

    def test_eviction_runs_cleanup_first_and_retains_failed_cleanup_audit(self):
        locked = {"value": True}
        attempts = []

        def cleanup(job_id, runtime):
            attempts.append((job_id, runtime))
            if locked["value"]:
                return {
                    "complete": False,
                    "residual_paths": ["locked-stage"],
                }
            return {"complete": True, "residual_paths": []}

        store = AsyncJobStore(
            max_terminal_jobs_per_scope=1,
            on_evict=cleanup,
        )
        first, _ = store.create(
            "vector",
            job_snapshot(40, state="failed"),
            runtime="first-runtime",
        )
        second, _ = store.create(
            "vector",
            job_snapshot(41, state="failed"),
            runtime="second-runtime",
        )

        with self.assertRaises(JobNotFoundError):
            store.get(first["job_id"], "vector")
        audits = store.cleanup_audits("vector")
        self.assertEqual(len(audits), 1)
        self.assertEqual(audits[0]["job_id"], first["job_id"])
        self.assertFalse(audits[0]["cleanup"]["complete"])
        self.assertTrue(audits[0]["cleanup"]["eviction_cleanup_failed"])
        self.assertEqual(
            audits[0]["cleanup"]["residual_paths"], ["locked-stage"]
        )
        self.assertIn((first["job_id"], "first-runtime"), attempts)

        locked["value"] = False
        third, _ = store.create(
            "vector",
            job_snapshot(42, state="failed"),
            runtime="third-runtime",
        )
        with self.assertRaises(JobNotFoundError):
            store.get(first["job_id"], "vector")
        self.assertEqual(store.get(third["job_id"], "vector")["state"], "failed")
        with self.assertRaises(JobNotFoundError):
            store.get(second["job_id"], "vector")
        self.assertEqual(
            {item["job_id"] for item in store.list("vector")},
            {third["job_id"]},
        )

    def test_eviction_callback_exception_retains_auditable_terminal_job(self):
        store = AsyncJobStore(
            max_terminal_jobs_per_scope=0,
            on_evict=lambda _job_id, _runtime: (_ for _ in ()).throw(
                PermissionError("stage is locked")
            ),
        )
        job, _ = store.create(
            "raster",
            job_snapshot(43, state="failed"),
            runtime="locked-runtime",
        )

        with self.assertRaises(JobNotFoundError):
            store.get(job["job_id"], "raster")
        audit = store.cleanup_audits("raster")[0]
        self.assertEqual(audit["state"], "failed")
        self.assertFalse(audit["cleanup"]["complete"])
        self.assertTrue(audit["cleanup"]["eviction_cleanup_failed"])
        self.assertIn("stage is locked", audit["cleanup"]["error"])

    def test_repeated_cleanup_failures_keep_terminal_runtime_and_audit_memory_bounded(self):
        attempts = []

        def cleanup(job_id, runtime):
            attempts.append((job_id, runtime))
            return {
                "complete": False,
                "residual_paths": [f"locked-{job_id}"],
                "errors": ["locked"],
            }

        store = AsyncJobStore(
            max_terminal_jobs_per_scope=1,
            on_evict=cleanup,
        )
        jobs = []
        for index in range(8):
            job, _ = store.create(
                "vector",
                job_snapshot(60 + index, state="failed"),
                runtime={"payload": bytearray(1024 * 1024)},
            )
            jobs.append(job)

        self.assertEqual(len(store.list("vector")), 1)
        self.assertEqual(len(store.terminal_runtime_items()), 1)
        self.assertEqual(len(store.cleanup_audits("vector")), 1)
        self.assertEqual(len(attempts), 7)
        for job in jobs[:-1]:
            with self.assertRaises(JobNotFoundError):
                store.get(job["job_id"], "vector")
        self.assertEqual(
            store.get(jobs[-1]["job_id"], "vector")["state"], "failed"
        )

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
    def test_overwrite_confirmation_store_is_bounded_under_concurrency(self):
        from qcopilots_common.processing_jobs import ProcessingOverwriteConfirmationStore

        store = ProcessingOverwriteConfirmationStore(max_entries=256)
        issued = []
        errors = []
        lock = threading.Lock()

        def issue_many(worker):
            try:
                local = []
                for index in range(100):
                    fingerprint = f"worker-{worker}-{index}"
                    local.append((store.issue(fingerprint), fingerprint))
                with lock:
                    issued.extend(local)
            except Exception as err:
                with lock:
                    errors.append(err)

        threads = [
            threading.Thread(target=issue_many, args=(worker,), daemon=True)
            for worker in range(8)
        ]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join(5)

        self.assertTrue(all(not thread.is_alive() for thread in threads))
        self.assertEqual(errors, [])
        self.assertEqual(len(issued), 800)
        self.assertEqual(len({token for token, _fingerprint in issued}), 800)
        self.assertLessEqual(len(store._entries), 256)
        latest_token = next(reversed(store._entries))
        latest_fingerprint = store._entries[latest_token]["fingerprint"]
        store.consume(latest_token, latest_fingerprint)
        with self.assertRaisesRegex(RuntimeError, "invalid or already used"):
            store.consume(latest_token, latest_fingerprint)

    def test_confirmed_start_reuses_authorized_input_versions_once(self):
        import qcopilots_common.processing_jobs as processing_jobs

        definitions = [
            FakeProcessingDefinition("INPUT", destination=False),
            FakeProcessingDefinition("OUTPUT", destination=True),
        ]
        algorithm = FakeAlgorithm(
            "vector:copy",
            parameter_definitions=definitions,
        )
        environment = FakeProcessingEnvironment([algorithm])
        dependencies = environment.dependencies()
        dependencies.pop("output_authorizer")
        manager = ProcessingJobManager(dependencies=dependencies)

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source.geojson"
            source.write_text(
                '{"type":"FeatureCollection","features":[]}',
                encoding="utf-8",
            )
            target = root / "target.geojson"
            target.write_text("old", encoding="utf-8")
            arguments = {
                "category": "vector",
                "algorithm_id": algorithm.id(),
                "parameters": {"INPUT": str(source), "OUTPUT": str(target)},
                "overwrite_outputs": True,
            }
            original = processing_jobs._processing_input_versions
            calls = []

            def counted(parameters, parameter_definitions):
                calls.append(1)
                return original(parameters, parameter_definitions)

            with mock.patch.object(
                processing_jobs,
                "_processing_input_versions",
                side_effect=counted,
            ):
                preview = manager.start(arguments)
                self.assertEqual(len(calls), 1)
                calls.clear()
                started = manager.start(
                    {
                        **arguments,
                        "overwrite_confirmation_token": preview[
                            "overwrite_confirmation_token"
                        ],
                    }
                )
                self.assertEqual(len(calls), 1)

            runtime = manager.store.get_runtime(started["job_id"], "vector")
            self.assertTrue(runtime.authorized_input_versions)
            manager.shutdown(timeout_seconds=0)

    def test_processing_fingerprint_limits_reject_before_expensive_reads(self):
        import qcopilots_common.processing_jobs as processing_jobs

        class OversizedLayer:
            def featureCount(self):
                return processing_jobs.MAX_PROCESSING_LAYER_FINGERPRINT_FEATURES + 1

            def getFeatures(self):
                raise AssertionError("getFeatures must not be called")

        with self.assertRaisesRegex(RuntimeError, "declared feature count"):
            processing_jobs._processing_project_layer_content_fingerprint(
                OversizedLayer()
            )

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "oversized.bin"
            with path.open("wb") as handle:
                handle.seek(processing_jobs.MAX_PROCESSING_VERSION_FILE_BYTES)
                handle.write(b"x")
            with mock.patch.object(
                processing_jobs,
                "_processing_file_sha256",
            ) as hasher, self.assertRaisesRegex(RuntimeError, "file exceeds"):
                processing_jobs._processing_file_version(path)
            hasher.assert_not_called()

            small = Path(tmp) / "small.bin"
            small.write_bytes(b"small")
            with self.assertRaisesRegex(RuntimeError, "fingerprinting exceeded"):
                processing_jobs._processing_file_sha256(
                    small,
                    deadline=time.monotonic() - 1,
                )

    def test_shutdown_retries_terminal_success_and_failure_cleanup(self):
        import qcopilots_common.processing_jobs as processing_jobs

        manager = ProcessingJobManager()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stages = []
            for index, state in enumerate(("succeeded", "failed")):
                staging_root = root / f"stage-{index}"
                staging_root.mkdir()
                (staging_root / "temporary.bin").write_bytes(b"temporary")
                runtime = processing_jobs._ProcessingRuntime(
                    algorithm=None,
                    parameters={},
                    context=None,
                    feedback=None,
                    add_outputs_to_project=False,
                    generation=0,
                    output_stages=[{"staging_root": staging_root}],
                )
                snapshot = job_snapshot(50 + index, state=state)
                snapshot["cleanup"] = {
                    "complete": False,
                    "residual_paths": [str(staging_root)],
                }
                manager.store.create("vector", snapshot, runtime=runtime)
                stages.append(staging_root)

            manager.shutdown(timeout_seconds=0)

            self.assertTrue(all(not stage.exists() for stage in stages))
            for snapshot in manager.store.list("vector"):
                self.assertTrue(snapshot["cleanup"]["complete"])
                self.assertEqual(snapshot["cleanup"]["residual_paths"], [])

    def test_project_layer_cleanup_failures_remain_auditable_and_retryable(self):
        import qcopilots_common.processing_jobs as processing_jobs

        def runtime():
            return processing_jobs._ProcessingRuntime(
                algorithm=object(),
                parameters={"INPUT": "memory:test"},
                context=object(),
                feedback=object(),
                add_outputs_to_project=True,
                generation=0,
                postprocess_initial_layer_ids={"initial"},
                task=object(),
            )

        query_failure = runtime()
        with mock.patch.object(
            processing_jobs,
            "_processing_project_layer_ids",
            side_effect=RuntimeError("project query failed"),
        ):
            processing_jobs._cleanup_processing_runtime_outputs(query_failure)
        query_receipt = processing_jobs._processing_cleanup_receipt(query_failure)
        self.assertFalse(query_receipt["complete"])
        self.assertIn("project query failed", query_receipt["errors"][0])
        self.assertEqual(query_failure.postprocess_initial_layer_ids, {"initial"})

        removal_failure = runtime()
        with mock.patch.object(
            processing_jobs,
            "_processing_project_layer_ids",
            side_effect=[{"initial", "added"}, {"initial", "added"}],
        ), mock.patch.object(
            processing_jobs,
            "_remove_processing_project_layers",
            side_effect=RuntimeError("project removal failed"),
        ):
            processing_jobs._cleanup_processing_runtime_outputs(removal_failure)
        removal_receipt = processing_jobs._processing_cleanup_receipt(removal_failure)
        self.assertFalse(removal_receipt["complete"])
        self.assertEqual(removal_receipt["residual_layer_ids"], ["added"])
        self.assertTrue(removal_receipt["retry_recommended"])
        self.assertEqual(removal_failure.postprocess_initial_layer_ids, {"initial"})

        with mock.patch.object(
            processing_jobs,
            "_processing_project_layer_ids",
            side_effect=[{"initial", "added"}, {"initial"}],
        ), mock.patch.object(
            processing_jobs,
            "_remove_processing_project_layers",
        ) as remove:
            processing_jobs._cleanup_processing_runtime_outputs(removal_failure)
        remove.assert_called_once_with({"added"})
        self.assertTrue(
            processing_jobs._processing_cleanup_receipt(removal_failure)["complete"]
        )
        self.assertIsNone(removal_failure.postprocess_initial_layer_ids)

    def test_evicted_processing_runtime_drops_heavy_objects_after_cleanup_failure(self):
        import qcopilots_common.processing_jobs as processing_jobs

        runtime = processing_jobs._ProcessingRuntime(
            algorithm=object(),
            parameters={"payload": bytearray(1024 * 1024)},
            context=object(),
            feedback=object(),
            add_outputs_to_project=True,
            generation=0,
            postprocess_initial_layer_ids={"initial"},
            task=object(),
        )

        def incomplete_cleanup(candidate, **_kwargs):
            candidate.cleanup_residual_layer_ids = ["added"]
            candidate.cleanup_errors = ["project removal failed"]

        with mock.patch.object(
            processing_jobs,
            "_cleanup_processing_runtime_outputs",
            side_effect=incomplete_cleanup,
        ):
            receipt = ProcessingJobManager._cleanup_evicted_runtime("job", runtime)

        self.assertFalse(receipt["complete"])
        self.assertEqual(receipt["residual_layer_ids"], ["added"])
        self.assertIsNone(runtime.algorithm)
        self.assertEqual(runtime.parameters, {})
        self.assertIsNone(runtime.context)
        self.assertIsNone(runtime.feedback)
        self.assertIsNone(runtime.task)

    def test_general_category_rejects_unsafe_policy_before_job_creation(self):
        destination = FakeProcessingDefinition(
            "OUTPUT",
            destination=True,
            definition_type="fileDestination",
        )
        unsafe = FakeAlgorithm(
            "native:filedownloader",
            parameter_definitions=[destination],
            group_id="networkanalysis",
        )
        safe = FakeAlgorithm(
            "native:shortestpathpointtopoint",
            parameter_definitions=[destination],
            group_id="networkanalysis",
        )
        environment = FakeProcessingEnvironment([unsafe, safe])
        dependencies = environment.dependencies()
        from qcopilots_common.processing_metadata import (
            processing_algorithm_matches_domain,
        )

        dependencies["category_matcher"] = processing_algorithm_matches_domain
        manager = ProcessingJobManager(dependencies=dependencies)

        with self.assertRaisesRegex(
            RuntimeError,
            "network_access_algorithm_unsupported",
        ):
            manager.start(
                {
                    "category": "general",
                    "algorithm_id": unsafe.id(),
                    "parameters": {"OUTPUT": "memory:"},
                }
            )
        self.assertEqual(environment.tasks, [])
        self.assertEqual(manager.list({"category": "general"})["jobs"], [])

        started = manager.start(
            {
                "category": "general",
                "algorithm_id": safe.id(),
                "parameters": {"OUTPUT": "memory:"},
            }
        )
        self.assertEqual(started["category"], "general")
        self.assertEqual(started["state"], "queued")
        environment.tasks[0].taskTerminated.emit()

    def test_general_database_package_starts_but_external_database_write_is_rejected(self):
        destination = FakeProcessingDefinition(
            "OUTPUT",
            destination=True,
            definition_type="fileDestination",
        )
        external_database = FakeAlgorithm(
            "native:importintospatialite",
            parameter_definitions=[
                FakeProcessingDefinition(
                    "DATABASE",
                    destination=False,
                    definition_type="vector",
                )
            ],
            group_id="database",
        )
        package = FakeAlgorithm(
            "native:package",
            parameter_definitions=[destination],
            group_id="database",
        )
        environment = FakeProcessingEnvironment([external_database, package])
        dependencies = environment.dependencies()
        from qcopilots_common.processing_metadata import (
            processing_algorithm_matches_domain,
        )

        dependencies["category_matcher"] = processing_algorithm_matches_domain
        manager = ProcessingJobManager(dependencies=dependencies)

        with self.assertRaisesRegex(
            RuntimeError,
            "external_database_or_sql_side_effects_unsupported",
        ):
            manager.start(
                {
                    "category": "general",
                    "algorithm_id": external_database.id(),
                    "parameters": {},
                }
            )
        self.assertEqual(environment.tasks, [])
        self.assertEqual(manager.list({"category": "general"})["jobs"], [])

        started = manager.start(
            {
                "category": "general",
                "algorithm_id": package.id(),
                "parameters": {"OUTPUT": "memory:"},
            }
        )
        self.assertEqual(started["category"], "general")
        self.assertEqual(started["state"], "queued")
        environment.tasks[0].taskTerminated.emit()

    def test_successful_publication_cleanup_retry_never_rolls_back_output(self):
        import qcopilots_common.processing_jobs as processing_jobs

        definition = FakeProcessingDefinition("OUTPUT", destination=True)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / "result.tif"
            target.write_bytes(b"original")
            _, stages = processing_jobs._stage_processing_output_destinations(
                {"OUTPUT": str(target)}, [definition]
            )
            stages[0]["staged_target"].write_bytes(b"published")
            receipt = processing_jobs._promote_processing_outputs(
                stages, overwrite=True
            )
            runtime = processing_jobs._ProcessingRuntime(
                algorithm=None,
                parameters={},
                context=None,
                feedback=None,
                add_outputs_to_project=False,
                generation=0,
                output_stages=stages,
                output_promotion_receipt=receipt,
                output_promotion_state="published_success",
            )
            real_rmtree = processing_jobs.shutil.rmtree
            backup_roots = {Path(path) for path in receipt["backup_roots"]}

            def lock_backup_once(path, *arguments, **kwargs):
                if Path(path) in backup_roots:
                    raise PermissionError("backup cleanup locked")
                return real_rmtree(path, *arguments, **kwargs)

            with mock.patch.object(
                processing_jobs.shutil,
                "rmtree",
                side_effect=lock_backup_once,
            ):
                processing_jobs._cleanup_processing_runtime_outputs(
                    runtime,
                    terminal_state="succeeded",
                )
            self.assertEqual(target.read_bytes(), b"published")
            self.assertIsNotNone(runtime.output_promotion_receipt)
            self.assertTrue(runtime.cleanup_residual_paths)

            store = AsyncJobStore(scope_field="category")
            job_id = str(uuid.uuid4())
            store.create(
                "vector",
                {"job_id": job_id, "state": "succeeded", "cleanup": {}},
                runtime=runtime,
            )
            manager = ProcessingJobManager(store=store)
            manager._release_finished_runtime(job_id, "vector", runtime)

            self.assertEqual(target.read_bytes(), b"published")
            self.assertIsNone(runtime.output_promotion_receipt)
            self.assertEqual(runtime.output_promotion_state, "none")
            self.assertTrue(store.get(job_id, "vector")["cleanup"]["complete"])

    def test_stage_preparation_tracks_current_root_before_version_capture(self):
        import qcopilots_common.processing_jobs as processing_jobs

        definition = FakeProcessingDefinition("OUTPUT", destination=True)
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(
            processing_jobs,
            "_processing_file_version",
            side_effect=RuntimeError("version capture failed"),
        ):
            root = Path(tmp)
            with self.assertRaisesRegex(RuntimeError, "version capture failed"):
                processing_jobs._stage_processing_output_destinations(
                    {"OUTPUT": str(root / "result.tif")},
                    [definition],
                )
            self.assertEqual(list(root.glob(".*.qcopilots-stage-*")), [])

    def test_processing_versions_hash_large_files_and_directory_contents(self):
        import qcopilots_common.processing_jobs as processing_jobs

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            large = root / "large.tif"
            large.write_bytes(b"a" * (16 * 1024 * 1024 + 1))
            original_stat = large.stat()
            first = processing_jobs._processing_file_version(large)
            with large.open("r+b") as handle:
                handle.seek(8 * 1024 * 1024)
                handle.write(b"b")
            os.utime(
                large,
                ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns),
            )
            second = processing_jobs._processing_file_version(large)
            self.assertIn("sha256", first)
            self.assertNotEqual(first["sha256"], second["sha256"])

            directory = root / "dataset"
            directory.mkdir()
            child = directory / "tile.bin"
            child.write_bytes(b"first")
            directory_stat = directory.stat()
            before = processing_jobs._processing_file_version(directory)
            child.write_bytes(b"other")
            os.utime(
                directory,
                ns=(directory_stat.st_atime_ns, directory_stat.st_mtime_ns),
            )
            after = processing_jobs._processing_file_version(directory)
            self.assertIn("manifest_sha256", before)
            self.assertNotEqual(
                before["manifest_sha256"], after["manifest_sha256"]
            )

    def test_folder_destinations_are_rejected_before_start_and_metadata_agrees(self):
        import qcopilots_common.processing_jobs as processing_jobs
        from qcopilots_common.processing_metadata import processing_algorithm_metadata

        definition = FakeProcessingDefinition(
            "FOLDER",
            destination=True,
            definition_type="folderDestination",
        )
        algorithm = FakeAlgorithm(
            "native:packagefolder",
            parameter_definitions=[definition],
        )
        metadata = processing_algorithm_metadata(algorithm)
        self.assertFalse(metadata["start_policy"]["supported"])
        self.assertEqual(
            metadata["start_policy"]["reason"],
            "folder_destination_atomic_publication_unsupported",
        )
        with tempfile.TemporaryDirectory() as tmp, self.assertRaisesRegex(
            RuntimeError,
            "folder destinations",
        ):
            processing_jobs._authorize_processing_output_destinations(
                algorithm,
                {"FOLDER": str(Path(tmp) / "new-folder")},
                [definition],
                False,
                "",
                processing_jobs.ProcessingOverwriteConfirmationStore(),
            )

    def test_formal_processing_policy_allows_global_local_inputs_and_destinations(self):
        import qcopilots_common.processing_jobs as processing_jobs
        from qcopilots_common.security_policy import filesystem_policy_from_config

        input_definition = FakeProcessingDefinition(
            "INPUT",
            destination=False,
            definition_type="vector",
        )
        output_definition = FakeProcessingDefinition(
            "OUTPUT",
            destination=True,
            definition_type="fileDestination",
        )
        file_definition = FakeProcessingDefinition(
            "STYLE",
            destination=False,
            definition_type="file",
        )
        folder_definition = FakeProcessingDefinition(
            "DIRECTORY",
            destination=False,
            definition_type="folder",
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            read_root = root / "read"
            write_root = root / "write"
            outside = root / "outside"
            for directory in (read_root, write_root, outside):
                directory.mkdir()
            source = read_root / "source.gpkg"
            source.write_bytes(b"input")
            style = read_root / "source.qml"
            style.write_text("style", encoding="utf-8")
            input_directory = read_root / "directory"
            input_directory.mkdir()
            outside_source = outside / "source.gpkg"
            outside_source.write_bytes(b"outside input")
            policy = filesystem_policy_from_config(
                {
                    "mode": "formal_restricted",
                    "shell": {"enabled": False, "executables": []},
                    "network": {"enabled": False, "allowed_origins": []},
                }
            )

            normalized = processing_jobs._apply_processing_filesystem_policy(
                {
                    "INPUT": str(source),
                    "STYLE": str(style),
                    "DIRECTORY": str(input_directory),
                    "OUTPUT": str(write_root / "result.gpkg"),
                },
                [
                    input_definition,
                    file_definition,
                    folder_definition,
                    output_definition,
                ],
                policy,
            )
            self.assertEqual(normalized["INPUT"], str(source.resolve()))
            self.assertEqual(normalized["STYLE"], str(style.resolve()))
            self.assertEqual(
                normalized["DIRECTORY"], str(input_directory.resolve())
            )
            self.assertEqual(
                normalized["OUTPUT"],
                str((write_root / "result.gpkg").resolve()),
            )
            global_paths = processing_jobs._apply_processing_filesystem_policy(
                {
                    "INPUT": str(outside_source),
                    "OUTPUT": str(outside / "result.gpkg"),
                },
                [input_definition, output_definition],
                policy,
            )
            self.assertEqual(global_paths["INPUT"], str(outside_source.resolve()))
            self.assertEqual(
                global_paths["OUTPUT"],
                str((outside / "result.gpkg").resolve()),
            )
            with self.assertRaisesRegex(PermissionError, "URI"):
                processing_jobs._apply_processing_filesystem_policy(
                    {
                        "INPUT": source.as_uri(),
                        "OUTPUT": str(write_root / "result.gpkg"),
                    },
                    [input_definition, output_definition],
                    policy,
                )
            for rejected in ("/vsimem/source.gpkg", "/vsicurl/http://example.test/a"):
                with self.subTest(rejected=rejected), self.assertRaisesRegex(
                    PermissionError, "virtual filesystem"
                ):
                    processing_jobs._apply_processing_filesystem_policy(
                        {"STYLE": rejected},
                        [file_definition],
                        policy,
                    )
            for rejected in (
                "dbname=C:\\outside\\source.gpkg",
                "postgresql://127.0.0.1/database",
            ):
                with self.subTest(rejected=rejected), self.assertRaisesRegex(
                    PermissionError, "local paths"
                ):
                    processing_jobs._apply_processing_filesystem_policy(
                        {"INPUT": rejected},
                        [input_definition],
                        policy,
                    )
            with self.assertRaisesRegex(PermissionError, "could not be resolved"):
                processing_jobs._apply_processing_filesystem_policy(
                    {"INPUT": "missing-layer-id"},
                    [input_definition],
                    policy,
                )
            with self.assertRaisesRegex(FileNotFoundError, "does not exist"):
                processing_jobs._apply_processing_filesystem_policy(
                    {"STYLE": str(read_root / "missing.qml")},
                    [file_definition],
                    policy,
                )

            versions = processing_jobs._processing_input_versions(
                normalized,
                [
                    input_definition,
                    file_definition,
                    folder_definition,
                    output_definition,
                ],
            )
            by_path = {item.get("path"): item for item in versions}
            self.assertIn("sha256", by_path[str(source.resolve())])
            self.assertIn("sha256", by_path[str(style.resolve())])
            self.assertIn(
                "manifest_sha256", by_path[str(input_directory.resolve())]
            )

    def test_compatible_processing_policy_validates_only_local_paths(self):
        import qcopilots_common.processing_jobs as processing_jobs
        from qcopilots_common.security_policy import FilesystemPolicy

        input_definition = FakeProcessingDefinition(
            "INPUT",
            destination=False,
            definition_type="vector",
        )
        file_definition = FakeProcessingDefinition(
            "STYLE",
            destination=False,
            definition_type="file",
        )
        output_definition = FakeProcessingDefinition(
            "OUTPUT",
            destination=True,
            definition_type="fileDestination",
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source.qml"
            source.write_text("style", encoding="utf-8")
            output = root / "result.gpkg"
            policy = FilesystemPolicy()

            normalized = processing_jobs._apply_processing_filesystem_policy(
                {
                    "STYLE": source.as_uri(),
                    "OUTPUT": Path(output),
                    "INPUT": "postgresql://127.0.0.1/database",
                },
                [file_definition, output_definition, input_definition],
                policy,
            )

            self.assertEqual(normalized["STYLE"], str(source.resolve()))
            self.assertEqual(normalized["OUTPUT"], output.resolve())
            self.assertEqual(
                normalized["INPUT"],
                "postgresql://127.0.0.1/database",
            )
            self.assertEqual(
                processing_jobs._apply_processing_filesystem_policy(
                    {"INPUT": "EPSG:4326"},
                    [input_definition],
                    policy,
                )["INPUT"],
                "EPSG:4326",
            )
            for drive_relative in (r"C:source.gpkg", r"C:..\source.gpkg"):
                for parameter_name, definition in (
                    ("INPUT", input_definition),
                    ("STYLE", file_definition),
                    ("OUTPUT", output_definition),
                ):
                    with self.subTest(
                        drive_relative=drive_relative,
                        parameter_name=parameter_name,
                    ), self.assertRaisesRegex(PermissionError, "drive-relative"):
                        processing_jobs._apply_processing_filesystem_policy(
                            {parameter_name: drive_relative},
                            [definition],
                            policy,
                        )
            self.assertIsNotNone(
                processing_jobs._processing_file_destination(r"C:result.gpkg")
            )
            local_provider = f"dbname='{source}' table=styles"
            provider_normalized = processing_jobs._apply_processing_filesystem_policy(
                {"INPUT": local_provider},
                [input_definition],
                policy,
            )
            self.assertEqual(provider_normalized["INPUT"], local_provider)
            database_provider = "host=127.0.0.1 dbname=qgis"
            self.assertEqual(
                processing_jobs._apply_processing_filesystem_policy(
                    {"INPUT": database_provider},
                    [input_definition],
                    policy,
                )["INPUT"],
                database_provider,
            )
            for connection_provider in (
                "host=127.0.0.1 dbname='tenant:2026'",
                "service=qgis dbname=qgis.prod",
            ):
                self.assertEqual(
                    processing_jobs._apply_processing_filesystem_policy(
                        {"INPUT": connection_provider},
                        [input_definition],
                        policy,
                    )["INPUT"],
                    connection_provider,
                )
            local_datasource = f"GPKG:{source}"
            self.assertEqual(
                processing_jobs._apply_processing_filesystem_policy(
                    {"INPUT": local_datasource},
                    [input_definition],
                    policy,
                )["INPUT"],
                local_datasource,
            )

            class ProjectLayer:
                def source(self):
                    return "nested/dbname=data.gpkg"

                def providerType(self):
                    return "ogr"

            nested = root / "nested"
            nested.mkdir()
            equals_source = nested / "dbname=data.gpkg"
            equals_source.write_bytes(b"data")
            previous_directory = Path.cwd()
            try:
                os.chdir(root)
                with mock.patch.object(
                    processing_jobs,
                    "_processing_project_layer_reference",
                    return_value=ProjectLayer(),
                ):
                    self.assertEqual(
                        processing_jobs._apply_processing_filesystem_policy(
                            {"INPUT": "layer-id"},
                            [input_definition],
                            policy,
                        )["INPUT"],
                        "layer-id",
                    )
            finally:
                os.chdir(previous_directory)
            virtual_path = processing_jobs._apply_processing_filesystem_policy(
                {"STYLE": "/vsicurl/http://example.test/source.qml"},
                [file_definition],
                policy,
            )
            self.assertEqual(
                virtual_path["STYLE"],
                "/vsicurl/http://example.test/source.qml",
            )

            for rejected in (r"\\?\C:\outside", r"\\server.invalid\share"):
                with self.subTest(rejected=rejected), self.assertRaises(
                    PermissionError
                ):
                    processing_jobs._apply_processing_filesystem_policy(
                        {"STYLE": rejected},
                        [file_definition],
                        policy,
                    )

            for rejected_provider in (
                r"dbname='\\server.invalid\share\source.gpkg' table=roads",
                r"dbname='\\?\C:\source.gpkg' table=roads",
                f"dbname='{source}:stream' table=roads",
                r"GPKG:\\server.invalid\share\source.gpkg",
                r"SQLite:\\?\C:\source.sqlite",
                f"GPKG:{source}:stream",
            ):
                with self.subTest(rejected_provider=rejected_provider), self.assertRaises(
                    PermissionError
                ):
                    processing_jobs._apply_processing_filesystem_policy(
                        {"INPUT": rejected_provider},
                        [input_definition],
                        policy,
                    )

    def test_compatible_processing_policy_runs_before_parameter_sanitizer(self):
        definition = FakeProcessingDefinition(
            "STYLE",
            destination=False,
            definition_type="file",
        )
        algorithm = FakeAlgorithm(
            "vector:validate-local-path",
            parameter_definitions=[definition],
        )
        environment = FakeProcessingEnvironment([algorithm])
        dependencies = environment.dependencies()
        sanitizer_calls = []

        def sanitizer(parameters, parameter_definitions):
            sanitizer_calls.append((parameters, parameter_definitions))
            return dict(parameters)

        dependencies["parameter_sanitizer"] = sanitizer
        manager = ProcessingJobManager(dependencies=dependencies)

        with self.assertRaisesRegex(PermissionError, "device paths"):
            manager.start(
                {
                    "category": "vector",
                    "algorithm_id": algorithm.id(),
                    "parameters": {"STYLE": r"\\?\C:\outside"},
                }
            )

        self.assertEqual(sanitizer_calls, [])
        self.assertEqual(environment.tasks, [])
        self.assertEqual(manager.list({"category": "vector"})["jobs"], [])

    def test_non_path_processing_strings_are_not_input_versions(self):
        import qcopilots_common.processing_jobs as processing_jobs

        expression_definition = FakeProcessingDefinition(
            "EXPRESSION",
            destination=False,
            definition_type="expression",
        )
        string_definition = FakeProcessingDefinition(
            "FIELD_NAME",
            destination=False,
            definition_type="string",
        )
        expressions = (
            '"population" / "area"',
            "column.suffix",
            'regexp_replace("value", "[<>:\"/\\\\|?*]", "")',
        )
        for value in expressions:
            with self.subTest(value=value):
                versions = processing_jobs._processing_input_versions(
                    {"EXPRESSION": value, "FIELD_NAME": value},
                    [expression_definition, string_definition],
                )
                self.assertEqual(versions, [])

    def test_project_layer_inputs_are_bound_to_confirmation_and_completion(self):
        import qcopilots_common.processing_jobs as processing_jobs

        class Layer:
            def __init__(self, source):
                self._source = str(source)
                self.selection = [2]

            def id(self):
                return "layer-id"

            def name(self):
                return "roads"

            def source(self):
                return self._source

            def providerType(self):
                return "ogr"

            def selectedFeatureIds(self):
                return list(self.selection)

            def isEditable(self):
                return False

            def isModified(self):
                return False

            def featureCount(self):
                return 1

            def fields(self):
                return []

            def subsetString(self):
                return ""

            def dataProvider(self):
                return None

        source_definition = FakeProcessingDefinition(
            "INPUT", destination=False, definition_type="vector"
        )
        output_definition = FakeProcessingDefinition("OUTPUT", destination=True)
        algorithm = FakeAlgorithm(
            "native:copy",
            parameter_definitions=[source_definition, output_definition],
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "roads.geojson"
            source.write_text('{"type":"FeatureCollection","features":[]}', encoding="utf-8")
            output = root / "result.geojson"
            output.write_text("old", encoding="utf-8")
            layer = Layer(source)
            store = processing_jobs.ProcessingOverwriteConfirmationStore()
            with mock.patch.object(
                processing_jobs,
                "_processing_project_layer_reference",
                return_value=layer,
            ):
                versions = processing_jobs._processing_input_versions(
                    {"INPUT": "layer-id", "OUTPUT": str(output)},
                    [source_definition, output_definition],
                )
                preview = processing_jobs._authorize_processing_output_destinations(
                    algorithm,
                    {"INPUT": "layer-id", "OUTPUT": str(output)},
                    [source_definition, output_definition],
                    True,
                    "",
                    store,
                )
            layer_version = next(
                item for item in versions if item.get("kind") == "project_layer"
            )
            self.assertIn("overwrite_confirmation_token", preview)
            self.assertTrue(layer_version["immutable_file_snapshot"])
            layer.selection = [3]
            with mock.patch.object(
                processing_jobs,
                "_processing_project_layer_by_id",
                return_value=layer,
            ), self.assertRaisesRegex(RuntimeError, "layer:layer-id"):
                processing_jobs._validate_processing_versions(
                    [layer_version],
                    "Processing input changed",
                )

    def test_non_file_project_layer_content_is_bound_to_job_version(self):
        import qcopilots_common.processing_jobs as processing_jobs

        class Geometry:
            def __init__(self, value):
                self.value = value

            def isNull(self):
                return False

            def asWkb(self):
                return self.value

        class Feature:
            def __init__(self, feature_id, value):
                self.feature_id = feature_id
                self.value = value

            def id(self):
                return self.feature_id

            def attributes(self):
                return [self.value]

            def geometry(self):
                return Geometry(b"point")

        class Layer:
            value = "first"

            def id(self):
                return "memory-layer"

            def name(self):
                return "scratch"

            def source(self):
                return "memory:scratch"

            def providerType(self):
                return "memory"

            def selectedFeatureIds(self):
                return []

            def isEditable(self):
                return False

            def isModified(self):
                return False

            def featureCount(self):
                return 1

            def fields(self):
                return []

            def subsetString(self):
                return ""

            def dataProvider(self):
                return None

            def getFeatures(self):
                return [Feature(1, self.value)]

        definition = FakeProcessingDefinition(
            "INPUT", destination=False, definition_type="vector"
        )
        layer = Layer()
        with mock.patch.object(
            processing_jobs,
            "_processing_project_layer_reference",
            return_value=layer,
        ):
            version = next(
                item
                for item in processing_jobs._processing_input_versions(
                    {"INPUT": "memory-layer"}, [definition]
                )
                if item.get("kind") == "project_layer"
            )
        self.assertIn("content_fingerprint", version)
        layer.value = "second"
        with mock.patch.object(
            processing_jobs,
            "_processing_project_layer_by_id",
            return_value=layer,
        ), self.assertRaisesRegex(RuntimeError, "layer:memory-layer"):
            processing_jobs._validate_processing_versions(
                [version], "Processing input changed"
            )

    def test_overwrite_publish_race_preserves_intruder_and_original_backup(self):
        import qcopilots_common.processing_jobs as processing_jobs

        definition = FakeProcessingDefinition("OUTPUT", destination=True)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / "race.tif"
            target.write_bytes(b"original")
            _, stages = processing_jobs._stage_processing_output_destinations(
                {"OUTPUT": str(target)}, [definition]
            )
            stages[0]["staged_target"].write_bytes(b"job-output")
            real_link = processing_jobs._link_processing_file_no_clobber

            def race_after_backup(source, destination):
                Path(destination).write_bytes(b"intruder")
                return real_link(source, destination)

            with mock.patch.object(
                processing_jobs,
                "_link_processing_file_no_clobber",
                side_effect=race_after_backup,
            ), self.assertRaisesRegex(RuntimeError, "rollback was incomplete"):
                processing_jobs._promote_processing_outputs(stages, overwrite=True)
            self.assertEqual(target.read_bytes(), b"intruder")
            backups = list(root.glob(".*.qcopilots-backup-*/*"))
            self.assertEqual(len(backups), 1)
            self.assertEqual(backups[0].read_bytes(), b"original")

    def test_overwrite_confirmation_binds_sidecars_inputs_and_is_one_use(self):
        from qcopilots_common.processing_jobs import (
            ProcessingOverwriteConfirmationStore,
            _authorize_processing_output_destinations,
        )

        definitions = [
            FakeProcessingDefinition("INPUT", destination=False),
            FakeProcessingDefinition("OUTPUT", destination=True),
        ]
        algorithm = FakeAlgorithm(
            "raster:controlled",
            parameter_definitions=definitions,
        )
        store = ProcessingOverwriteConfirmationStore()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source.shp"
            source.write_bytes(b"shape")
            source.with_suffix(".dbf").write_bytes(b"attributes-v1")
            target = root / "target.tif"
            target.write_bytes(b"raster-v1")
            target.with_name(target.name + ".ovr").write_bytes(b"overview-v1")
            parameters = {"INPUT": str(source), "OUTPUT": str(target)}

            with self.assertRaisesRegex(FileExistsError, "overwrite preview"):
                _authorize_processing_output_destinations(
                    algorithm, parameters, definitions, False, "", store
                )
            preview = _authorize_processing_output_destinations(
                algorithm, parameters, definitions, True, "", store
            )
            self.assertTrue(preview["confirmation_required"])
            self.assertEqual(
                {Path(item["path"]).name for item in preview["overwrite_targets"]},
                {"target.tif", "target.tif.ovr"},
            )

            source.with_suffix(".dbf").write_bytes(b"attributes-v2")
            with self.assertRaisesRegex(RuntimeError, "inputs.*changed|parameters"):
                _authorize_processing_output_destinations(
                    algorithm,
                    parameters,
                    definitions,
                    True,
                    preview["overwrite_confirmation_token"],
                    store,
                )
            refreshed = _authorize_processing_output_destinations(
                algorithm, parameters, definitions, True, "", store
            )
            self.assertIsNone(
                _authorize_processing_output_destinations(
                    algorithm,
                    parameters,
                    definitions,
                    True,
                    refreshed["overwrite_confirmation_token"],
                    store,
                )
            )
            with self.assertRaisesRegex(RuntimeError, "invalid or already used"):
                _authorize_processing_output_destinations(
                    algorithm,
                    parameters,
                    definitions,
                    True,
                    refreshed["overwrite_confirmation_token"],
                    store,
                )

    def test_in_place_and_existing_container_destinations_are_denied(self):
        from qcopilots_common.processing_jobs import (
            ProcessingOverwriteConfirmationStore,
            _authorize_processing_output_destinations,
        )

        store = ProcessingOverwriteConfirmationStore()
        input_definition = FakeProcessingDefinition("INPUT", destination=False)
        output_definition = FakeProcessingDefinition("OUTPUT", destination=True)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            raster = root / "original.tif"
            raster.write_bytes(b"original raster")
            before = raster.read_bytes()
            in_place = FakeAlgorithm(
                "gdal:overviews",
                parameter_definitions=[input_definition],
            )
            with self.assertRaisesRegex(RuntimeError, "no separate destination"):
                _authorize_processing_output_destinations(
                    in_place,
                    {"INPUT": str(raster)},
                    [input_definition],
                    False,
                    "",
                    store,
                )
            self.assertEqual(raster.read_bytes(), before)

            container = root / "existing.gpkg"
            connection = sqlite3.connect(container)
            try:
                connection.execute("CREATE TABLE unrelated (value TEXT)")
                connection.execute("INSERT INTO unrelated VALUES ('preserved')")
                connection.commit()
            finally:
                connection.close()
            isolated = FakeAlgorithm(
                "vector:controlled",
                parameter_definitions=[output_definition],
            )
            with self.assertRaisesRegex(RuntimeError, "existing container"):
                _authorize_processing_output_destinations(
                    isolated,
                    {"OUTPUT": f"{container}|layername=new_layer"},
                    [output_definition],
                    True,
                    "",
                    store,
                )
            connection = sqlite3.connect(container)
            try:
                self.assertEqual(
                    connection.execute("SELECT value FROM unrelated").fetchone()[0],
                    "preserved",
                )
            finally:
                connection.close()
            with self.assertRaisesRegex(RuntimeError, "provider/database URIs"):
                _authorize_processing_output_destinations(
                    isolated,
                    {"OUTPUT": "postgres:dbname='test' table=results"},
                    [output_definition],
                    False,
                    "",
                    store,
                )

    def test_existing_directory_output_is_denied_when_child_changes_are_hidden(self):
        from qcopilots_common.processing_jobs import (
            ProcessingOverwriteConfirmationStore,
            _authorize_processing_output_destinations,
        )

        definition = FakeProcessingDefinition("OUTPUT", destination=True)
        algorithm = FakeAlgorithm(
            "raster:directory-output",
            parameter_definitions=[definition],
        )
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "dataset-directory"
            target.mkdir()
            child = target / "tile.bin"
            child.write_bytes(b"version-one")
            directory_stat = target.stat()

            with self.assertRaisesRegex(RuntimeError, "existing directory"):
                _authorize_processing_output_destinations(
                    algorithm,
                    {"OUTPUT": str(target)},
                    [definition],
                    True,
                    "",
                    ProcessingOverwriteConfirmationStore(),
                )

            child.write_bytes(b"version-two")
            os.utime(
                target,
                ns=(directory_stat.st_atime_ns, directory_stat.st_mtime_ns),
            )
            self.assertEqual(target.stat().st_mtime_ns, directory_stat.st_mtime_ns)
            with self.assertRaisesRegex(RuntimeError, "existing directory"):
                _authorize_processing_output_destinations(
                    algorithm,
                    {"OUTPUT": str(target)},
                    [definition],
                    True,
                    "",
                    ProcessingOverwriteConfirmationStore(),
                )
            self.assertEqual(child.read_bytes(), b"version-two")

    def test_staged_outputs_recheck_targets_rollback_and_retry_cleanly(self):
        from qcopilots_common.processing_jobs import (
            _cleanup_processing_stages,
            _commit_processing_output_promotion,
            _promote_processing_outputs,
            _rollback_processing_output_promotion,
            _stage_processing_output_destinations,
            _validate_processing_stage_targets,
        )

        definition = FakeProcessingDefinition("OUTPUT", destination=True)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / "target.tif"
            target.write_bytes(b"old raster")
            sidecar = target.with_name(target.name + ".aux.xml")
            sidecar.write_bytes(b"old metadata")
            _, stages = _stage_processing_output_destinations(
                {"OUTPUT": str(target)}, [definition]
            )
            staged = stages[0]["staged_target"]
            staged.write_bytes(b"new raster")
            staged.with_name(staged.name + ".aux.xml").write_bytes(b"new metadata")

            target.write_bytes(b"concurrent change")
            with self.assertRaisesRegex(RuntimeError, "changed after authorization"):
                _validate_processing_stage_targets(stages)
            self.assertEqual(target.read_bytes(), b"concurrent change")
            self.assertEqual(_cleanup_processing_stages(stages), [])

            target.write_bytes(b"old raster")
            _, retry_stages = _stage_processing_output_destinations(
                {"OUTPUT": str(target)}, [definition]
            )
            retry_staged = retry_stages[0]["staged_target"]
            retry_staged.write_bytes(b"new raster")
            retry_staged.with_name(retry_staged.name + ".aux.xml").write_bytes(
                b"new metadata"
            )
            receipt = _promote_processing_outputs(retry_stages, overwrite=True)
            self.assertEqual(target.read_bytes(), b"new raster")
            rollback = _rollback_processing_output_promotion(receipt)
            self.assertTrue(rollback["complete"], rollback)
            self.assertEqual(target.read_bytes(), b"old raster")
            self.assertEqual(sidecar.read_bytes(), b"old metadata")
            self.assertEqual(_cleanup_processing_stages(retry_stages), [])

            _, final_stages = _stage_processing_output_destinations(
                {"OUTPUT": str(target)}, [definition]
            )
            final_staged = final_stages[0]["staged_target"]
            final_staged.write_bytes(b"final raster")
            final_receipt = _promote_processing_outputs(final_stages, overwrite=True)
            self.assertEqual(_commit_processing_output_promotion(final_receipt), [])
            self.assertEqual(_cleanup_processing_stages(final_stages), [])
            self.assertEqual(target.read_bytes(), b"final raster")
            self.assertEqual(list(root.glob(".*.qcopilots-*-*")), [])

    def test_multi_output_stage_creation_failure_unwinds_prior_stages(self):
        import qcopilots_common.processing_jobs as processing_jobs

        definitions = [
            FakeProcessingDefinition("FIRST", destination=True),
            FakeProcessingDefinition("SECOND", destination=True),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            real_mkdtemp = processing_jobs.tempfile.mkdtemp
            calls = 0

            def fail_second_stage(*arguments, **kwargs):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise OSError("second stage creation failed")
                return real_mkdtemp(*arguments, **kwargs)

            with mock.patch.object(
                processing_jobs.tempfile,
                "mkdtemp",
                side_effect=fail_second_stage,
            ):
                with self.assertRaisesRegex(OSError, "second stage"):
                    processing_jobs._stage_processing_output_destinations(
                        {
                            "FIRST": str(root / "first.tif"),
                            "SECOND": str(root / "second.tif"),
                        },
                        definitions,
                    )
            self.assertEqual(list(root.glob(".*.qcopilots-stage-*")), [])

    def test_absent_output_race_never_replaces_unconfirmed_file(self):
        import qcopilots_common.processing_jobs as processing_jobs

        definition = FakeProcessingDefinition("OUTPUT", destination=True)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / "race.tif"
            _, stages = processing_jobs._stage_processing_output_destinations(
                {"OUTPUT": str(target)}, [definition]
            )
            stages[0]["staged_target"].write_bytes(b"job-output")
            real_link = processing_jobs.os.link

            def create_racing_target(source, destination):
                Path(destination).write_bytes(b"concurrent-owner")
                return real_link(source, destination)

            with mock.patch.object(
                processing_jobs.os,
                "link",
                side_effect=create_racing_target,
            ):
                with self.assertRaises(FileExistsError):
                    processing_jobs._promote_processing_outputs(
                        stages, overwrite=True
                    )
            self.assertEqual(target.read_bytes(), b"concurrent-owner")
            self.assertEqual(
                processing_jobs._cleanup_processing_stages(stages), []
            )

    def test_output_rollback_continues_after_locked_target_and_is_retryable(self):
        import qcopilots_common.processing_jobs as processing_jobs

        definitions = [
            FakeProcessingDefinition("FIRST", destination=True),
            FakeProcessingDefinition("SECOND", destination=True),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            first = root / "first.tif"
            second = root / "second.tif"
            first.write_bytes(b"first-original")
            second.write_bytes(b"second-original")
            _, stages = processing_jobs._stage_processing_output_destinations(
                {"FIRST": str(first), "SECOND": str(second)}, definitions
            )
            stages[0]["staged_target"].write_bytes(b"first-new")
            stages[1]["staged_target"].write_bytes(b"second-new")
            receipt = processing_jobs._promote_processing_outputs(
                stages, overwrite=True
            )

            real_remove = processing_jobs._remove_processing_path

            def locked_first(path):
                if Path(path).resolve(strict=False) == first.resolve(strict=False):
                    raise PermissionError("first target is locked")
                real_remove(path)

            with mock.patch.object(
                processing_jobs, "_remove_processing_path", side_effect=locked_first
            ):
                rollback = processing_jobs._rollback_processing_output_promotion(
                    receipt
                )

            self.assertFalse(rollback["complete"])
            self.assertIn(
                first.resolve(strict=False),
                {
                    Path(path).resolve(strict=False)
                    for path in rollback["residual_paths"]
                },
            )
            self.assertEqual(first.read_bytes(), b"first-new")
            self.assertEqual(second.read_bytes(), b"second-original")
            second_backup = next(
                item
                for item in receipt["backups"]
                if Path(item["target"]).resolve(strict=False)
                == second.resolve(strict=False)
            )
            self.assertEqual(
                processing_jobs._processing_version_signature(
                    processing_jobs._processing_file_version(second)
                ),
                processing_jobs._processing_version_signature(
                    second_backup["original_version"]
                ),
            )
            self.assertTrue(
                any(Path(item["backup"]).exists() for item in receipt["backups"])
            )

            retried = processing_jobs._rollback_processing_output_promotion(receipt)
            self.assertTrue(retried["complete"], retried)
            self.assertEqual(first.read_bytes(), b"first-original")
            self.assertEqual(second.read_bytes(), b"second-original")
            self.assertEqual(
                processing_jobs._cleanup_processing_stages(stages), []
            )

    def test_locked_stage_cleanup_reports_residual_and_can_be_retried(self):
        import qcopilots_common.processing_jobs as processing_jobs

        definition = FakeProcessingDefinition("OUTPUT", destination=True)
        with tempfile.TemporaryDirectory() as tmp:
            _, stages = processing_jobs._stage_processing_output_destinations(
                {"OUTPUT": str(Path(tmp) / "result.tif")}, [definition]
            )
            stage_root = stages[0]["staging_root"]
            stages[0]["staged_target"].write_bytes(b"partial")
            real_rmtree = processing_jobs.shutil.rmtree

            def locked_stage(path, *arguments, **kwargs):
                if Path(path) == stage_root:
                    raise PermissionError("stage is locked")
                return real_rmtree(path, *arguments, **kwargs)

            with mock.patch.object(
                processing_jobs.shutil, "rmtree", side_effect=locked_stage
            ):
                residuals = processing_jobs._cleanup_processing_stages(stages)
            self.assertEqual(residuals, [str(stage_root)])
            self.assertTrue(stage_root.exists())
            with mock.patch.object(processing_jobs.shutil, "rmtree"):
                residuals = processing_jobs._cleanup_processing_stages(stages)
            self.assertEqual(residuals, [str(stage_root)])
            self.assertTrue(stage_root.exists())
            self.assertEqual(processing_jobs._cleanup_processing_stages(stages), [])
            self.assertFalse(stage_root.exists())

    def test_processing_output_guard_requires_explicit_overwrite_and_cleans_nothing(self):
        from qcopilots_common.processing_jobs import (
            _guard_processing_output_destinations,
        )

        class DestinationDefinition:
            def name(self):
                return "OUTPUT"

            def isDestination(self):
                return True

        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "existing.tif"
            output.write_bytes(b"original")
            parameters = {"OUTPUT": str(output)}
            with self.assertRaisesRegex(FileExistsError, "overwrite_outputs"):
                _guard_processing_output_destinations(
                    parameters,
                    [DestinationDefinition()],
                    False,
                )
            self.assertEqual(output.read_bytes(), b"original")
            _guard_processing_output_destinations(
                parameters,
                [DestinationDefinition()],
                True,
            )
            _guard_processing_output_destinations(
                {"OUTPUT": "TEMPORARY_OUTPUT"},
                [DestinationDefinition()],
                False,
            )

    def test_staged_output_rewrites_context_before_project_postprocessing(self):
        definition = FakeProcessingDefinition("OUTPUT", destination=True)
        environment = FakeProcessingEnvironment(
            [
                FakeAlgorithm(
                    "vector:buffer",
                    parameter_definitions=[definition],
                )
            ]
        )
        dependencies = environment.dependencies()
        context = FakeLoadContext()
        dependencies["context_factory"] = lambda feedback: context
        observed = {}

        def post_processor(algorithm, processing_context, feedback, results):
            del algorithm, feedback
            observed["destinations"] = dict(
                processing_context.layersToLoadOnCompletion()
            )
            observed["results"] = dict(results)
            return all(Path(path).is_file() for path in observed["destinations"])

        dependencies["post_processor"] = post_processor
        manager = ProcessingJobManager(dependencies=dependencies)
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "published.gpkg"
            started = manager.start(
                {
                    "category": "vector",
                    "algorithm_id": "vector:buffer",
                    "parameters": {"OUTPUT": str(target)},
                    "add_outputs_to_project": True,
                }
            )
            runtime = manager.store.get_runtime(started["job_id"], "vector")
            self.assertEqual(len(runtime.output_stages), 1)
            staged = runtime.output_stages[0]["staged_target"]
            staged.write_bytes(b"staged output")
            details = object()
            context.layers = {str(staged): details}

            environment.tasks[0].begun.emit()
            environment.tasks[0].executed.emit(
                True,
                {"OUTPUT": str(staged)},
            )

            succeeded = manager.get(
                {"job_id": started["job_id"], "category": "vector"}
            )
            self.assertEqual(succeeded["state"], "succeeded")
            canonical_target = target.resolve(strict=False)
            self.assertEqual(
                Path(succeeded["result"]["OUTPUT"]).resolve(strict=False),
                canonical_target,
            )
            self.assertEqual(
                Path(observed["results"]["OUTPUT"]).resolve(strict=False),
                canonical_target,
            )
            self.assertEqual(
                {
                    Path(path).resolve(strict=False): value
                    for path, value in observed["destinations"].items()
                },
                {canonical_target: details},
            )
            self.assertEqual(target.read_bytes(), b"staged output")
            self.assertFalse(staged.exists())

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
        self.assertFalse(started["overwrite_outputs"])
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

        layer_snapshot = {
            "layer_id": "layer-1",
            "name": "Generated layer",
            "source": "memory:generated",
            "provider": "memory",
            "layer_type": "VectorLayer",
        }
        with mock.patch(
            "qcopilots_common.processing_jobs._map_layer_snapshot",
            side_effect=lambda value: (
                layer_snapshot if isinstance(value, FakeMapLayer) else None
            ),
        ):
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

    def test_partial_postprocessing_failure_removes_only_job_added_layers(self):
        import qcopilots_common.processing_jobs as processing_jobs

        environment = FakeProcessingEnvironment([FakeAlgorithm("vector:buffer")])
        dependencies = environment.dependencies()
        project_layer_ids = {"preexisting"}
        removed = []

        def partial_postprocess(*arguments):
            del arguments
            project_layer_ids.update({"job-output-a", "job-output-b"})
            return False

        def remove_layers(layer_ids):
            removed.extend(sorted(layer_ids))
            project_layer_ids.difference_update(layer_ids)

        dependencies["post_processor"] = partial_postprocess
        manager = ProcessingJobManager(dependencies=dependencies)
        with mock.patch.object(
            processing_jobs,
            "_processing_project_layer_ids",
            side_effect=lambda: set(project_layer_ids),
        ), mock.patch.object(
            processing_jobs,
            "_remove_processing_project_layers",
            side_effect=remove_layers,
        ):
            started = manager.start(
                {
                    "category": "vector",
                    "algorithm_id": "vector:buffer",
                    "add_outputs_to_project": True,
                }
            )
            environment.tasks[0].begun.emit()
            environment.tasks[0].executed.emit(
                True, {"OUTPUT": "memory:partially-loaded"}
            )

        failed = manager.get(started["job_id"])
        self.assertEqual(failed["state"], "failed")
        self.assertEqual(removed, ["job-output-a", "job-output-b"])
        self.assertEqual(project_layer_ids, {"preexisting"})

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
        environment.tasks[0].destroyed.emit()
        self.assertIsNone(retained_runtime.task)
        self.assertIs(retained_runtime.context, retained_context)

    def test_disk_results_release_context_after_task_destruction(self):
        input_definition = FakeProcessingDefinition(
            "INPUT",
            destination=False,
            definition_type="raster",
        )
        environment = FakeProcessingEnvironment(
            [
                FakeAlgorithm(
                    "raster:warp",
                    parameter_definitions=[input_definition],
                )
            ]
        )

        class FileInputContext:
            def __init__(self, path):
                self._handle = path.open("rb")

            def __del__(self):
                self._handle.close()

        dependencies = environment.dependencies()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            input_path = root / "input.asc"
            output_path = root / "output.tif"
            input_path.write_text("ncols 1\nnrows 1\n1\n", encoding="ascii")
            dependencies["context_factory"] = (
                lambda feedback: FileInputContext(input_path)
            )
            manager = ProcessingJobManager(dependencies=dependencies)
            runtime = None
            task = None
            try:
                started = manager.start(
                    {
                        "category": "raster",
                        "algorithm_id": "raster:warp",
                        "parameters": {
                            "INPUT": str(input_path),
                            "OUTPUT": str(output_path),
                        },
                        "add_outputs_to_project": False,
                    }
                )
                runtime = manager.store.get_runtime(started["job_id"], "raster")
                task = environment.tasks[0]
                output_path.write_bytes(b"disk output")
                task.begun.emit()
                task.executed.emit(True, {"OUTPUT": str(output_path)})

                succeeded = manager.get(started["job_id"])
                self.assertEqual(succeeded["state"], "succeeded")
                self.assertFalse(runtime.retain_context_for_results)
                self.assertIsNotNone(runtime.context)

                task.destroyed.emit()
                self.assertIsNone(runtime.task)
                self.assertIsNone(runtime.context)
                input_path.unlink()
                self.assertFalse(input_path.exists())
            finally:
                if (
                    runtime is not None
                    and runtime.context is not None
                    and task is not None
                ):
                    task.destroyed.emit()

    def test_disk_result_does_not_retain_context_for_file_input_layer(self):
        import qcopilots_common.processing_jobs as processing_jobs

        class FileInputLayer:
            def id(self):
                return "input-layer"

        class TemporaryLayerStore:
            def mapLayers(self):
                return {"input-layer": FileInputLayer()}

        class ProcessingContext:
            def temporaryLayerStore(self):
                return TemporaryLayerStore()

        context = ProcessingContext()
        self.assertFalse(
            processing_jobs._processing_results_require_context(
                {"OUTPUT": str(Path(tempfile.gettempdir()) / "output.tif")},
                context,
            )
        )
        self.assertTrue(
            processing_jobs._processing_results_require_context(
                {"OUTPUT": "memory:retained-output"},
                context,
            )
        )

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
        self.assertEqual(manager.get(started["job_id"])["state"], "cancelled")

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
        self.assertEqual(succeeded["state"], "cancelled")
        self.assertTrue(succeeded["cancel_requested"])
        task.taskTerminated.emit()
        self.assertEqual(manager.get(started["job_id"]), succeeded)

    def test_shutdown_retains_stages_until_noncooperative_task_finishes(self):
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
        runtime = manager.store.get_runtime(started["job_id"], "vector")
        with tempfile.TemporaryDirectory() as tmp:
            stage_root = Path(tmp) / "stage"
            stage_root.mkdir()
            staged_target = stage_root / "result.tif"
            staged_target.write_bytes(b"still being written")
            runtime.output_stages = [
                {
                    "parameter": "OUTPUT",
                    "target": Path(tmp) / "result.tif",
                    "family_paths": [Path(tmp) / "result.tif"],
                    "staging_root": stage_root,
                    "staged_target": staged_target,
                    "authorized_versions": [],
                }
            ]
            task.begun.emit()

            manager.shutdown(timeout_seconds=0.01)
            self.assertEqual(manager.get(started["job_id"])["state"], "cancelling")
            self.assertTrue(stage_root.exists())
            self.assertIs(runtime.task, task)

            task.executed.emit(True, {"OUTPUT": str(staged_target)})
            cancelled = manager.get(started["job_id"])
            self.assertEqual(cancelled["state"], "cancelled")
            self.assertFalse(stage_root.exists())
            self.assertIsNone(runtime.task)

    def test_task_destruction_retains_and_retries_locked_recovery_state(self):
        import qcopilots_common.processing_jobs as processing_jobs

        environment = FakeProcessingEnvironment([FakeAlgorithm("vector:failed")])
        manager = ProcessingJobManager(dependencies=environment.dependencies())
        started = manager.start(
            {"category": "vector", "algorithm_id": "vector:failed"}
        )
        runtime = manager.store.get_runtime(started["job_id"], "vector")
        environment.tasks[0].begun.emit()
        environment.tasks[0].executed.emit(False, {})
        self.assertEqual(manager.get(started["job_id"])["state"], "failed")

        with tempfile.TemporaryDirectory() as tmp:
            stage_root = Path(tmp) / "locked-stage"
            stage_root.mkdir()
            staged_target = stage_root / "result.tif"
            staged_target.write_bytes(b"partial")
            runtime.output_stages = [
                {
                    "parameter": "OUTPUT",
                    "target": Path(tmp) / "result.tif",
                    "family_paths": [Path(tmp) / "result.tif"],
                    "staging_root": stage_root,
                    "staged_target": staged_target,
                    "authorized_versions": [],
                }
            ]
            real_rmtree = processing_jobs.shutil.rmtree

            def locked_cleanup(path, *arguments, **kwargs):
                if Path(path) == stage_root:
                    raise PermissionError("stage is still locked")
                return real_rmtree(path, *arguments, **kwargs)

            with mock.patch.object(
                processing_jobs.shutil,
                "rmtree",
                side_effect=locked_cleanup,
            ) as rmtree_mock:
                manager._release_finished_runtime(
                    started["job_id"], "vector", runtime
                )
                self.assertTrue(rmtree_mock.called)
                self.assertEqual(runtime.cleanup_residual_paths, [str(stage_root)])
                self.assertTrue(runtime.output_stages)
            retained = manager.store.get(started["job_id"], "vector")
            self.assertFalse(retained["cleanup"]["complete"])
            self.assertTrue(retained["cleanup"]["retry_recommended"])
            self.assertEqual(retained["cleanup"]["residual_paths"], [str(stage_root)])
            self.assertTrue(runtime.output_stages)
            self.assertEqual(runtime.cleanup_residual_paths, [str(stage_root)])
            self.assertIsNone(runtime.task)

            recovered = manager.get(started["job_id"])
            self.assertTrue(recovered["cleanup"]["complete"])
            self.assertFalse(recovered["cleanup"]["retry_recommended"])
            self.assertEqual(recovered["cleanup"]["residual_paths"], [])
            self.assertEqual(runtime.output_stages, [])
            self.assertEqual(runtime.cleanup_residual_paths, [])

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

    def test_general_native_file_inputs_allow_global_paths_and_are_versioned(self):
        import qcopilots_common.processing_jobs as processing_jobs
        from qcopilots_common.security_policy import filesystem_policy_from_config

        algorithm_ids = (
            "native:b3dmtogltf",
            "native:categorizeusingstyle",
            "native:gltftovector",
            "native:stylefromproject",
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            read_root = root / "read"
            outside_root = root / "outside"
            for directory in (read_root, outside_root):
                directory.mkdir()
            inside = read_root / "input.style"
            outside = outside_root / "input.style"
            inside.write_bytes(b"inside")
            outside.write_bytes(b"outside")
            policy = filesystem_policy_from_config(
                {
                    "mode": "formal_restricted",
                    "shell": {"enabled": False, "executables": []},
                    "network": {"enabled": False, "allowed_origins": []},
                }
            )

            for algorithm_id in algorithm_ids:
                with self.subTest(algorithm_id=algorithm_id):
                    algorithm = self.registry.algorithmById(algorithm_id)
                    self.assertIsNotNone(algorithm, algorithm_id)
                    definitions = [
                        item
                        for item in algorithm.parameterDefinitions()
                        if not item.isDestination()
                        and "file"
                        in str(item.type()).replace("_", "").lower()
                    ]
                    self.assertTrue(definitions, algorithm_id)
                    definition = definitions[0]
                    name = str(definition.name())
                    for input_path in (inside, outside):
                        normalized = (
                            processing_jobs._apply_processing_filesystem_policy(
                                {name: str(input_path)},
                                [definition],
                                policy,
                            )
                        )
                        versions = processing_jobs._processing_input_versions(
                            normalized,
                            [definition],
                        )
                        self.assertEqual(len(versions), 1)
                        self.assertEqual(
                            versions[0]["path"],
                            str(input_path.resolve()),
                        )
                        self.assertIn("sha256", versions[0])


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
