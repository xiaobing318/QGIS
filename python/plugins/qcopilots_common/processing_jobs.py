"""Persistent asynchronous QGIS Processing jobs for the QCopilots bridge.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import hashlib
import json
import math
import threading
import time
import uuid
from dataclasses import dataclass, field
from datetime import date, datetime, time as datetime_time, timezone
from enum import Enum
from pathlib import Path
from typing import Any

from qcopilots_common.async_jobs import (
    AsyncJobStore,
    PUBLIC_JOB_STATES,
    TERMINAL_JOB_STATES,
    utc_now_rfc3339,
)


def json_safe_value(value: Any, _seen: set[int] | None = None) -> Any:
    """Recursively convert Processing results to strict JSON-compatible data."""

    if value is None or isinstance(value, (str, bool, int)):
        return value
    if isinstance(value, float):
        return value if math.isfinite(value) else None
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, datetime):
        normalized = value
        if normalized.tzinfo is None:
            normalized = normalized.replace(tzinfo=timezone.utc)
        return normalized.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")
    if isinstance(value, (date, datetime_time)):
        return value.isoformat()
    if isinstance(value, Enum):
        return json_safe_value(value.value, _seen)

    seen = _seen if _seen is not None else set()
    value_id = id(value)
    if value_id in seen:
        return "<recursive>"
    seen.add(value_id)
    try:
        if isinstance(value, dict):
            return {
                str(key): json_safe_value(item, seen) for key, item in value.items()
            }
        if isinstance(value, (list, tuple, set, frozenset)):
            return [json_safe_value(item, seen) for item in value]

        map_layer = _map_layer_snapshot(value)
        if map_layer is not None:
            return map_layer

        # Qt date and time values expose ISO-aware conversion without requiring
        # qgis imports at module import time.
        class_name = type(value).__name__
        if class_name in {"QDateTime", "QDate", "QTime"} and hasattr(
            value, "toString"
        ):
            try:
                from qgis.PyQt.QtCore import Qt

                date_format = (
                    Qt.DateFormat.ISODateWithMs
                    if class_name == "QDateTime"
                    else Qt.DateFormat.ISODate
                )
                return str(value.toString(date_format))
            except Exception:
                return str(value.toString())

        if hasattr(value, "asWkt"):
            try:
                return str(value.asWkt())
            except Exception:
                pass
        if hasattr(value, "toString"):
            try:
                return str(value.toString())
            except Exception:
                pass
        return str(value)
    finally:
        seen.discard(value_id)


def _map_layer_snapshot(value: Any) -> dict[str, Any] | None:
    try:
        from qgis.core import QgsMapLayer

        if not isinstance(value, QgsMapLayer):
            return None
    except Exception:
        # Lightweight tests may provide a duck-typed map layer without QGIS.
        if not all(hasattr(value, name) for name in ("id", "name", "source")):
            return None
        if "layer" not in type(value).__name__.lower():
            return None

    provider = None
    try:
        provider = value.providerType()
    except Exception:
        try:
            provider = value.dataProvider().name()
        except Exception:
            provider = None
    layer_type = None
    try:
        layer_type_value = value.type()
        layer_type = getattr(layer_type_value, "name", None) or str(layer_type_value)
    except Exception:
        pass
    return {
        "layer_id": str(value.id()),
        "name": str(value.name()),
        "source": str(value.source()),
        "provider": str(provider) if provider is not None else None,
        "layer_type": str(layer_type) if layer_type is not None else None,
    }


@dataclass
class _ProcessingRuntime:
    algorithm: Any
    parameters: dict[str, Any]
    context: Any
    feedback: Any
    add_outputs_to_project: bool
    generation: int
    task: Any = None
    feedback_messages: list[str] = field(default_factory=list)
    sink_counts: dict[str, int] = field(default_factory=dict)
    lock: threading.RLock = field(default_factory=threading.RLock)

    def release_completed_objects(self, *, keep_context: bool) -> None:
        """Release QGIS objects after their task no longer references context."""

        with self.lock:
            # Drop the task wrapper first. QgsProcessingAlgRunnerTask stores a
            # reference to its context, so context must outlive the task object.
            self.task = None
            self.algorithm = None
            self.feedback = None
            self.parameters = {}
            self.sink_counts.clear()
            self.feedback_messages.clear()
            if not keep_context:
                self.context = None


class ProcessingJobManager:
    """Starts and owns Processing jobs for the lifetime of the QGIS bridge."""

    def __init__(
        self,
        iface: Any = None,
        store: AsyncJobStore | None = None,
        dependencies: dict[str, Any] | None = None,
    ):
        self.iface = iface
        self.store = store or AsyncJobStore(scope_field="category")
        self._dependencies = dict(dependencies or {})
        self._lock = threading.RLock()
        self._accepting = True
        self._generation = 0

    @property
    def accepting(self) -> bool:
        with self._lock:
            return self._accepting and self.store.accepting

    def start(self, arguments: dict[str, Any]) -> dict[str, Any]:
        with self._lock:
            if not self._accepting:
                raise RuntimeError("QCopilots Processing job manager is stopping")
            generation = self._generation

        category = _required_category(arguments.get("category"))
        algorithm_id = _required_nonempty_string(
            arguments.get("algorithm_id"), "algorithm_id"
        )
        parameters = arguments.get("parameters", {})
        if parameters is None:
            parameters = {}
        if not isinstance(parameters, dict):
            raise ValueError("parameters must be an object")
        add_outputs = arguments.get("add_outputs_to_project", True)
        if not isinstance(add_outputs, bool):
            raise ValueError("add_outputs_to_project must be a boolean")
        client_request_id = _optional_client_request_id(
            arguments.get("client_request_id")
        )

        # Idempotent replays must not depend on the provider still being loaded
        # or Processing being re-initializable. MCP arguments are JSON values,
        # so this canonical input fingerprint is stable across service restarts.
        request_fingerprint = _request_fingerprint(
            {
                "category": category,
                "algorithm_id": algorithm_id,
                "parameters": parameters,
                "add_outputs_to_project": add_outputs,
            }
        )
        existing = self.store.lookup_idempotent(
            category, client_request_id, request_fingerprint
        )
        if existing is not None:
            return existing

        self._ensure_initialized()
        registry = self._registry()
        algorithm = registry.createAlgorithmById(algorithm_id)
        if algorithm is None:
            raise RuntimeError("Processing algorithm not found")
        if not self._category_matcher()(algorithm, category):
            raise RuntimeError(
                f"Processing algorithm is not available for {category} data"
            )
        sanitized_parameters = self._parameter_sanitizer()(
            parameters, algorithm.parameterDefinitions()
        )
        feedback = self._feedback_factory()()
        context = self._context_factory()(feedback)
        execution_mode = (
            "main_thread" if self._is_no_threading()(algorithm) else "qgs_task"
        )
        cancellable = (
            False
            if execution_mode == "main_thread"
            else bool(self._is_algorithm_cancellable()(algorithm))
        )
        job_id = str(uuid.uuid4())
        runtime = _ProcessingRuntime(
            algorithm=algorithm,
            parameters=dict(sanitized_parameters),
            context=context,
            feedback=feedback,
            add_outputs_to_project=add_outputs,
            generation=generation,
        )
        self._connect_feedback(job_id, category, runtime)
        now = utc_now_rfc3339()
        initial = {
            "job_id": job_id,
            "category": category,
            "client_request_id": client_request_id,
            "state": "queued",
            "execution_mode": execution_mode,
            "cancellable": cancellable,
            "algorithm": _algorithm_summary(algorithm),
            "progress_percent": 0.0,
            "progress_text": "",
            "processed_count": 0,
            "cancel_requested": False,
            "created_at": now,
            "started_at": None,
            "finished_at": None,
            "error": None,
        }
        snapshot, created = self.store.create(
            category,
            initial,
            client_request_id=client_request_id,
            request_fingerprint=request_fingerprint,
            runtime=runtime,
        )
        if not created:
            return snapshot

        try:
            prepared_parameters = self._prepare_outputs()(
                algorithm, dict(sanitized_parameters), add_outputs
            )
        except Exception as err:
            return self._finish_failed(
                job_id,
                category,
                "output_preparation_failed",
                "prepare",
                str(err),
                runtime,
            )
        runtime.parameters = prepared_parameters
        if execution_mode == "main_thread":
            with self._lock:
                current = self.store.get(job_id, category)
                if (
                    not self._accepting
                    or runtime.generation != self._generation
                    or current["state"] != "queued"
                    or current.get("cancel_requested")
                ):
                    if current["state"] not in TERMINAL_JOB_STATES:
                        current = self.store.transition(
                            job_id,
                            "cancelled",
                            category,
                            cancel_requested=True,
                            finished_at=utc_now_rfc3339(),
                            progress_text="Cancelled before Processing was scheduled",
                        )
                    runtime.release_completed_objects(keep_context=False)
                    return current
                try:
                    self._single_shot()(
                        0, lambda: self._run_main_thread(job_id, category)
                    )
                except Exception as err:
                    return self._finish_failed(
                        job_id,
                        category,
                        "schedule_failed",
                        "schedule",
                        str(err),
                        runtime,
                    )
            return self.store.get(job_id, category)

        task = None
        try:
            task = self._task_factory()(
                algorithm, prepared_parameters, context, feedback
            )
            task.begun.connect(
                lambda job_id=job_id, category=category: self._on_task_begun(
                    job_id, category
                )
            )
            task.executed.connect(
                lambda successful, results, job_id=job_id, category=category: (
                    self._on_task_executed(
                        job_id, category, bool(successful), results
                    )
                )
            )
            task.taskTerminated.connect(
                lambda job_id=job_id, category=category: self._on_task_terminated(
                    job_id, category
                )
            )
            _connect_signal(
                task,
                "destroyed",
                lambda *_args, job_id=job_id, category=category, runtime=runtime: (
                    self._release_finished_runtime(job_id, category, runtime)
                ),
            )
            with self._lock:
                current = self.store.get(job_id, category)
                runtime.task = task
                if (
                    not self._accepting
                    or runtime.generation != self._generation
                    or current["state"] != "queued"
                    or current.get("cancel_requested")
                ):
                    if current["state"] not in TERMINAL_JOB_STATES:
                        current = self.store.transition(
                            job_id,
                            "cancelled",
                            category,
                            cancel_requested=True,
                            finished_at=utc_now_rfc3339(),
                            progress_text="Cancelled before Processing was scheduled",
                        )
                    task = _detach_unsubmitted_task(runtime, task)
                    runtime.release_completed_objects(keep_context=False)
                    return current
                task_cancellable = bool(self._task_cancellable()(task, algorithm))
                self.store.patch(job_id, category, cancellable=task_cancellable)
                if _call_boolean(task, "isCanceled"):
                    failed = self._finish_failed(
                        job_id,
                        category,
                        "algorithm_prepare_failed",
                        "prepare",
                        "Processing algorithm could not be prepared",
                        runtime,
                    )
                    task = _detach_unsubmitted_task(runtime, task)
                    runtime.release_completed_objects(keep_context=False)
                    return failed
                task_id = self._task_manager().addTask(task)
                if not task_id:
                    failed = self._finish_failed(
                        job_id,
                        category,
                        "task_start_failed",
                        "schedule",
                        "QGIS task manager rejected the Processing task",
                        runtime,
                    )
                    task = _detach_unsubmitted_task(runtime, task)
                    runtime.release_completed_objects(keep_context=False)
                    return failed
        except Exception as err:
            failed = self._finish_failed(
                job_id,
                category,
                "task_start_failed",
                "schedule",
                str(err),
                runtime,
            )
            if task is not None:
                task = _detach_unsubmitted_task(runtime, task)
                runtime.release_completed_objects(keep_context=False)
            return failed
        return self.store.get(job_id, category)

    def get(self, arguments: dict[str, Any] | str) -> dict[str, Any]:
        if isinstance(arguments, dict):
            job_id = _required_nonempty_string(arguments.get("job_id"), "job_id")
            category = arguments.get("category")
            if category is not None:
                category = _required_category(category)
        else:
            job_id = _required_nonempty_string(arguments, "job_id")
            category = None
        return self.store.get(job_id, category)

    def list(self, arguments: dict[str, Any]) -> dict[str, Any]:
        category = _required_category(arguments.get("category"))
        states = arguments.get("states")
        if states is not None:
            if not isinstance(states, list):
                raise ValueError("states must be an array")
            invalid = {str(state) for state in states} - PUBLIC_JOB_STATES
            if invalid:
                raise ValueError(
                    "Invalid public job states: " + ", ".join(sorted(invalid))
                )
        limit = arguments.get("limit", 100)
        if isinstance(limit, bool) or not isinstance(limit, int) or not 1 <= limit <= 200:
            raise ValueError("limit must be an integer from 1 through 200")
        jobs = self.store.list(category, states=states, limit=limit)
        return {"jobs": [_job_summary(snapshot) for snapshot in jobs]}

    def cancel(self, arguments: dict[str, Any] | str) -> dict[str, Any]:
        if isinstance(arguments, dict):
            job_id = _required_nonempty_string(arguments.get("job_id"), "job_id")
            category = arguments.get("category")
            if category is not None:
                category = _required_category(category)
        else:
            job_id = _required_nonempty_string(arguments, "job_id")
            category = None
        with self._lock:
            snapshot, runtime, requested = self.store.request_cancel(job_id, category)
            if not requested or runtime is None:
                return snapshot
            task = runtime.task
            if task is not None:
                try:
                    task.cancel()
                except Exception as err:
                    return self._finish_failed(
                        job_id,
                        snapshot["category"],
                        "cancel_failed",
                        "cancel",
                        str(err),
                        runtime,
                    )
            return self.store.get(job_id, snapshot["category"])

    def shutdown(self, timeout_seconds: float = 5) -> None:
        timeout = float(timeout_seconds)
        if not math.isfinite(timeout) or timeout < 0:
            raise ValueError("timeout_seconds must be finite and non-negative")
        with self._lock:
            if not self._accepting:
                return
            self._accepting = False
            self._generation += 1
        self.store.shutdown()

        active = self.store.active_runtime_items()
        tasks = []
        for job_id, category, runtime in active:
            if runtime is None:
                continue
            task = runtime.task
            if task is None:
                # No task or main-thread callback has been registered yet.
                # The registration lock and generation guard prevent either
                # form of execution from being scheduled after shutdown.
                self.store.transition(
                    job_id,
                    "cancelled",
                    category,
                    cancel_requested=True,
                    finished_at=utc_now_rfc3339(),
                    progress_text="Cancelled while QGIS bridge was stopping",
                )
                runtime.release_completed_objects(keep_context=False)
                continue
            try:
                self.store.request_cancel(job_id, category)
            except RuntimeError:
                # Shutdown cancellation is internal and also applies to tasks
                # which do not advertise user cancellation.
                self.store.patch(job_id, category, cancel_requested=True)
            try:
                task.cancel()
            except Exception:
                pass
            tasks.append((job_id, category, task))

        deadline = time.monotonic() + timeout
        for job_id, category, task in tasks:
            remaining_ms = max(0, int((deadline - time.monotonic()) * 1000))
            wait = getattr(task, "waitForFinished", None)
            if callable(wait) and remaining_ms > 0:
                try:
                    wait(remaining_ms)
                except TypeError:
                    # A no-argument compatibility overload may block forever.
                    # Skip it so bridge shutdown always remains bounded.
                    pass
                except Exception:
                    pass

    def _run_main_thread(self, job_id: str, category: str) -> None:
        try:
            runtime = self.store.get_runtime(job_id, category)
            snapshot = self.store.get(job_id, category)
        except RuntimeError:
            return
        with self._lock:
            current_generation = self._generation
            accepting = self._accepting
        if (
            not accepting
            or runtime.generation != current_generation
            or snapshot["state"] != "queued"
        ):
            if snapshot["state"] not in TERMINAL_JOB_STATES:
                self.store.transition(
                    job_id,
                    "cancelled",
                    category,
                    cancel_requested=True,
                    finished_at=utc_now_rfc3339(),
                )
            return
        self.store.transition(
            job_id,
            "running",
            category,
            started_at=utc_now_rfc3339(),
            progress_text="Processing algorithm started",
        )
        try:
            results = self._main_thread_runner()(
                runtime.algorithm,
                runtime.parameters,
                runtime.context,
                runtime.feedback,
            )
            self._finish_succeeded(job_id, category, results, runtime)
        except Exception as err:
            self._finish_failed(
                job_id,
                category,
                "algorithm_execution_failed",
                "execute",
                str(err),
                runtime,
            )

    def _on_task_begun(self, job_id: str, category: str) -> None:
        try:
            snapshot = self.store.get(job_id, category)
        except RuntimeError:
            return
        if snapshot["state"] != "queued":
            return
        self.store.transition(
            job_id,
            "running",
            category,
            started_at=utc_now_rfc3339(),
            progress_text="Processing algorithm started",
        )

    def _on_task_executed(
        self, job_id: str, category: str, successful: bool, results: Any
    ) -> None:
        try:
            runtime = self.store.get_runtime(job_id, category)
            snapshot = self.store.get(job_id, category)
        except RuntimeError:
            return
        if snapshot["state"] in TERMINAL_JOB_STATES:
            return
        cancelled = (
            _call_boolean(runtime.feedback, "isCanceled")
            or _call_boolean(runtime.task, "isCanceled")
        )
        if successful:
            self._finish_succeeded(job_id, category, results, runtime)
        elif cancelled:
            self.store.transition(
                job_id,
                "cancelled",
                category,
                cancel_requested=True,
                finished_at=utc_now_rfc3339(),
                progress_text="Processing algorithm cancelled",
            )
        else:
            self._finish_failed(
                job_id,
                category,
                "algorithm_execution_failed",
                "execute",
                "Processing algorithm reported an unsuccessful result",
                runtime,
                details={"results": json_safe_value(results)},
            )

    def _on_task_terminated(self, job_id: str, category: str) -> None:
        try:
            runtime = self.store.get_runtime(job_id, category)
            snapshot = self.store.get(job_id, category)
        except RuntimeError:
            return
        if snapshot["state"] in TERMINAL_JOB_STATES:
            return
        if _call_boolean(runtime.task, "isCanceled") or _call_boolean(
            runtime.feedback, "isCanceled"
        ):
            self.store.transition(
                job_id,
                "cancelled",
                category,
                cancel_requested=True,
                finished_at=utc_now_rfc3339(),
                progress_text="Processing algorithm cancelled",
            )
            return
        self._finish_failed(
            job_id,
            category,
            "task_terminated",
            "execute",
            "Processing task terminated unexpectedly",
            runtime,
        )

    def _finish_succeeded(
        self,
        job_id: str,
        category: str,
        results: Any,
        runtime: _ProcessingRuntime,
    ) -> dict[str, Any]:
        try:
            current = self.store.get(job_id, category)
            if current["state"] in TERMINAL_JOB_STATES:
                return current
            if runtime.add_outputs_to_project:
                loaded = self._post_processor()(
                    runtime.algorithm,
                    runtime.context,
                    runtime.feedback,
                    results,
                )
                if loaded is False:
                    raise RuntimeError("One or more result layers could not be loaded")
            completed = self.store.transition(
                job_id,
                "succeeded",
                category,
                progress_percent=100.0,
                progress_text="Processing algorithm completed",
                finished_at=utc_now_rfc3339(),
                error=None,
                result=json_safe_value(results),
            )
            if runtime.task is None:
                runtime.release_completed_objects(
                    keep_context=not runtime.add_outputs_to_project
                )
            return completed
        except Exception as err:
            return self._finish_failed(
                job_id,
                category,
                "output_postprocessing_failed",
                "postprocess",
                str(err),
                runtime,
            )

    def _finish_failed(
        self,
        job_id: str,
        category: str,
        code: str,
        stage: str,
        message: str,
        runtime: _ProcessingRuntime,
        details: Any = None,
    ) -> dict[str, Any]:
        with runtime.lock:
            feedback_messages = list(runtime.feedback_messages)
        failed = self.store.transition(
            job_id,
            "failed",
            category,
            finished_at=utc_now_rfc3339(),
            progress_text=message,
            error={
                "code": code,
                "stage": stage,
                "message": message,
                "details": json_safe_value(details) if details is not None else None,
                "feedback_messages": feedback_messages,
            },
        )
        if runtime.task is None:
            runtime.release_completed_objects(keep_context=False)
        return failed

    def _connect_feedback(
        self, job_id: str, category: str, runtime: _ProcessingRuntime
    ) -> None:
        feedback = runtime.feedback
        _connect_signal(
            feedback,
            "progressChanged",
            lambda progress: self._update_progress(job_id, category, progress),
        )
        _connect_signal(
            feedback,
            "processedCountChanged",
            lambda count: self._safe_patch(
                job_id, category, processed_count=max(0, int(count))
            ),
        )
        _connect_signal(
            feedback,
            "progressTextChanged",
            lambda message: self._safe_patch(
                job_id, category, progress_text=str(message)
            ),
        )
        _connect_signal(
            feedback,
            "sinkFeatureCountChanged",
            lambda output, count: self._update_sink_count(
                job_id, category, runtime, output, count
            ),
        )
        for signal_name, label in (
            ("errorReported", "error"),
            ("warningPushed", "warning"),
            ("infoPushed", "info"),
            ("commandInfoPushed", "command"),
            ("debugInfoPushed", "debug"),
            ("consoleInfoPushed", "console"),
        ):
            _connect_signal(
                feedback,
                signal_name,
                lambda message, *_, label=label: self._append_feedback(
                    runtime, label, message
                ),
            )

    def _update_progress(self, job_id: str, category: str, progress: Any) -> None:
        try:
            numeric = float(progress)
        except (TypeError, ValueError):
            return
        if not math.isfinite(numeric):
            return
        self._safe_patch(job_id, category, progress_percent=min(100.0, max(0.0, numeric)))

    def _update_sink_count(
        self,
        job_id: str,
        category: str,
        runtime: _ProcessingRuntime,
        output: Any,
        count: Any,
    ) -> None:
        try:
            numeric = max(0, int(count))
        except (TypeError, ValueError):
            return
        with runtime.lock:
            runtime.sink_counts[str(output)] = numeric
            total = sum(runtime.sink_counts.values())
        self._safe_patch(job_id, category, processed_count=total)

    @staticmethod
    def _append_feedback(
        runtime: _ProcessingRuntime, label: str, message: Any
    ) -> None:
        text = str(message).strip()
        if not text:
            return
        with runtime.lock:
            if len(runtime.feedback_messages) >= 200:
                return
            runtime.feedback_messages.append(f"{label}: {text[:4000]}")

    def _safe_patch(self, job_id: str, category: str, **changes: Any) -> None:
        try:
            self.store.patch(job_id, category, **changes)
        except RuntimeError:
            pass

    def _release_finished_runtime(
        self,
        job_id: str,
        category: str,
        runtime: _ProcessingRuntime,
    ) -> None:
        try:
            snapshot = self.store.get(job_id, category)
        except RuntimeError:
            return
        if snapshot["state"] not in TERMINAL_JOB_STATES:
            return
        keep_context = (
            snapshot["state"] == "succeeded"
            and not runtime.add_outputs_to_project
        )
        runtime.release_completed_objects(keep_context=keep_context)

    def _ensure_initialized(self) -> None:
        dependency = self._dependencies.get("ensure_initialized")
        if dependency is not None:
            dependency()
            return
        try:
            from processing.core.Processing import Processing

            Processing.initialize()
        except Exception as err:
            raise RuntimeError(f"QGIS Processing could not be initialized: {err}") from err

    def _registry(self):
        dependency = self._dependencies.get("registry")
        if dependency is not None:
            return dependency() if callable(dependency) else dependency
        from qgis.core import QgsApplication

        return QgsApplication.processingRegistry()

    def _task_manager(self):
        dependency = self._dependencies.get("task_manager")
        if dependency is not None:
            return dependency() if callable(dependency) else dependency
        from qgis.core import QgsApplication

        return QgsApplication.taskManager()

    def _feedback_factory(self):
        dependency = self._dependencies.get("feedback_factory")
        if dependency is not None:
            return dependency

        def factory():
            from qgis.core import QgsProcessingFeedback

            return QgsProcessingFeedback(False)

        return factory

    def _context_factory(self):
        dependency = self._dependencies.get("context_factory")
        if dependency is not None:
            return dependency
        from processing.tools.dataobjects import createContext

        return createContext

    def _task_factory(self):
        dependency = self._dependencies.get("task_factory")
        if dependency is not None:
            return dependency

        def factory(algorithm, parameters, context, feedback):
            from qgis.core import QgsProcessingAlgRunnerTask

            return QgsProcessingAlgRunnerTask(
                algorithm, parameters, context=context, feedback=feedback
            )

        return factory

    def _single_shot(self):
        dependency = self._dependencies.get("single_shot")
        if dependency is not None:
            return dependency
        from qgis.PyQt.QtCore import QTimer

        return QTimer.singleShot

    def _main_thread_runner(self):
        dependency = self._dependencies.get("main_thread_runner")
        if dependency is not None:
            return dependency

        def run(algorithm, parameters, context, feedback):
            from processing.core.Processing import Processing

            return Processing.runAlgorithm(
                algorithm,
                parameters,
                onFinish=lambda *_: None,
                feedback=feedback,
                context=context,
            )

        return run

    def _post_processor(self):
        dependency = self._dependencies.get("post_processor")
        if dependency is not None:
            return dependency
        from processing.gui.Postprocessing import handleAlgorithmResults

        return handleAlgorithmResults

    def _parameter_sanitizer(self):
        return self._dependencies.get("parameter_sanitizer", lambda value, _: dict(value))

    def _category_matcher(self):
        return self._dependencies.get("category_matcher", _default_category_matcher)

    def _prepare_outputs(self):
        return self._dependencies.get("prepare_outputs", _prepare_output_destinations)

    def _is_no_threading(self):
        return self._dependencies.get("is_no_threading", _algorithm_is_no_threading)

    def _task_cancellable(self):
        return self._dependencies.get("is_task_cancellable", _task_is_cancellable)

    def _is_algorithm_cancellable(self):
        return self._dependencies.get(
            "is_algorithm_cancellable", _algorithm_cancellable
        )


def _prepare_output_destinations(
    algorithm: Any, parameters: dict[str, Any], add_outputs: bool
) -> dict[str, Any]:
    if not add_outputs:
        return parameters
    try:
        from qgis.core import (
            QgsProcessingOutputLayerDefinition,
            QgsProcessingParameterFeatureSink,
            QgsProcessingParameterRasterDestination,
            QgsProcessingParameterVectorDestination,
            QgsProject,
        )
    except Exception:
        return parameters

    for parameter in algorithm.parameterDefinitions():
        if parameter.name() not in parameters or not isinstance(
            parameter,
            (
                QgsProcessingParameterFeatureSink,
                QgsProcessingParameterVectorDestination,
                QgsProcessingParameterRasterDestination,
            ),
        ):
            continue
        value = parameters[parameter.name()]
        if not isinstance(value, QgsProcessingOutputLayerDefinition):
            parameters[parameter.name()] = QgsProcessingOutputLayerDefinition(
                value, QgsProject.instance()
            )
        else:
            value.destinationProject = QgsProject.instance()
    return parameters


def _algorithm_summary(algorithm: Any) -> dict[str, Any]:
    algorithm_id = str(algorithm.id())
    provider = algorithm_id.partition(":")[0]
    try:
        algorithm_provider = algorithm.provider()
        if algorithm_provider is not None:
            provider = str(algorithm_provider.id())
    except Exception:
        pass
    return {
        "id": algorithm_id,
        "name": str(algorithm.name()),
        "display_name": str(algorithm.displayName()),
        "group": str(algorithm.group()),
        "provider": provider,
    }


def _algorithm_is_no_threading(algorithm: Any) -> bool:
    try:
        from qgis.core import QgsProcessingAlgorithm

        flag = QgsProcessingAlgorithm.Flag.FlagNoThreading
        return bool(algorithm.flags() & flag)
    except Exception:
        return bool(getattr(algorithm, "no_threading", False))


def _algorithm_cancellable(algorithm: Any) -> bool:
    try:
        from qgis.core import Qgis

        return bool(algorithm.flags() & Qgis.ProcessingAlgorithmFlag.CanCancel)
    except Exception:
        return bool(getattr(algorithm, "cancellable", True))


def _task_is_cancellable(task: Any, algorithm: Any) -> bool:
    try:
        from qgis.core import QgsTask

        return bool(task.flags() & QgsTask.Flag.CanCancel)
    except Exception:
        return _algorithm_cancellable(algorithm)


def _default_category_matcher(algorithm: Any, category: str) -> bool:
    text = " ".join(
        str(value)
        for value in (
            algorithm.id(),
            algorithm.name(),
            algorithm.displayName(),
            algorithm.group(),
        )
    ).lower()
    if category == "vector":
        return any(
            token in text for token in ("vector", "feature", "geometry", "attribute")
        )
    return any(token in text for token in ("raster", "grid", "dem", "cell"))


def _job_summary(snapshot: dict[str, Any]) -> dict[str, Any]:
    summary_keys = (
        "job_id",
        "category",
        "client_request_id",
        "state",
        "execution_mode",
        "cancellable",
        "algorithm",
        "progress_percent",
        "progress_text",
        "processed_count",
        "cancel_requested",
        "created_at",
        "started_at",
        "finished_at",
    )
    summary = {key: snapshot.get(key) for key in summary_keys}
    error = snapshot.get("error")
    if isinstance(error, dict):
        summary["error"] = {
            "code": error.get("code"),
            "stage": error.get("stage"),
            "message": error.get("message"),
        }
    else:
        summary["error"] = None
    return summary


def _request_fingerprint(payload: dict[str, Any]) -> str:
    canonical = json.dumps(
        json_safe_value(payload),
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


def _required_category(value: Any) -> str:
    category = _required_nonempty_string(value, "category").lower()
    if category not in {"vector", "raster"}:
        raise ValueError("category must be vector or raster")
    return category


def _required_nonempty_string(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{name} must be a non-empty string")
    return value.strip()


def _optional_client_request_id(value: Any) -> str | None:
    if value is None:
        return None
    request_id = _required_nonempty_string(value, "client_request_id")
    if len(request_id) > 128:
        raise ValueError("client_request_id must contain at most 128 characters")
    return request_id


def _connect_signal(owner: Any, name: str, callback) -> None:
    try:
        signal = getattr(owner, name)
        signal.connect(callback)
    except (AttributeError, TypeError):
        pass


def _detach_unsubmitted_task(runtime: _ProcessingRuntime, task: Any) -> None:
    """Drop a task wrapper while its referenced context is still alive."""

    with runtime.lock:
        if runtime.task is task:
            runtime.task = None
    return None


def _call_boolean(owner: Any, name: str) -> bool:
    if owner is None:
        return False
    try:
        return bool(getattr(owner, name)())
    except Exception:
        return False
