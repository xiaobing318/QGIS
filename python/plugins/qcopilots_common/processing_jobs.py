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
import os
import re
import shutil
import tempfile
import threading
import time
import uuid
from dataclasses import dataclass, field
from datetime import date, datetime, time as datetime_time, timezone
from enum import Enum
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlparse

from qcopilots_common.async_jobs import (
    AsyncJobStore,
    PUBLIC_JOB_STATES,
    TERMINAL_JOB_STATES,
    utc_now_rfc3339,
)
from qcopilots_common.security_policy import FilesystemPolicy


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


def _processing_results_require_context(results: Any, context: Any) -> bool:
    """Return whether public results still reference context-owned layer state."""

    context_layer_ids: set[str] = set()
    try:
        layer_store = context.temporaryLayerStore()
        stored_layers = layer_store.mapLayers()
        candidates = (
            stored_layers.values()
            if isinstance(stored_layers, dict)
            else stored_layers
        )
        context_layer_ids.update(
            str(layer.id())
            for layer in candidates
            if layer is not None and str(layer.id())
        )
    except (AttributeError, RuntimeError, TypeError):
        pass

    seen: set[int] = set()

    def requires_context(value: Any) -> bool:
        if value is None or isinstance(value, (bool, int, float, bytes)):
            return False
        if isinstance(value, str):
            normalized = value.strip()
            folded = normalized.casefold()
            return bool(
                normalized in context_layer_ids
                or folded == "temporary_output"
                or folded.startswith(("memory:", "memory://", "vsimem:"))
                or folded.startswith("/vsimem/")
            )
        if isinstance(value, Path):
            return False

        value_id = id(value)
        if value_id in seen:
            return False
        seen.add(value_id)
        try:
            if isinstance(value, dict):
                return any(requires_context(item) for item in value.values())
            if isinstance(value, (list, tuple, set, frozenset)):
                return any(requires_context(item) for item in value)
            if isinstance(value, Enum):
                return requires_context(value.value)

            layer = _map_layer_snapshot(value)
            if layer is None:
                return False
            layer_id = str(layer.get("layer_id") or "")
            source = str(layer.get("source") or "").strip().casefold()
            provider = str(layer.get("provider") or "").strip().casefold()
            return bool(
                layer_id in context_layer_ids
                or provider == "memory"
                or source.startswith(("memory:", "memory://", "vsimem:", "/vsimem/"))
            )
        finally:
            seen.discard(value_id)

    return requires_context(results)


@dataclass
class _ProcessingRuntime:
    algorithm: Any
    parameters: dict[str, Any]
    context: Any
    feedback: Any
    add_outputs_to_project: bool
    generation: int
    retain_context_for_results: bool = False
    overwrite_outputs: bool = False
    authorized_input_versions: list[dict[str, Any]] = field(default_factory=list)
    output_stages: list[dict[str, Any]] = field(default_factory=list)
    output_promotion_receipt: dict[str, Any] | None = None
    output_promotion_state: str = "none"
    postprocess_initial_layer_ids: set[str] | None = None
    cleanup_residual_paths: list[str] = field(default_factory=list)
    cleanup_residual_layer_ids: list[str] = field(default_factory=list)
    cleanup_errors: list[str] = field(default_factory=list)
    task: Any = None
    feedback_messages: list[str] = field(default_factory=list)
    sink_counts: dict[str, int] = field(default_factory=dict)
    lock: threading.RLock = field(default_factory=threading.RLock)

    def release_completed_objects(self, *, keep_context: bool) -> None:
        """Release QGIS objects after their task no longer references context."""

        with self.lock:
            preserve_recovery = bool(
                self.cleanup_residual_paths
                or self.cleanup_residual_layer_ids
                or self.cleanup_errors
                or self.output_promotion_receipt is not None
                or self.output_stages
                or self.postprocess_initial_layer_ids is not None
            )
            # Drop the task wrapper first. QgsProcessingAlgRunnerTask stores a
            # reference to its context, so context must outlive the task object.
            self.task = None
            self.algorithm = None
            self.feedback = None
            self.parameters = {}
            if not preserve_recovery:
                self.output_stages.clear()
                self.output_promotion_receipt = None
                self.output_promotion_state = "none"
                self.cleanup_residual_paths.clear()
                self.cleanup_residual_layer_ids.clear()
                self.cleanup_errors.clear()
                self.postprocess_initial_layer_ids = None
            self.authorized_input_versions.clear()
            self.sink_counts.clear()
            self.feedback_messages.clear()
            if not keep_context:
                self.context = None
                self.retain_context_for_results = False


class ProcessingOverwriteConfirmationStore:
    def __init__(
        self,
        ttl_seconds: float = 300,
        clock: Any = time.monotonic,
        max_entries: int = 256,
    ):
        self.ttl_seconds = float(ttl_seconds)
        if isinstance(max_entries, bool) or int(max_entries) < 1:
            raise ValueError("max_entries must be a positive integer")
        self.max_entries = min(int(max_entries), 256)
        self._clock = clock
        self._entries: dict[str, dict[str, Any]] = {}
        self._lock = threading.RLock()
        self._next_sequence = 0

    def issue(self, fingerprint: str) -> str:
        with self._lock:
            self._purge_expired()
            while len(self._entries) >= self.max_entries:
                oldest = min(
                    self._entries,
                    key=lambda candidate: self._entries[candidate]["sequence"],
                )
                self._entries.pop(oldest, None)
            token = uuid.uuid4().hex + uuid.uuid4().hex
            while token in self._entries:
                token = uuid.uuid4().hex + uuid.uuid4().hex
            self._next_sequence += 1
            self._entries[token] = {
                "fingerprint": fingerprint,
                "expires_at": self._clock() + self.ttl_seconds,
                "sequence": self._next_sequence,
            }
            return token

    def consume(self, token: str, fingerprint: str) -> None:
        with self._lock:
            entry = self._entries.pop(str(token), None)
            if entry is None:
                raise RuntimeError(
                    "Processing overwrite confirmation token is invalid or already used"
                )
            if entry["expires_at"] <= self._clock():
                raise RuntimeError("Processing overwrite confirmation token has expired")
            if entry["fingerprint"] != fingerprint:
                raise RuntimeError(
                    "Processing parameters, inputs or overwrite targets changed after "
                    "preview. Request a new confirmation token."
                )

    def clear(self) -> None:
        with self._lock:
            self._entries.clear()

    def _purge_expired(self) -> None:
        now = self._clock()
        for token in [
            token
            for token, entry in self._entries.items()
            if entry["expires_at"] <= now
        ]:
            self._entries.pop(token, None)


class ProcessingJobManager:
    """Starts and owns Processing jobs for the lifetime of the QGIS bridge."""

    def __init__(
        self,
        iface: Any = None,
        store: AsyncJobStore | None = None,
        dependencies: dict[str, Any] | None = None,
    ):
        self.iface = iface
        self._dependencies = dict(dependencies or {})
        self.store = store or AsyncJobStore(
            scope_field="category",
            on_evict=self._cleanup_evicted_runtime,
        )
        self._lock = threading.RLock()
        self._accepting = True
        self._generation = 0
        self._overwrite_confirmations = ProcessingOverwriteConfirmationStore()

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
        overwrite_outputs = arguments.get("overwrite_outputs", False)
        if not isinstance(overwrite_outputs, bool):
            raise ValueError("overwrite_outputs must be a boolean")
        overwrite_confirmation_token = str(
            arguments.get("overwrite_confirmation_token") or ""
        ).strip()
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
                "overwrite_outputs": overwrite_outputs,
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
        if category == "general":
            from qcopilots_common.processing_metadata import (
                processing_algorithm_start_policy,
            )

            start_policy = processing_algorithm_start_policy(algorithm)
            if not start_policy["supported"]:
                raise RuntimeError(
                    "Processing algorithm start is unsupported: "
                    + str(start_policy["reason"])
                )
        policy_parameters = _apply_processing_filesystem_policy(
            parameters,
            algorithm.parameterDefinitions(),
            self._filesystem_policy(),
        )
        sanitized_parameters = self._parameter_sanitizer()(
            policy_parameters, algorithm.parameterDefinitions()
        )
        authorizer = self._output_authorizer()
        authorization_context: dict[str, Any] = {}
        if authorizer is _authorize_processing_output_destinations:
            authorization = authorizer(
                algorithm,
                sanitized_parameters,
                algorithm.parameterDefinitions(),
                overwrite_outputs,
                overwrite_confirmation_token,
                self._overwrite_confirmations,
                authorization_context=authorization_context,
            )
        else:
            authorization = authorizer(
                algorithm,
                sanitized_parameters,
                algorithm.parameterDefinitions(),
                overwrite_outputs,
                overwrite_confirmation_token,
                self._overwrite_confirmations,
            )
        if authorization is not None:
            return authorization
        authorized_input_versions = authorization_context.get("input_versions")
        if authorized_input_versions is None:
            authorized_input_versions = _processing_input_versions(
                sanitized_parameters,
                algorithm.parameterDefinitions(),
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
        execution_parameters, output_stages = _stage_processing_output_destinations(
            sanitized_parameters,
            algorithm.parameterDefinitions(),
        )
        runtime = _ProcessingRuntime(
            algorithm=algorithm,
            parameters=dict(execution_parameters),
            context=context,
            feedback=feedback,
            add_outputs_to_project=add_outputs,
            generation=generation,
            overwrite_outputs=overwrite_outputs,
            authorized_input_versions=authorized_input_versions,
            output_stages=output_stages,
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
            "overwrite_outputs": overwrite_outputs,
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
        try:
            snapshot, created = self.store.create(
                category,
                initial,
                client_request_id=client_request_id,
                request_fingerprint=request_fingerprint,
                runtime=runtime,
            )
        except Exception:
            _cleanup_processing_runtime_outputs(runtime)
            raise
        if not created:
            _cleanup_processing_runtime_outputs(runtime)
            return snapshot

        try:
            prepared_parameters = self._prepare_outputs()(
                algorithm, dict(execution_parameters), add_outputs
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
                        _cleanup_processing_runtime_outputs(runtime)
                        current = self.store.transition(
                            job_id,
                            "cancelled",
                            category,
                            cancel_requested=True,
                            finished_at=utc_now_rfc3339(),
                            progress_text="Cancelled before Processing was scheduled",
                            cleanup=_processing_cleanup_receipt(runtime),
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
                        _cleanup_processing_runtime_outputs(runtime)
                        current = self.store.transition(
                            job_id,
                            "cancelled",
                            category,
                            cancel_requested=True,
                            finished_at=utc_now_rfc3339(),
                            progress_text="Cancelled before Processing was scheduled",
                            cleanup=_processing_cleanup_receipt(runtime),
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
        snapshot = self.store.get(job_id, category)
        cleanup = snapshot.get("cleanup") or {}
        if snapshot["state"] in TERMINAL_JOB_STATES and cleanup.get("complete") is False:
            runtime = self.store.get_runtime(job_id, snapshot["category"])
            if runtime is not None and runtime.task is None:
                self._release_finished_runtime(
                    job_id,
                    snapshot["category"],
                    runtime,
                )
                snapshot = self.store.get(job_id, snapshot["category"])
        return snapshot

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
        self._overwrite_confirmations.clear()

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
                _cleanup_processing_runtime_outputs(runtime)
                self.store.transition(
                    job_id,
                    "cancelled",
                    category,
                    cancel_requested=True,
                    finished_at=utc_now_rfc3339(),
                    progress_text="Cancelled while QGIS bridge was stopping",
                    cleanup=_processing_cleanup_receipt(runtime),
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
            tasks.append((job_id, category, task, runtime))

        deadline = time.monotonic() + timeout
        for job_id, category, task, runtime in tasks:
            remaining_ms = max(0, int((deadline - time.monotonic()) * 1000))
            wait = getattr(task, "waitForFinished", None)
            finished = False
            if callable(wait) and remaining_ms > 0:
                try:
                    finished = bool(wait(remaining_ms))
                except TypeError:
                    # A no-argument compatibility overload may block forever.
                    # Skip it so bridge shutdown always remains bounded.
                    pass
                except Exception:
                    pass
            try:
                snapshot = self.store.get(job_id, category)
            except RuntimeError:
                continue
            if snapshot["state"] not in TERMINAL_JOB_STATES:
                if finished:
                    _cleanup_processing_runtime_outputs(runtime)
                    self.store.transition(
                        job_id,
                        "cancelled",
                        category,
                        cancel_requested=True,
                        finished_at=utc_now_rfc3339(),
                        progress_text="Cancelled while QGIS bridge was stopping",
                        cleanup=_processing_cleanup_receipt(runtime),
                    )
                    runtime.release_completed_objects(keep_context=False)

        for job_id, category, runtime in self.store.terminal_runtime_items():
            if runtime is None:
                continue
            self._release_finished_runtime(job_id, category, runtime)

    @staticmethod
    def _cleanup_evicted_runtime(
        _job_id: str,
        runtime: _ProcessingRuntime | None,
    ) -> dict[str, Any]:
        if runtime is None:
            return {
                "complete": True,
                "residual_paths": [],
                "residual_layer_ids": [],
                "errors": [],
                "retry_recommended": False,
            }
        _cleanup_processing_runtime_outputs(runtime)
        cleanup = _processing_cleanup_receipt(runtime)
        runtime.release_completed_objects(keep_context=False)
        return cleanup

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
            or snapshot.get("cancel_requested")
        ):
            if snapshot["state"] not in TERMINAL_JOB_STATES:
                _cleanup_processing_runtime_outputs(runtime)
                self.store.transition(
                    job_id,
                    "cancelled",
                    category,
                    cancel_requested=True,
                    finished_at=utc_now_rfc3339(),
                    cleanup=_processing_cleanup_receipt(runtime),
                )
                runtime.release_completed_objects(keep_context=False)
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
            self._release_finished_runtime(job_id, category, runtime)
            return
        with self._lock:
            accepting = self._accepting
        if not accepting and snapshot.get("cancel_requested"):
            _cleanup_processing_runtime_outputs(runtime)
            self.store.transition(
                job_id,
                "cancelled",
                category,
                cancel_requested=True,
                finished_at=utc_now_rfc3339(),
                progress_text="Processing completed after bridge shutdown and was discarded",
                cleanup=_processing_cleanup_receipt(runtime),
            )
            runtime.release_completed_objects(keep_context=False)
            return
        cancelled = (
            _call_boolean(runtime.feedback, "isCanceled")
            or _call_boolean(runtime.task, "isCanceled")
        )
        if successful:
            self._finish_succeeded(job_id, category, results, runtime)
        elif cancelled:
            _cleanup_processing_runtime_outputs(runtime)
            self.store.transition(
                job_id,
                "cancelled",
                category,
                cancel_requested=True,
                finished_at=utc_now_rfc3339(),
                progress_text="Processing algorithm cancelled",
                cleanup=_processing_cleanup_receipt(runtime),
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
            self._release_finished_runtime(job_id, category, runtime)
            return
        if _call_boolean(runtime.task, "isCanceled") or _call_boolean(
            runtime.feedback, "isCanceled"
        ):
            _cleanup_processing_runtime_outputs(runtime)
            self.store.transition(
                job_id,
                "cancelled",
                category,
                cancel_requested=True,
                finished_at=utc_now_rfc3339(),
                progress_text="Processing algorithm cancelled",
                cleanup=_processing_cleanup_receipt(runtime),
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
            published_results = results
            if runtime.output_stages:
                _validate_processing_versions(
                    runtime.authorized_input_versions,
                    "Processing input changed while the job was running",
                )
                _validate_processing_stage_targets(runtime.output_stages)
                runtime.output_promotion_receipt = _promote_processing_outputs(
                    runtime.output_stages,
                    overwrite=runtime.overwrite_outputs,
                )
                runtime.output_promotion_state = "promoted"
                published_results = _rewrite_processing_output_results(
                    results,
                    runtime.output_stages,
                )
                _rewrite_processing_context_layer_destinations(
                    runtime.context,
                    runtime.output_stages,
                )
            if runtime.add_outputs_to_project:
                runtime.postprocess_initial_layer_ids = _processing_project_layer_ids()
                loaded = self._post_processor()(
                    runtime.algorithm,
                    runtime.context,
                    runtime.feedback,
                    published_results,
                )
                if loaded is False:
                    raise RuntimeError("One or more result layers could not be loaded")
                runtime.postprocess_initial_layer_ids = None
            runtime.retain_context_for_results = bool(
                not runtime.add_outputs_to_project
                and _processing_results_require_context(
                    published_results,
                    runtime.context,
                )
            )
            cleanup_residuals = []
            if runtime.output_promotion_receipt is not None:
                runtime.output_promotion_state = "published_success"
                cleanup_residuals.extend(
                    _commit_processing_output_promotion(
                        runtime.output_promotion_receipt
                    )
                )
                if not cleanup_residuals:
                    runtime.output_promotion_receipt = None
                    runtime.output_promotion_state = "none"
            stage_residuals = _cleanup_processing_stages(runtime.output_stages)
            cleanup_residuals.extend(stage_residuals)
            if not stage_residuals:
                runtime.output_stages.clear()
            runtime.cleanup_residual_paths = sorted(set(cleanup_residuals))
            completed = self.store.transition(
                job_id,
                "succeeded",
                category,
                progress_percent=100.0,
                progress_text="Processing algorithm completed",
                finished_at=utc_now_rfc3339(),
                error=None,
                result=json_safe_value(published_results),
                cleanup=_processing_cleanup_receipt(runtime),
            )
            if runtime.task is None and _processing_cleanup_receipt(runtime)[
                "complete"
            ]:
                runtime.release_completed_objects(
                    keep_context=runtime.retain_context_for_results
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
        _cleanup_processing_runtime_outputs(runtime)
        cleanup_details = _processing_cleanup_receipt(runtime)
        if not cleanup_details["complete"]:
            details = {
                "original_error": {
                    "code": code,
                    "stage": stage,
                    "details": details,
                },
                "cleanup": cleanup_details,
            }
            code = "output_state_indeterminate"
            message = (
                f"{message}. Output cleanup was incomplete, inspect residual_paths"
            )
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
            cleanup=cleanup_details,
        )
        if runtime.task is None and cleanup_details["complete"]:
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
        _cleanup_processing_runtime_outputs(
            runtime,
            terminal_state=snapshot["state"],
        )
        self.store.patch_cleanup_receipt(
            job_id,
            _processing_cleanup_receipt(runtime),
            category,
        )
        keep_context = bool(
            snapshot["state"] == "succeeded"
            and runtime.retain_context_for_results
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

    def _output_authorizer(self):
        return self._dependencies.get(
            "output_authorizer", _authorize_processing_output_destinations
        )

    def _filesystem_policy(self) -> FilesystemPolicy:
        policy = self._dependencies.get("filesystem_policy")
        return policy if isinstance(policy, FilesystemPolicy) else FilesystemPolicy()

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


SHAPEFILE_FAMILY_SUFFIXES = (
    ".shp",
    ".shx",
    ".dbf",
    ".prj",
    ".cpg",
    ".qpj",
    ".sbn",
    ".sbx",
    ".qix",
    ".fix",
    ".idm",
    ".ind",
)
CONTAINER_OUTPUT_SUFFIXES = {".gpkg", ".sqlite", ".db"}
MAX_PROCESSING_VERSION_DIRECTORY_ENTRIES = 10000
MAX_PROCESSING_VERSION_DIRECTORY_BYTES = 512 * 1024 * 1024
MAX_PROCESSING_VERSION_FILE_BYTES = 512 * 1024 * 1024
MAX_PROCESSING_INPUT_VERSIONS = 512
MAX_PROCESSING_LAYER_FINGERPRINT_FEATURES = 50000
MAX_PROCESSING_LAYER_FINGERPRINT_BYTES = 128 * 1024 * 1024
MAX_PROCESSING_FINGERPRINT_SECONDS = 2.0


def _authorize_processing_output_destinations(
    algorithm: Any,
    parameters: dict[str, Any],
    parameter_definitions: Any,
    overwrite_outputs: bool,
    confirmation_token: str,
    confirmation_store: ProcessingOverwriteConfirmationStore,
    *,
    authorization_context: dict[str, Any] | None = None,
) -> dict[str, Any] | None:
    definitions = list(parameter_definitions or [])
    algorithm_id = str(algorithm.id())
    folder_destinations = [
        str(item.name())
        for item in definitions
        if _processing_definition_is_folder_destination(item)
    ]
    if folder_destinations:
        raise RuntimeError(
            f"Processing algorithm {algorithm_id} uses unsupported folder "
            "destinations which cannot be atomically published: "
            + ", ".join(folder_destinations)
        )
    if not any(_processing_definition_is_destination(item) for item in definitions):
        raise RuntimeError(
            f"Processing algorithm {algorithm_id} has no separate destination and "
            "is unsupported by the MCP start service because it may mutate inputs "
            "or create in-place sidecars. Run it only on an isolated copy outside "
            "this generic service."
        )

    output_plans = _processing_output_plans(parameters, definitions)
    input_versions = _processing_input_versions(parameters, definitions)
    if authorization_context is not None:
        authorization_context["input_versions"] = input_versions
    input_identities = {
        _path_identity(Path(item["path"]))
        for item in input_versions
        if item.get("kind", "file") == "file"
    }
    output_identities = {}
    for plan in output_plans:
        for path in plan["family_paths"]:
            identity = _path_identity(path)
            if identity in output_identities:
                raise RuntimeError(
                    "Processing output parameters resolve to the same file family: "
                    f"{output_identities[identity]} and {plan['parameter']}"
                )
            output_identities[identity] = plan["parameter"]
        overlap = [
            str(path)
            for path in plan["family_paths"]
            if _path_identity(path) in input_identities
        ]
        if overlap:
            raise RuntimeError(
                "Processing input and output paths must be different. Use an "
                "isolated output copy: " + ", ".join(overlap)
            )

    output_versions = [
        {
            "parameter": plan["parameter"],
            **_processing_file_version(path),
        }
        for plan in output_plans
        for path in plan["family_paths"]
    ]
    collisions = [item for item in output_versions if item["exists"]]
    if collisions:
        if not overwrite_outputs:
            details = ", ".join(
                f"{item['parameter']}={item['path']}" for item in collisions
            )
            raise FileExistsError(
                "Processing output destination already exists. Request an exact "
                "overwrite preview before approving these targets: " + details
            )
        unsafe_layer_inputs = [
            item
            for item in input_versions
            if item.get("kind") == "project_layer"
            and not item.get("immutable_file_snapshot", False)
        ]
        if unsafe_layer_inputs:
            raise RuntimeError(
                "Confirmed Processing overwrites require immutable local file "
                "snapshots for every QGIS project layer input. Unsupported layers: "
                + ", ".join(
                    f"{item.get('name')} ({item.get('layer_id')})"
                    for item in unsafe_layer_inputs
                )
            )
        parameters_sha256 = _request_fingerprint(json_safe_value(parameters))
        confirmation_snapshot = {
            "algorithm_id": algorithm_id,
            "parameters_sha256": parameters_sha256,
            "inputs": input_versions,
            "outputs": output_versions,
        }
        fingerprint = _request_fingerprint(confirmation_snapshot)
        if confirmation_token:
            confirmation_store.consume(confirmation_token, fingerprint)
            return None
        token = confirmation_store.issue(fingerprint)
        return {
            "confirmation_required": True,
            "algorithm_id": algorithm_id,
            "overwrite_outputs": True,
            "overwrite_targets": collisions,
            "input_versions": input_versions,
            "parameters_sha256": parameters_sha256,
            "overwrite_confirmation_token": token,
            "confirmation_expires_in_seconds": confirmation_store.ttl_seconds,
        }
    if confirmation_token:
        raise RuntimeError(
            "overwrite_confirmation_token was supplied, but no existing output "
            "targets require confirmation"
        )
    return None


def _guard_processing_output_destinations(
    parameters: dict[str, Any],
    parameter_definitions: Any,
    overwrite_outputs: bool,
) -> None:
    collisions = [
        {
            "parameter": plan["parameter"],
            "path": str(path),
        }
        for plan in _processing_output_plans(parameters, parameter_definitions)
        for path in plan["family_paths"]
        if path.exists()
    ]
    if collisions and not overwrite_outputs:
        details = ", ".join(
            f"{item['parameter']}={item['path']}" for item in collisions
        )
        raise FileExistsError(
            "Processing output destination already exists. Set overwrite_outputs "
            f"to true only after approving the exact targets: {details}"
        )


def _processing_output_plans(
    parameters: dict[str, Any],
    parameter_definitions: Any,
) -> list[dict[str, Any]]:
    plans = []
    for definition in parameter_definitions or []:
        name = str(definition.name())
        if _processing_definition_is_folder_destination(definition):
            raise RuntimeError(
                "Processing folder destinations are unsupported because directory "
                f"publication is not atomic: {name}"
            )
        if name not in parameters or not _processing_definition_is_destination(
            definition
        ):
            continue
        value = parameters[name]
        path = _processing_file_destination(value)
        if path is None:
            raw = str(value).strip()
            if raw and not raw.upper() == "TEMPORARY_OUTPUT" and not raw.lower().startswith(
                "memory:"
            ):
                raise RuntimeError(
                    "Processing destination provider/database URIs are unsupported by "
                    f"the generic MCP start service: {name}. Use a new isolated local "
                    "file destination."
                )
            continue
        raw = str(value)
        _, separator, options = raw.partition("|")
        if path.exists() and path.is_dir():
            raise RuntimeError(
                "Processing cannot safely overwrite an existing directory "
                f"destination: {path}. Use a new isolated output directory."
            )
        is_container = path.suffix.lower() in CONTAINER_OUTPUT_SUFFIXES
        if is_container and path.exists():
            raise RuntimeError(
                "Processing cannot safely replace or append to an existing container "
                f"destination: {path}. Use a new isolated container path."
            )
        plans.append(
            {
                "parameter": name,
                "target": path,
                "family_paths": _processing_output_family_paths(path),
                "container_layer_options": options if separator else "",
            }
        )
    return plans


def _apply_processing_filesystem_policy(
    parameters: dict[str, Any],
    parameter_definitions: Any,
    policy: FilesystemPolicy,
) -> dict[str, Any]:
    definitions = {
        str(definition.name()): definition
        for definition in parameter_definitions or []
    }
    normalized = dict(parameters)
    for name, value in parameters.items():
        definition = definitions.get(str(name))
        if definition is None:
            continue
        if _processing_definition_is_destination(definition):
            normalized[name] = _normalize_processing_policy_path_value(
                value,
                definition,
                policy,
                access="write",
            )
        elif _processing_definition_accepts_path(definition):
            normalized[name] = _normalize_processing_policy_path_value(
                value,
                definition,
                policy,
                access="read",
            )
    return normalized


def _normalize_processing_policy_path_value(
    value: Any,
    definition: Any,
    policy: FilesystemPolicy,
    *,
    access: str,
) -> Any:
    if isinstance(value, dict):
        return {
            key: _normalize_processing_policy_path_value(
                item, definition, policy, access=access
            )
            for key, item in value.items()
        }
    if isinstance(value, (list, tuple)):
        return [
            _normalize_processing_policy_path_value(
                item, definition, policy, access=access
            )
            for item in value
        ]

    if access == "read":
        layer = _processing_project_layer_reference(value, definition)
        if layer is not None:
            try:
                provider = str(layer.providerType()).lower()
            except Exception:
                provider = ""
            if provider == "memory":
                return value
            source = str(layer.source())
            source_path = _processing_policy_local_path(source)
            if source_path is None:
                if policy.restricted:
                    raise PermissionError(
                        "Formal restricted mode requires QGIS project layer inputs "
                        "to use approved local files"
                    )
                policy.validate_provider_local_paths(
                    source,
                    access="read",
                    provider=provider,
                )
                return value
            if source_path.lower().startswith("/vsi"):
                if policy.restricted:
                    raise PermissionError(
                        "Formal restricted mode does not permit GDAL virtual "
                        "filesystem paths"
                    )
                return value
            resolved_source = policy.resolve_path(source_path, access="read")
            if policy.restricted and not resolved_source.exists():
                raise FileNotFoundError(
                    "Formal restricted Processing input does not exist: "
                    f"{resolved_source}"
                )
            return value

    if not isinstance(value, (str, Path)):
        return value

    raw = str(value).strip()
    if not raw or raw.upper() == "TEMPORARY_OUTPUT" or raw.lower().startswith(
        "memory:"
    ):
        return value
    if raw.lower().startswith("/vsi"):
        if policy.restricted:
            raise PermissionError(
                "Formal restricted mode does not permit GDAL virtual filesystem paths"
            )
        return value

    path_part, separator, options = raw.partition("|")
    lowered = path_part.lower()
    has_windows_drive = bool(re.match(r"^[A-Za-z]:", path_part))
    known_provider_assignment = bool(
        re.match(
            r"^(?:type|url|dbname|service|host|path|file|filename)\s*=",
            path_part,
            re.IGNORECASE,
        )
    )
    uri_or_provider = (
        "://" in path_part
        or lowered.startswith("file:")
        or known_provider_assignment
        or (
            not has_windows_drive
            and bool(re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", path_part))
        )
    )
    if uri_or_provider:
        if policy.restricted:
            raise PermissionError(
                "Formal restricted Processing parameters require approved local "
                "paths and do not permit URI values"
            )
        local_uri_path = _processing_policy_local_path(path_part)
        if local_uri_path is None:
            policy.validate_provider_local_paths(
                path_part
                + (("&" + options.replace("|", "&")) if separator else ""),
                access=access,
            )
            return value
        path_part = local_uri_path
    looks_like_path = (
        has_windows_drive
        or Path(path_part).is_absolute()
        or "/" in path_part
        or "\\" in path_part
        or bool(Path(path_part).suffix)
        or path_part.lower().startswith("file:")
    )
    if not looks_like_path and access == "read":
        if _processing_definition_accepts_layer(definition):
            if policy.restricted:
                raise PermissionError(
                    "Formal restricted Processing layer input could not be resolved "
                    f"to a project layer or approved local path: {raw}"
                )
            return value
        looks_like_path = True
    resolved = policy.resolve_path(path_part, access=access)
    if policy.restricted and access == "read" and not resolved.exists():
        raise FileNotFoundError(
            f"Formal restricted Processing input does not exist: {resolved}"
        )
    normalized = str(resolved) + (separator + options if separator else "")
    return Path(normalized) if isinstance(value, Path) and not separator else normalized


def _processing_policy_local_path(value: str) -> str | None:
    """Return an unresolved local path spelling for shared policy validation."""

    raw = str(value).strip().split("|", 1)[0]
    has_windows_drive = bool(re.match(r"^[A-Za-z]:", raw))
    if not has_windows_drive and raw.lower().startswith("file:"):
        parsed = urlparse(raw)
        path = unquote(parsed.path)
        if parsed.netloc and parsed.netloc.lower() != "localhost":
            path = f"//{parsed.netloc}{path}"
        elif re.match(r"^/[A-Za-z]:/", path):
            path = path[1:]
        return path
    if (
        not has_windows_drive
        and (
            "://" in raw
            or bool(re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", raw))
            or bool(
                re.match(
                    r"^(?:type|url|dbname|service|host|path|file|filename)\s*=",
                    raw,
                    re.IGNORECASE,
                )
            )
        )
    ):
        return None
    return raw


def _processing_output_family_paths(path: Path) -> list[Path]:
    suffix = path.suffix.lower()
    if suffix == ".shp":
        paths = [path.with_suffix(item) for item in SHAPEFILE_FAMILY_SUFFIXES]
        paths.append(path.with_name(path.name + ".xml"))
        return paths
    if suffix in {".tif", ".tiff", ".vrt"}:
        paths = [
            path,
            path.with_name(path.name + ".aux.xml"),
            path.with_name(path.name + ".ovr"),
        ]
        if suffix in {".tif", ".tiff"}:
            world_suffix = ".tfw" if suffix == ".tif" else ".tifw"
            paths.append(path.with_suffix(world_suffix))
        return paths
    if suffix in CONTAINER_OUTPUT_SUFFIXES:
        return [
            path,
            path.with_name(path.name + "-wal"),
            path.with_name(path.name + "-shm"),
            path.with_name(path.name + "-journal"),
        ]
    return [path]


def _processing_input_versions(
    parameters: dict[str, Any],
    parameter_definitions: Any,
) -> list[dict[str, Any]]:
    deadline = time.monotonic() + MAX_PROCESSING_FINGERPRINT_SECONDS
    definitions = {str(item.name()): item for item in parameter_definitions or []}
    paths = {}
    layers = {}
    for name, value in parameters.items():
        definition = definitions.get(str(name))
        if definition is None or _processing_definition_is_destination(definition):
            continue
        for layer in _iter_processing_project_layers(value, definition):
            version = _processing_project_layer_version(layer)
            source_path = _processing_file_destination(version["source"])
            immutable_source = bool(
                source_path is not None
                and source_path.is_file()
                and not version["editable"]
                and not version["modified"]
            )
            version["immutable_file_snapshot"] = immutable_source
            if immutable_source:
                for family_path in _processing_output_family_paths(source_path):
                    paths[_path_identity(family_path)] = family_path
            else:
                version["content_fingerprint"] = (
                    _processing_project_layer_content_fingerprint(
                        layer,
                        deadline=deadline,
                    )
                )
            layers[version["layer_id"]] = version
            if len(paths) + len(layers) > MAX_PROCESSING_INPUT_VERSIONS:
                raise RuntimeError(
                    "Could not establish bounded Processing input versions: "
                    f"more than {MAX_PROCESSING_INPUT_VERSIONS} inputs"
                )
        if _processing_definition_accepts_path(definition):
            for path in _iter_processing_input_paths(value):
                for family_path in _processing_output_family_paths(path):
                    paths[_path_identity(family_path)] = family_path
                    if len(paths) + len(layers) > MAX_PROCESSING_INPUT_VERSIONS:
                        raise RuntimeError(
                            "Could not establish bounded Processing input versions: "
                            f"more than {MAX_PROCESSING_INPUT_VERSIONS} inputs"
                        )
    file_versions = [
        _processing_file_version(path, deadline=deadline)
        for path in sorted(paths.values(), key=lambda item: _path_identity(item))
    ]
    return file_versions + [layers[key] for key in sorted(layers)]


def _iter_processing_input_paths(value: Any):
    if isinstance(value, dict):
        for item in value.values():
            yield from _iter_processing_input_paths(item)
        return
    if isinstance(value, (list, tuple)):
        for item in value:
            yield from _iter_processing_input_paths(item)
        return
    if not isinstance(value, (str, Path)):
        return
    raw = str(value).strip().split("|", 1)[0]
    if not raw or raw.lower().startswith(("memory:", "/vsi")):
        return
    if not (
        Path(raw).is_absolute()
        or "/" in raw
        or "\\" in raw
        or bool(Path(raw).suffix)
        or raw.lower().startswith("file://")
    ):
        return
    path = _processing_file_destination(raw)
    if path is not None:
        yield path


def _iter_processing_project_layers(value: Any, definition: Any):
    if isinstance(value, dict):
        for item in value.values():
            yield from _iter_processing_project_layers(item, definition)
        return
    if isinstance(value, (list, tuple)):
        for item in value:
            yield from _iter_processing_project_layers(item, definition)
        return
    layer = _processing_project_layer_reference(value, definition)
    if layer is not None:
        yield layer


def _processing_project_layer_reference(value: Any, definition: Any) -> Any | None:
    if all(hasattr(value, name) for name in ("id", "name", "source")):
        return value
    if not isinstance(value, str) or not _processing_definition_accepts_layer(definition):
        return None
    try:
        from qgis.core import QgsProject

        project = QgsProject.instance()
        layer = project.mapLayer(value)
        if layer is not None:
            return layer
        matches = list(project.mapLayersByName(value))
    except Exception:
        return None
    if len(matches) > 1:
        raise RuntimeError(
            f"Processing layer name is ambiguous, use a layer id instead: {value}"
        )
    return matches[0] if matches else None


def _processing_definition_accepts_layer(definition: Any) -> bool:
    try:
        text = str(definition.type()).replace("_", "").lower()
    except Exception:
        text = type(definition).__name__.replace("_", "").lower()
    return any(
        token in text
        for token in (
            "source",
            "vector",
            "raster",
            "maplayer",
            "multilayer",
            "feature",
            "mesh",
            "pointcloud",
        )
    )


def _processing_definition_accepts_path(definition: Any) -> bool:
    try:
        text = str(definition.type()).replace("_", "").lower()
    except Exception:
        text = type(definition).__name__.replace("_", "").lower()
    return any(
        token in text
        for token in (
            "file",
            "folder",
            "directory",
            "source",
            "vector",
            "raster",
            "maplayer",
            "multilayer",
            "feature",
            "mesh",
            "pointcloud",
            "tininputlayers",
        )
    )


def _processing_project_layer_version(layer: Any) -> dict[str, Any]:
    selected_ids = []
    try:
        selected_ids = sorted(int(value) for value in layer.selectedFeatureIds())
    except Exception:
        selected_ids = []
    if len(selected_ids) > 10000:
        raise RuntimeError(
            "Processing overwrite confirmation does not support project layers "
            "with more than 10000 selected features"
        )
    provider = None
    try:
        provider = layer.dataProvider()
    except Exception:
        provider = None
    provider_revision = None
    if provider is not None:
        for method_name in ("dataTimestamp", "timestamp"):
            try:
                value = getattr(provider, method_name)()
            except Exception:
                continue
            try:
                provider_revision = int(value.toMSecsSinceEpoch())
            except Exception:
                provider_revision = str(value)
            break
    data_shape = {}
    for method_name, key in (
        ("featureCount", "feature_count"),
        ("width", "width"),
        ("height", "height"),
        ("bandCount", "band_count"),
    ):
        try:
            data_shape[key] = int(getattr(layer, method_name)())
        except Exception:
            continue
    try:
        fields = [str(field.name()) for field in layer.fields()]
    except Exception:
        fields = []
    try:
        subset = str(layer.subsetString())
    except Exception:
        subset = ""
    try:
        provider_name = str(layer.providerType())
    except Exception:
        try:
            provider_name = str(provider.name()) if provider is not None else ""
        except Exception:
            provider_name = ""
    return {
        "kind": "project_layer",
        "layer_id": str(layer.id()),
        "name": str(layer.name()),
        "source": str(layer.source()),
        "provider": provider_name,
        "provider_revision": provider_revision,
        "editable": bool(getattr(layer, "isEditable", lambda: False)()),
        "modified": bool(getattr(layer, "isModified", lambda: False)()),
        "selection_ids": selected_ids,
        "fields": fields,
        "subset": subset,
        "data_shape": data_shape,
    }


def _processing_project_layer_content_fingerprint(
    layer: Any,
    *,
    deadline: float | None = None,
) -> dict[str, Any]:
    """Return a bounded strong fingerprint for a non-file project layer."""

    get_features = getattr(layer, "getFeatures", None)
    if not callable(get_features):
        raise RuntimeError(
            "Processing project layer inputs without an immutable local file must "
            "provide a bounded vector feature snapshot"
        )
    try:
        declared_feature_count = int(layer.featureCount())
    except Exception:
        declared_feature_count = -1
    if declared_feature_count > MAX_PROCESSING_LAYER_FINGERPRINT_FEATURES:
        raise RuntimeError(
            "Could not establish a bounded strong Processing project layer "
            "fingerprint: declared feature count exceeds "
            f"{MAX_PROCESSING_LAYER_FINGERPRINT_FEATURES}"
        )
    deadline = (
        time.monotonic() + MAX_PROCESSING_FINGERPRINT_SECONDS
        if deadline is None
        else float(deadline)
    )

    records = []
    content_bytes = 0
    try:
        features = get_features()
    except Exception as err:
        raise RuntimeError(
            "Could not establish a strong Processing project layer fingerprint: "
            f"{err}"
        ) from err
    try:
        for feature in features:
            _check_processing_fingerprint_deadline(deadline)
            if len(records) >= MAX_PROCESSING_LAYER_FINGERPRINT_FEATURES:
                raise RuntimeError(
                    "Could not establish a bounded strong Processing project layer "
                    "fingerprint: more than "
                    f"{MAX_PROCESSING_LAYER_FINGERPRINT_FEATURES} features"
                )
            feature_id = int(feature.id())
            attributes = json.dumps(
                json_safe_value(feature.attributes()),
                ensure_ascii=False,
                sort_keys=True,
                allow_nan=False,
                separators=(",", ":"),
            ).encode("utf-8")
            geometry_bytes = b""
            geometry = feature.geometry()
            if not geometry.isNull():
                geometry_bytes = bytes(geometry.asWkb())
            content_bytes += len(attributes) + len(geometry_bytes)
            if content_bytes > MAX_PROCESSING_LAYER_FINGERPRINT_BYTES:
                raise RuntimeError(
                    "Could not establish a bounded strong Processing project layer "
                    "fingerprint: feature content exceeds "
                    f"{MAX_PROCESSING_LAYER_FINGERPRINT_BYTES} bytes"
                )
            feature_digest = hashlib.sha256()
            feature_digest.update(attributes)
            feature_digest.update(b"\0")
            feature_digest.update(geometry_bytes)
            records.append((feature_id, feature_digest.digest()))
    except RuntimeError:
        raise
    except Exception as err:
        raise RuntimeError(
            "Could not establish a strong Processing project layer fingerprint: "
            f"{err}"
        ) from err

    digest = hashlib.sha256()
    for feature_id, feature_digest in sorted(records):
        digest.update(str(feature_id).encode("ascii"))
        digest.update(b":")
        digest.update(feature_digest)
        digest.update(b"\0")
    return {
        "kind": "vector_features",
        "sha256": digest.hexdigest(),
        "feature_count": len(records),
        "content_bytes": content_bytes,
    }


def _processing_file_version(
    path: Path,
    *,
    deadline: float | None = None,
) -> dict[str, Any]:
    try:
        metadata = path.stat()
    except FileNotFoundError:
        return {"path": str(path), "exists": False}
    version = {
        "path": str(path),
        "exists": True,
        "is_directory": path.is_dir(),
        "size": int(metadata.st_size),
        "modified_ns": int(metadata.st_mtime_ns),
    }
    if path.is_file():
        if metadata.st_size > MAX_PROCESSING_VERSION_FILE_BYTES:
            raise RuntimeError(
                "Could not establish a bounded strong file version for "
                f"{path}: file exceeds {MAX_PROCESSING_VERSION_FILE_BYTES} bytes"
            )
        version["sha256"] = _processing_file_sha256(path, deadline=deadline)
    elif path.is_dir():
        manifest = _processing_directory_manifest(path, deadline=deadline)
        version.update(manifest)
    return version


def _processing_file_sha256(
    path: Path,
    *,
    deadline: float | None = None,
) -> str:
    deadline = (
        time.monotonic() + MAX_PROCESSING_FINGERPRINT_SECONDS
        if deadline is None
        else float(deadline)
    )
    metadata = path.stat()
    if metadata.st_size > MAX_PROCESSING_VERSION_FILE_BYTES:
        raise RuntimeError(
            "Could not establish a bounded strong file version for "
            f"{path}: file exceeds {MAX_PROCESSING_VERSION_FILE_BYTES} bytes"
        )
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            _check_processing_fingerprint_deadline(deadline)
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def _processing_directory_manifest(
    path: Path,
    *,
    deadline: float | None = None,
) -> dict[str, Any]:
    deadline = (
        time.monotonic() + MAX_PROCESSING_FINGERPRINT_SECONDS
        if deadline is None
        else float(deadline)
    )
    digest = hashlib.sha256()
    entry_count = 0
    content_bytes = 0
    pending = [path]
    while pending:
        _check_processing_fingerprint_deadline(deadline)
        directory = pending.pop()
        entries = []
        try:
            with os.scandir(directory) as iterator:
                for entry in iterator:
                    _check_processing_fingerprint_deadline(deadline)
                    entry_count += 1
                    if entry_count > MAX_PROCESSING_VERSION_DIRECTORY_ENTRIES:
                        raise RuntimeError(
                            "Could not establish a bounded strong directory version for "
                            f"{path}: more than "
                            f"{MAX_PROCESSING_VERSION_DIRECTORY_ENTRIES} entries"
                        )
                    entries.append(entry)
        except OSError as err:
            raise RuntimeError(
                f"Could not establish a strong directory version for {path}: {err}"
            ) from err
        entries.sort(key=lambda item: os.path.normcase(item.name))
        for entry in entries:
            _check_processing_fingerprint_deadline(deadline)
            entry_path = Path(entry.path)
            relative = entry_path.relative_to(path).as_posix().encode("utf-8")
            try:
                if entry.is_symlink():
                    raise RuntimeError(
                        "Could not establish a strong directory version for "
                        f"{path}: symbolic links are unsupported ({entry_path})"
                    )
                elif entry.is_dir(follow_symlinks=False):
                    digest.update(b"D\0" + relative + b"\0")
                    pending.append(entry_path)
                elif entry.is_file(follow_symlinks=False):
                    size = int(entry.stat(follow_symlinks=False).st_size)
                    content_bytes += size
                    if content_bytes > MAX_PROCESSING_VERSION_DIRECTORY_BYTES:
                        raise RuntimeError(
                            "Could not establish a bounded strong directory version for "
                            f"{path}: file content exceeds "
                            f"{MAX_PROCESSING_VERSION_DIRECTORY_BYTES} bytes"
                        )
                    digest.update(b"F\0" + relative + b"\0")
                    digest.update(str(size).encode("ascii") + b"\0")
                    digest.update(
                        _processing_file_sha256(
                            entry_path,
                            deadline=deadline,
                        ).encode("ascii")
                    )
                else:
                    raise RuntimeError(
                        f"Unsupported directory entry while versioning {path}: {entry_path}"
                    )
            except OSError as err:
                raise RuntimeError(
                    f"Could not establish a strong directory version for {path}: {err}"
                ) from err
    return {
        "manifest_sha256": digest.hexdigest(),
        "entry_count": entry_count,
        "content_bytes": content_bytes,
    }


def _check_processing_fingerprint_deadline(deadline: float) -> None:
    if time.monotonic() >= float(deadline):
        raise RuntimeError(
            "Could not establish a bounded strong Processing input version: "
            f"fingerprinting exceeded {MAX_PROCESSING_FINGERPRINT_SECONDS} seconds"
        )


def _path_identity(path: Path) -> str:
    return os.path.normcase(str(path.resolve(strict=False)))


def _stage_processing_output_destinations(
    parameters: dict[str, Any],
    parameter_definitions: Any,
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    staged_parameters = dict(parameters)
    stages = []
    try:
        for plan in _processing_output_plans(parameters, parameter_definitions):
            target = plan["target"]
            target.parent.mkdir(parents=True, exist_ok=True)
            staging_root = Path(
                tempfile.mkdtemp(
                    prefix=f".{target.stem}.qcopilots-stage-",
                    dir=str(target.parent),
                )
            )
            stage = {"staging_root": staging_root}
            stages.append(stage)
            staged_target = staging_root / target.name
            original_value = parameters[plan["parameter"]]
            staged_parameters[plan["parameter"]] = _replace_processing_output_path(
                original_value,
                staged_target,
            )
            stage.update(
                {
                    **plan,
                    "staged_target": staged_target,
                    "authorized_versions": [
                        _processing_file_version(path) for path in plan["family_paths"]
                    ],
                }
            )
    except Exception as err:
        residuals = _cleanup_processing_stages(stages)
        if residuals:
            raise RuntimeError(
                f"{err}. Processing stage preparation cleanup was incomplete: "
                + ", ".join(residuals)
            ) from err
        raise
    return staged_parameters, stages


def _replace_processing_output_path(value: Any, staged_target: Path) -> Any:
    if isinstance(value, Path):
        return staged_target
    raw = str(value)
    _, separator, options = raw.partition("|")
    return str(staged_target) + (separator + options if separator else "")


def _promote_processing_outputs(
    stages: list[dict[str, Any]],
    *,
    overwrite: bool,
) -> dict[str, Any]:
    receipt: dict[str, Any] = {
        "published": [],
        "published_versions": [],
        "backups": [],
        "backup_roots": [],
        "rollback_complete": None,
        "rollback_errors": [],
        "residual_paths": [],
    }
    try:
        for stage in stages:
            staging_root = stage["staging_root"]
            artifacts = sorted(staging_root.iterdir(), key=lambda item: item.name)
            if not artifacts:
                raise RuntimeError(
                    "Processing completed without creating the staged output for "
                    f"{stage['parameter']}"
                )
            existing = [
                path for path in stage["family_paths"] if path.exists()
            ]
            artifact_targets = [
                stage["target"].parent / artifact.name for artifact in artifacts
            ]
            existing.extend(
                path
                for path in artifact_targets
                if path.exists() and path not in existing
            )
            authorized_versions = {
                _path_identity(Path(item["path"])): item
                for item in stage["authorized_versions"]
            }
            for path in existing:
                expected = authorized_versions.get(_path_identity(path))
                if expected is None:
                    raise RuntimeError(
                        "Processing produced an output sidecar which collided with an "
                        f"unpreviewed existing path: {path}"
                    )
                if _processing_file_version(path) != expected:
                    raise RuntimeError(
                        "Processing overwrite target changed after confirmation: "
                        f"{path}"
                    )
            if existing and not overwrite:
                raise FileExistsError(
                    "Processing output appeared after preflight and was not "
                    "overwritten: " + ", ".join(str(path) for path in existing)
                )
            if existing:
                backup_root = Path(
                    tempfile.mkdtemp(
                        prefix=f".{stage['target'].stem}.qcopilots-backup-",
                        dir=str(stage["target"].parent),
                    )
                )
                receipt["backup_roots"].append(backup_root)
                for index, path in enumerate(existing):
                    backup = backup_root / f"{index}-{path.name}"
                    original_version = authorized_versions[_path_identity(path)]
                    os.replace(path, backup)
                    backup_item = {
                        "backup": backup,
                        "target": path,
                        "original_version": original_version,
                    }
                    receipt["backups"].append(backup_item)
                    moved_version = _processing_file_version(backup)
                    moved_version["path"] = str(path)
                    if moved_version != original_version:
                        raise RuntimeError(
                            "Processing overwrite target changed while it was being "
                            f"backed up: {path}"
                        )
            for artifact, target in zip(artifacts, artifact_targets):
                if artifact.is_dir():
                    raise RuntimeError(
                        "Processing directory outputs are unsupported by staged "
                        f"publication: {target}"
                    )
                _link_processing_file_no_clobber(artifact, target)
                artifact.unlink()
                receipt["published"].append(target)
                receipt["published_versions"].append(
                    {
                        "target": target,
                        "version": _processing_file_version(target),
                    }
                )
        return receipt
    except Exception as err:
        rollback = _rollback_processing_output_promotion(receipt)
        if not rollback["complete"]:
            raise RuntimeError(
                f"{err}. Processing output rollback was incomplete, residual paths: "
                + ", ".join(rollback["residual_paths"])
            ) from err
        raise


def _rollback_processing_output_promotion(receipt: dict[str, Any]) -> dict[str, Any]:
    errors = []
    backups_by_target = {
        _path_identity(item["target"]): item
        for item in receipt.get("backups", [])
    }
    published_versions = {
        _path_identity(item["target"]): item["version"]
        for item in receipt.get("published_versions", [])
    }
    for path in reversed(receipt.get("published", [])):
        try:
            backup = backups_by_target.get(_path_identity(path))
            current = _processing_file_version(path)
            if backup is not None and _processing_version_signature(current) == (
                _processing_version_signature(backup["original_version"])
            ):
                continue
            published_version = published_versions.get(_path_identity(path))
            if current.get("exists") and (
                published_version is None or current != published_version
            ):
                errors.append(
                    f"retain concurrent target {path}: content no longer matches "
                    "the output published by this job"
                )
                continue
            _remove_processing_path(path)
        except Exception as err:
            errors.append(f"remove {path}: {err}")
    for item in reversed(receipt.get("backups", [])):
        backup = item["backup"]
        target = item["target"]
        try:
            if _processing_version_signature(
                _processing_file_version(target)
            ) == _processing_version_signature(item["original_version"]):
                if backup.exists():
                    _remove_processing_path(backup)
                continue
            if target.exists():
                errors.append(
                    f"retain concurrent target {target}: original backup remains at "
                    f"{backup}"
                )
                continue
            if backup.exists():
                _link_processing_file_no_clobber(backup, target)
                backup.unlink()
            if _processing_version_signature(_processing_file_version(target)) != (
                _processing_version_signature(item["original_version"])
            ):
                raise RuntimeError("restored content does not match its backup snapshot")
        except Exception as err:
            errors.append(f"restore {target}: {err}")
    for root in receipt.get("backup_roots", []):
        try:
            if root.exists():
                outstanding_backups = [
                    item["backup"]
                    for item in receipt.get("backups", [])
                    if item["backup"].exists()
                    and item["backup"].parent == root
                ]
                if outstanding_backups:
                    errors.append(
                        "retain backup directory "
                        f"{root}: restoration is incomplete"
                    )
                    continue
                shutil.rmtree(root)
                if root.exists():
                    raise RuntimeError("directory still exists after removal")
        except Exception as err:
            errors.append(f"remove backup directory {root}: {err}")
    residual_paths = _processing_receipt_residual_paths(receipt)
    receipt["rollback_errors"] = errors
    receipt["residual_paths"] = residual_paths
    receipt["rollback_complete"] = not errors and not residual_paths
    return {
        "complete": receipt["rollback_complete"],
        "errors": list(errors),
        "residual_paths": list(residual_paths),
    }


def _commit_processing_output_promotion(receipt: dict[str, Any]) -> list[str]:
    residuals = []
    for root in receipt.get("backup_roots", []):
        try:
            if root.exists():
                shutil.rmtree(root)
                if root.exists():
                    raise RuntimeError("directory still exists after removal")
        except Exception:
            residuals.append(str(root))
    return residuals


def _cleanup_processing_stages(stages: list[dict[str, Any]]) -> list[str]:
    residuals = []
    for stage in stages:
        root = stage["staging_root"]
        try:
            if root.exists():
                shutil.rmtree(root)
                if root.exists():
                    raise RuntimeError("directory still exists after removal")
        except Exception:
            residuals.append(str(root))
    return residuals


def _remove_processing_path(path: Path) -> None:
    if path.is_dir():
        shutil.rmtree(path)
    else:
        path.unlink(missing_ok=True)


def _link_processing_file_no_clobber(source: Path, target: Path) -> None:
    try:
        os.link(source, target)
    except FileExistsError as err:
        raise FileExistsError(
            f"Processing output target appeared concurrently and was preserved: {target}"
        ) from err


def _processing_version_signature(version: dict[str, Any]) -> dict[str, Any]:
    signature = {
        key: version[key]
        for key in ("exists", "is_directory", "size")
        if key in version
    }
    if "sha256" in version:
        signature["sha256"] = version["sha256"]
    elif "manifest_sha256" in version:
        signature["manifest_sha256"] = version["manifest_sha256"]
        signature["entry_count"] = version.get("entry_count")
        signature["content_bytes"] = version.get("content_bytes")
    elif "modified_ns" in version:
        signature["modified_ns"] = version["modified_ns"]
    return signature


def _processing_receipt_residual_paths(receipt: dict[str, Any]) -> list[str]:
    backups_by_target = {
        _path_identity(item["target"]): item
        for item in receipt.get("backups", [])
    }
    candidates = [
        path
        for path in receipt.get("published", [])
        if _path_identity(path) not in backups_by_target
    ]
    for item in backups_by_target.values():
        current = _processing_file_version(item["target"])
        if _processing_version_signature(current) != _processing_version_signature(
            item["original_version"]
        ):
            candidates.append(item["target"])
    candidates.extend(item["backup"] for item in receipt.get("backups", []))
    candidates.extend(receipt.get("backup_roots", []))
    return sorted({str(path) for path in candidates if path.exists()})


def _validate_processing_versions(
    expected_versions: list[dict[str, Any]],
    message: str,
) -> None:
    deadline = time.monotonic() + MAX_PROCESSING_FINGERPRINT_SECONDS
    changed = []
    for expected in expected_versions:
        if expected.get("kind", "file") == "project_layer":
            layer = _processing_project_layer_by_id(expected["layer_id"])
            current = (
                _processing_project_layer_version(layer)
                if layer is not None
                else {"kind": "project_layer", "layer_id": expected["layer_id"], "exists": False}
            )
            if layer is not None and "content_fingerprint" in expected:
                current["content_fingerprint"] = (
                    _processing_project_layer_content_fingerprint(
                        layer,
                        deadline=deadline,
                    )
                )
            current["immutable_file_snapshot"] = expected.get(
                "immutable_file_snapshot",
                False,
            )
            identity = f"layer:{expected['layer_id']}"
        else:
            current = _processing_file_version(
                Path(expected["path"]),
                deadline=deadline,
            )
            identity = str(expected["path"])
        if current != expected:
            changed.append(identity)
    if changed:
        raise RuntimeError(f"{message}: " + ", ".join(changed))


def _processing_project_layer_by_id(layer_id: str) -> Any | None:
    try:
        from qgis.core import QgsProject

        return QgsProject.instance().mapLayer(str(layer_id))
    except Exception:
        return None


def _validate_processing_stage_targets(stages: list[dict[str, Any]]) -> None:
    for stage in stages:
        _validate_processing_versions(
            stage["authorized_versions"],
            "Processing output target changed after authorization",
        )


def _rewrite_processing_output_results(
    value: Any,
    stages: list[dict[str, Any]],
) -> Any:
    if isinstance(value, dict):
        return {
            key: _rewrite_processing_output_results(item, stages)
            for key, item in value.items()
        }
    if isinstance(value, list):
        return [_rewrite_processing_output_results(item, stages) for item in value]
    if isinstance(value, tuple):
        return tuple(_rewrite_processing_output_results(item, stages) for item in value)
    if isinstance(value, Path):
        value = str(value)
    if not isinstance(value, str):
        return value
    rewritten = value
    for stage in stages:
        rewritten = rewritten.replace(
            str(stage["staged_target"]),
            str(stage["target"]),
        )
    return rewritten


def _rewrite_processing_context_layer_destinations(
    context: Any,
    stages: list[dict[str, Any]],
) -> None:
    """Point QGIS post-processing at promoted outputs, not staging paths."""

    try:
        layers = context.layersToLoadOnCompletion()
        setter = context.setLayersToLoadOnCompletion
    except (AttributeError, TypeError):
        return
    rewritten: dict[str, Any] = {}
    for destination, details in layers.items():
        published_destination = _rewrite_processing_output_results(
            str(destination),
            stages,
        )
        if published_destination in rewritten:
            raise RuntimeError(
                "Promoted Processing outputs resolve to the same layer destination: "
                f"{published_destination}"
            )
        rewritten[published_destination] = details
    setter(rewritten)


def _cleanup_processing_runtime_outputs(
    runtime: _ProcessingRuntime,
    *,
    terminal_state: str | None = None,
) -> None:
    cleanup_residuals = []
    _cleanup_processing_project_layers(runtime)
    if runtime.output_promotion_receipt is not None:
        successful_publication = (
            runtime.output_promotion_state == "published_success"
            or terminal_state == "succeeded"
        )
        if successful_publication:
            runtime.output_promotion_state = "published_success"
            cleanup_residuals.extend(
                _commit_processing_output_promotion(
                    runtime.output_promotion_receipt
                )
            )
            if not cleanup_residuals:
                runtime.output_promotion_receipt = None
                runtime.output_promotion_state = "none"
        else:
            rollback = _rollback_processing_output_promotion(
                runtime.output_promotion_receipt
            )
            cleanup_residuals.extend(rollback["residual_paths"])
            if rollback["complete"]:
                runtime.output_promotion_receipt = None
                runtime.output_promotion_state = "none"
    residuals = _cleanup_processing_stages(runtime.output_stages)
    cleanup_residuals.extend(residuals)
    if not residuals:
        runtime.output_stages.clear()
    runtime.cleanup_residual_paths = sorted(set(cleanup_residuals))


def _processing_cleanup_receipt(runtime: _ProcessingRuntime) -> dict[str, Any]:
    complete = not (
        runtime.cleanup_residual_paths
        or runtime.cleanup_residual_layer_ids
        or runtime.cleanup_errors
    )
    return {
        "complete": complete,
        "residual_paths": list(runtime.cleanup_residual_paths),
        "residual_layer_ids": list(runtime.cleanup_residual_layer_ids),
        "errors": list(runtime.cleanup_errors),
        "retry_recommended": not complete,
    }


def _processing_project_layer_ids() -> set[str]:
    try:
        from qgis.core import QgsProject

        return set(QgsProject.instance().mapLayers())
    except Exception as err:
        raise RuntimeError(
            f"Could not query QGIS project layers during cleanup: {err}"
        ) from err


def _remove_processing_project_layers(layer_ids: set[str]) -> None:
    if not layer_ids:
        return
    try:
        from qgis.core import QgsProject

        QgsProject.instance().removeMapLayers(sorted(layer_ids))
    except Exception as err:
        raise RuntimeError(
            f"Could not remove QGIS project layers during cleanup: {err}"
        ) from err


def _cleanup_processing_project_layers(runtime: _ProcessingRuntime) -> None:
    initial = runtime.postprocess_initial_layer_ids
    runtime.cleanup_residual_layer_ids = []
    runtime.cleanup_errors = []
    if initial is None:
        return
    try:
        current = _processing_project_layer_ids()
    except Exception as err:
        runtime.cleanup_errors = [str(err)]
        return
    added = current - initial
    if not added:
        runtime.postprocess_initial_layer_ids = None
        return
    residual = set(added)
    try:
        _remove_processing_project_layers(added)
    except Exception as err:
        runtime.cleanup_errors.append(str(err))
    try:
        residual &= _processing_project_layer_ids()
    except Exception as err:
        runtime.cleanup_errors.append(str(err))
    runtime.cleanup_residual_layer_ids = sorted(str(item) for item in residual)
    if runtime.cleanup_residual_layer_ids:
        runtime.cleanup_errors.append(
            "QGIS project layers remain after cleanup: "
            + ", ".join(runtime.cleanup_residual_layer_ids)
        )
    if not runtime.cleanup_residual_layer_ids and not runtime.cleanup_errors:
        runtime.postprocess_initial_layer_ids = None


def _processing_definition_is_destination(definition: Any) -> bool:
    try:
        return bool(definition.isDestination())
    except Exception:
        try:
            definition_type = str(definition.type()).lower()
        except Exception:
            return False
        return definition_type == "sink" or definition_type.endswith("destination")


def _processing_definition_is_folder_destination(definition: Any) -> bool:
    try:
        definition_type = str(definition.type()).replace("_", "").lower()
    except Exception:
        definition_type = type(definition).__name__.lower()
    return "folderdestination" in definition_type


def _processing_file_destination(value: Any) -> Path | None:
    if not isinstance(value, (str, Path)):
        return None
    raw = str(value).strip()
    if not raw or raw.upper() == "TEMPORARY_OUTPUT" or raw.lower().startswith("memory:"):
        return None
    raw = raw.split("|", 1)[0]
    if raw.lower().startswith("file://"):
        parsed = urlparse(raw)
        raw = unquote(parsed.path)
        if parsed.netloc:
            raw = f"//{parsed.netloc}{raw}"
        if re.match(r"^/[A-Za-z]:/", raw):
            raw = raw[1:]
    has_windows_drive = bool(re.match(r"^[A-Za-z]:", raw))
    if not has_windows_drive and re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", raw):
        return None
    if "=" in raw and not any(separator in raw for separator in ("/", "\\")):
        return None
    return Path(raw).expanduser().resolve(strict=False)


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
    from qcopilots_common.processing_metadata import (
        processing_algorithm_matches_domain,
    )

    return processing_algorithm_matches_domain(algorithm, category)


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
    if category not in {"vector", "raster", "general"}:
        raise ValueError("category must be vector, raster or general")
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
