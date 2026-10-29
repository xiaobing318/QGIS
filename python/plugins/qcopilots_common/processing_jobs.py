"""Persistent asynchronous QGIS Processing jobs for the QCopilots bridge.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import copy
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
from qcopilots_common.processing_file_worker import (
    ProcessingFileCancelled, file_work_context as _processing_file_work_context,
    in_file_work, check_file_cancelled, report_file_progress, make_file_task,
)
from qcopilots_common.processing_input_cache import capture_input_cache, compare_input_cache


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
    cleanup_file_errors: list[str] = field(default_factory=list)
    task: Any = None
    file_task: Any = None
    file_callbacks: int = 0
    pending_callbacks: int = 0
    algorithm_result_received: bool = False
    cancel_event: threading.Event = field(default_factory=threading.Event)
    operation: str = "execute"
    approved_snapshot: dict[str, Any] | None = None
    input_templates: list[dict[str, Any]] = field(default_factory=list)
    output_plans: list[dict[str, Any]] = field(default_factory=list)
    band_counts: dict[str, int] = field(default_factory=dict)
    cache_changes: list[dict[str, Any]] = field(default_factory=list)
    pending_results: Any = None
    pending_terminal: dict[str, Any] | None = None
    phase: str = "preflight"
    scheduling_algorithm: bool = False
    feedback_messages: list[str] = field(default_factory=list)
    sink_counts: dict[str, int] = field(default_factory=dict)
    lock: threading.RLock = field(default_factory=threading.RLock)

    def release_completed_objects(self, *, keep_context: bool) -> None:
        """Release QGIS objects after their task no longer references context."""

        with self.lock:
            if self.file_task is not None or self.scheduling_algorithm or self.pending_callbacks:
                return
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
            self.pending_results = None
            self.pending_terminal = None
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

    def issue(self, fingerprint: str, *, snapshot=None, request_fingerprint=None) -> str:
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
                "snapshot": copy.deepcopy(snapshot),
                "request_fingerprint": request_fingerprint,
                "expires_at": self._clock() + self.ttl_seconds,
                "sequence": self._next_sequence,
            }
            return token

    def claim(self, token: str, request_fingerprint: str) -> dict[str, Any]:
        with self._lock:
            entry = self._entries.pop(str(token), None)
            if entry is None:
                raise RuntimeError("Processing overwrite confirmation token is invalid or already used")
            if entry["expires_at"] <= self._clock():
                raise RuntimeError("Processing overwrite confirmation token has expired")
            if entry.get("request_fingerprint") != request_fingerprint or not isinstance(entry.get("snapshot"), dict):
                raise RuntimeError("Processing parameters changed after preview. Request a new confirmation token.")
            return copy.deepcopy(entry["snapshot"])

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
        self.store.add_retention_guard(_processing_runtime_requires_retention)
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
        if overwrite_confirmation_token and not overwrite_outputs:
            raise ValueError("overwrite_confirmation_token requires overwrite_outputs=true")
        operation = "preview" if overwrite_outputs and not overwrite_confirmation_token else "execute"
        existing = self.store.lookup_idempotent(
            category, client_request_id, request_fingerprint, idempotency_namespace=operation
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
        definitions = list(algorithm.parameterDefinitions())
        custom_authorizer = self._output_authorizer()
        if custom_authorizer is not _authorize_processing_output_destinations:
            custom_authorizer(algorithm, sanitized_parameters, definitions,
                              overwrite_outputs, overwrite_confirmation_token,
                              self._overwrite_confirmations)
        elif not any(_processing_definition_is_destination(item) for item in definitions):
            raise RuntimeError("Processing algorithm has no separate destination and is unsupported by the MCP start service")
        plans = _processing_output_plans(sanitized_parameters, definitions)
        templates = _processing_input_versions(sanitized_parameters, definitions, defer_files=True)
        runtime = _ProcessingRuntime(
            algorithm=algorithm, parameters=dict(sanitized_parameters), context=None,
            feedback=None, add_outputs_to_project=add_outputs, generation=generation,
            overwrite_outputs=overwrite_outputs, operation=operation,
            input_templates=templates, output_plans=plans,
            band_counts=_processing_raster_band_counts(templates),
        )
        job_id = str(uuid.uuid4())
        execution_mode = "main_thread" if self._is_no_threading()(algorithm) else "qgs_task"
        initial = {
            "job_id": job_id, "category": category, "client_request_id": client_request_id,
            "state": "queued", "operation": operation, "phase": "preflight",
            "phase_progress": {"bytes_completed": 0, "bytes_total": 0},
            "algorithm_executed": False, "execution_mode": execution_mode,
            "cancellable": True, "overwrite_outputs": overwrite_outputs,
            "algorithm": _algorithm_summary(algorithm), "progress_percent": 0.0,
            "progress_text": "Processing file preflight queued", "processed_count": 0,
            "cancel_requested": False, "created_at": utc_now_rfc3339(),
            "started_at": None, "finished_at": None, "error": None,
        }
        with self._lock:
            if not self._accepting or generation != self._generation:
                raise RuntimeError("QCopilots Processing job manager is stopping")
            # Recheck after metadata preparation. Concurrent submissions must
            # share one admission decision, and replays never need a new slot.
            existing = self.store.lookup_idempotent(
                category, client_request_id, request_fingerprint,
                idempotency_namespace=operation,
            )
            if existing is not None:
                return existing
            retained_ids = {
                job_id for job_id, scope, _ in self.store.active_runtime_items()
                if scope == category
            }
            retained_ids.update(
                job_id for job_id, scope, owned in self.store.terminal_runtime_items()
                if scope == category and _processing_runtime_requires_retention(owned)
            )
            capacity = max(1, self.store.max_terminal_jobs_per_scope)
            if len(retained_ids) >= capacity:
                raise RuntimeError(
                    "processing_recovery_capacity_exceeded: "
                    f"{category} already has {capacity} active or retained recovery jobs. "
                    "Query and finish recovery of existing jobs before starting new work."
                )
            snapshot, created = self.store.create(
                category, initial, client_request_id=client_request_id,
                request_fingerprint=request_fingerprint, runtime=runtime,
                idempotency_namespace=operation,
            )
        if not created:
            return snapshot
        try:
            if overwrite_confirmation_token:
                runtime.approved_snapshot = self._overwrite_confirmations.claim(
                    overwrite_confirmation_token, request_fingerprint,
                )
            self._submit_file_work(
                job_id, category, runtime, "preflight",
                lambda: _processing_file_preflight(runtime),
                lambda result: self._preflight_completed(
                    job_id, category, runtime, result, request_fingerprint),
            )
        except Exception as err:
            return self._finish_failed(job_id, category, "preflight_failed", "preflight", str(err), runtime)
        return self.store.get(job_id, category)

    def _submit_file_work(self, job_id, category, runtime, phase, function, completed, *, recovery=False):
        if runtime.file_task is not None or runtime.pending_callbacks:
            raise RuntimeError("Processing file task is already active")
        runtime.phase = phase
        snapshot = self.store.get(job_id, category)
        if snapshot["state"] == "queued":
            self.store.transition(job_id, "running", category, started_at=utc_now_rfc3339())
        self._safe_patch(job_id, category, phase=phase, cancellable=not recovery,
                         phase_progress={"bytes_completed": 0, "bytes_total": 0})
        cancel_event = None if recovery else runtime.cancel_event
        def progress(done, total):
            self._safe_patch(job_id, category, phase=runtime.phase, phase_progress={"bytes_completed": done, "bytes_total": total})
        def complete(error, value):
            if error is not None:
                if recovery:
                    runtime.cleanup_errors.append(str(error))
                    runtime.cleanup_file_errors.append(str(error))
                    completed(None)
                elif isinstance(error, ProcessingFileCancelled):
                    self._finish_cancelled(job_id, category, runtime)
                else:
                    self._finish_failed(job_id, category, "file_operation_failed", phase, str(error), runtime)
                return
            if not recovery and (runtime.cancel_event.is_set() or runtime.generation != self._generation):
                self._finish_cancelled(job_id, category, runtime)
                return
            try:
                completed(value)
            except Exception as err:
                code = "output_postprocessing_failed" if phase == "verify_outputs" else "file_completion_failed"
                stage = "postprocess" if phase == "verify_outputs" else phase
                self._finish_failed(job_id, category, code, stage, str(err), runtime)
        completion_received = False
        def done(error, value):
            nonlocal completion_received
            if completion_received:
                return
            completion_received = True
            # finished() is still inside QgsTaskManager's completion stack.
            # Detach that completed task now, but retain its plain result and
            # runtime until the queued continuation owns the next phase.
            runtime.file_callbacks += 1
            try:
                self._defer_continuation(
                    runtime, lambda: complete(error, value), release_file_task=True
                )
            except Exception as err:
                self._continuation_schedule_failed(job_id, category, runtime, err)
            finally:
                runtime.file_callbacks -= 1
        injected = self._dependencies.get("file_runner")
        if injected is not None:
            runtime.file_task = True
            injected(function, done, cancel_event, progress)
            return
        task = make_file_task(function, done, cancel_event=cancel_event, progress=progress)
        runtime.file_task = task
        try:
            if not self._task_manager().addTask(task):
                raise RuntimeError("QGIS task manager rejected the Processing file task")
        except Exception:
            runtime.file_task = None
            raise

    def _defer_continuation(self, runtime, callback, *, release_file_task=False):
        """Resume only after the current QgsTask completion stack unwinds."""
        runtime.pending_callbacks += 1
        if release_file_task:
            runtime.file_task = None
        resumed = False
        def resume():
            nonlocal resumed
            if resumed:
                return
            resumed = True
            runtime.pending_callbacks -= 1
            runtime.file_callbacks += 1
            try:
                callback()
            finally:
                runtime.file_callbacks -= 1
        try:
            scheduler = self._dependencies.get("continuation_scheduler")
            if scheduler is None:
                self._single_shot()(0, resume)
            else:
                scheduler(resume)
        except Exception:
            if not resumed:
                runtime.pending_callbacks -= 1
            raise

    def _continuation_schedule_failed(self, job_id, category, runtime, error):
        # Do not recursively register recovery while still inside finished().
        # A later get/stop can retry once an application event loop is available.
        runtime.cleanup_file_errors.append(f"Could not queue Processing continuation: {error}")
        runtime.cleanup_residual_paths = _processing_known_recovery_paths(runtime)
        receipt = _processing_cleanup_receipt(runtime)
        if self.store.get(job_id, category)["state"] in TERMINAL_JOB_STATES:
            self.store.patch_cleanup_receipt(job_id, receipt, category)
            return
        self.store.transition(
            job_id, "failed", category, phase="complete", cancellable=False,
            finished_at=utc_now_rfc3339(),
            error={"code": "continuation_schedule_failed", "stage": runtime.phase,
                   "message": str(error)},
            cleanup=receipt,
        )

    def _preflight_completed(self, job_id, category, runtime, result, request_fingerprint):
        _validate_processing_versions(
            [v for v in runtime.input_templates if v.get("kind") == "project_layer"],
            "Processing project input changed during preflight",
        )
        runtime.authorized_input_versions = result["input_versions"]
        if runtime.operation == "preview":
            payload = {
                "confirmation_required": bool(result["collisions"]),
                "algorithm_id": str(runtime.algorithm.id()), "overwrite_outputs": True,
                "overwrite_targets": result["collisions"],
                "input_versions": result["input_versions"],
                "parameters_sha256": _request_fingerprint(json_safe_value(runtime.parameters)),
            }
            if result["collisions"]:
                token = self._overwrite_confirmations.issue(
                    _request_fingerprint(result["approval_snapshot"]),
                    snapshot=result["approval_snapshot"], request_fingerprint=request_fingerprint,
                )
                payload.update(overwrite_confirmation_token=token,
                               confirmation_expires_in_seconds=self._overwrite_confirmations.ttl_seconds,
                               confirmation_expires_at=datetime.fromtimestamp(
                                   time.time()+self._overwrite_confirmations.ttl_seconds, timezone.utc
                               ).isoformat().replace("+00:00", "Z"))
            self.store.transition(job_id, "succeeded", category, phase="complete", cancellable=False,
                                  finished_at=utc_now_rfc3339(), result=payload,
                                  progress_text="Processing overwrite preview completed; algorithm was not run",
                                  cleanup=_processing_cleanup_receipt(runtime), **payload)
            runtime.release_completed_objects(keep_context=False)
            return
        runtime.parameters = result["parameters"]
        runtime.output_stages = result["stages"]
        runtime.feedback = self._feedback_factory()()
        runtime.context = self._context_factory()(runtime.feedback)
        self._connect_feedback(job_id, category, runtime)
        self._schedule_algorithm(job_id, category, runtime)

    def _schedule_algorithm(self, job_id, category, runtime):
        runtime.scheduling_algorithm = True
        try:
            return self._schedule_algorithm_body(job_id, category, runtime)
        finally:
            runtime.scheduling_algorithm = False
            if runtime.pending_terminal is not None and runtime.file_task is None and runtime.task is None:
                self._recover_runtime(job_id, category, runtime)

    def _schedule_algorithm_body(self, job_id, category, runtime):
        runtime.phase = "execute"
        algorithm = runtime.algorithm
        execution_parameters = runtime.parameters
        add_outputs = runtime.add_outputs_to_project
        feedback, context = runtime.feedback, runtime.context
        execution_mode = self.store.get(job_id, category)["execution_mode"]
        self._safe_patch(job_id, category, phase="execute", phase_progress=None,
                         cancellable=(execution_mode != "main_thread" and bool(self._is_algorithm_cancellable()(algorithm))))
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
                    or current["state"] not in {"queued", "running"}
                    or current.get("cancel_requested")
                ):
                    if current["state"] not in TERMINAL_JOB_STATES:
                        current = self._finish_cancelled(job_id, category, runtime)
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
                    self._algorithm_task_destroyed(job_id, category, runtime)
                ),
            )
            with self._lock:
                current = self.store.get(job_id, category)
                runtime.task = task
                if (
                    not self._accepting
                    or runtime.generation != self._generation
                    or current["state"] not in {"queued", "running"}
                    or current.get("cancel_requested")
                ):
                    if current["state"] not in TERMINAL_JOB_STATES:
                        current = self._finish_cancelled(job_id, category, runtime)
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
            job_id, category = _required_nonempty_string(arguments, "job_id"), None
        snapshot = self.store.get(job_id, category)
        if snapshot["state"] in TERMINAL_JOB_STATES and (snapshot.get("cleanup") or {}).get("complete") is False:
            runtime = self.store.get_runtime(job_id, snapshot["category"])
            if runtime is not None and runtime.file_task is None:
                self._recover_runtime(job_id, snapshot["category"], runtime)
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
            job_id, category = _required_nonempty_string(arguments, "job_id"), None
        snapshot, runtime, requested = self.store.request_cancel(job_id, category)
        if not requested or runtime is None:
            return snapshot
        runtime.cancel_event.set()
        if runtime.pending_callbacks:
            return self.store.get(job_id, snapshot["category"])
        task = runtime.file_task if runtime.file_task is not None else runtime.task
        if task is not None:
            try:
                task.cancel()
            except AttributeError:
                pass
        else:
            self._finish_cancelled(job_id, snapshot["category"], runtime)
        return self.store.get(job_id, snapshot["category"])

    def shutdown(self, timeout_seconds: float = 5) -> dict[str, Any]:
        timeout = float(timeout_seconds)
        if not math.isfinite(timeout) or timeout < 0:
            raise ValueError("timeout_seconds must be finite and non-negative")
        with self._lock:
            if self._accepting:
                self._accepting = False
                self._generation += 1
        self.store.shutdown()
        self._overwrite_confirmations.clear()
        tasks = []
        for job_id, category, runtime in self.store.active_runtime_items():
            if runtime is None:
                continue
            runtime.cancel_event.set()
            try:
                self.store.transition(job_id, "cancelling", category, cancel_requested=True)
            except RuntimeError:
                pass
            # Recovery is allowed to finish even though execution was cancelled.
            for task in (runtime.task, runtime.file_task):
                if task is None or task is True:
                    continue
                if runtime.pending_callbacks and task is runtime.task:
                    continue
                if task is not runtime.file_task or runtime.phase != "cleanup":
                    try:
                        task.cancel()
                    except Exception:
                        pass
            if (runtime.task is None and runtime.file_task is None
                    and not runtime.pending_callbacks):
                self._finish_cancelled(job_id, category, runtime)
        # A terminal algorithm may still own an active recovery attempt.
        # Schedule eligible retries before collecting the bounded wait set.
        for job_id, category, runtime in self.store.terminal_runtime_items():
            if runtime is not None and runtime.file_task is None:
                self._release_finished_runtime(job_id, category, runtime)
        all_items = self.store.active_runtime_items() + self.store.terminal_runtime_items()
        for _, _, runtime in all_items:
            if runtime is None:
                continue
            for task in (runtime.task, runtime.file_task):
                if runtime.pending_callbacks and task is runtime.task:
                    continue
                if task is not None and task is not True and all(task is not seen for seen in tasks):
                    tasks.append(task)
        deadline = time.monotonic() + timeout
        for task in tasks:
            remaining = max(0, int((deadline-time.monotonic())*1000))
            if not remaining:
                break
            wait = getattr(task, "waitForFinished", None)
            if callable(wait):
                try:
                    wait(remaining)
                except Exception:
                    pass
        # finished() callbacks own cleanup and object release on the main
        # thread. A bounded wait never authorizes racing an active worker.
        pending = []
        all_items = self.store.active_runtime_items() + self.store.terminal_runtime_items()
        for job_id, category, runtime in all_items:
            if runtime is None:
                continue
            snapshot = self.store.get(job_id, category)
            receipt = _processing_cleanup_receipt(runtime)
            terminal = snapshot["state"] in TERMINAL_JOB_STATES
            if (terminal and receipt["complete"]
                    and not runtime.file_callbacks and not runtime.pending_callbacks):
                continue
            pending.append(job_id)
            receipt.update(complete=False, retry_recommended=True,
                           residual_paths=_processing_known_recovery_paths(runtime),
                           errors=receipt["errors"] + ["Shutdown reached its wait limit with task completion or recovery still pending"])
            if terminal:
                self.store.patch_cleanup_receipt(job_id, receipt, category)
            else:
                self._safe_patch(job_id, category, cleanup=receipt)
        return {"complete": not pending, "pending_job_ids": pending}

    @staticmethod
    def _cleanup_evicted_runtime(_job_id, runtime):
        if runtime is None:
            return {"complete": True, "residual_paths": [], "residual_layer_ids": [], "errors": [], "retry_recommended": False}
        if _processing_runtime_requires_retention(runtime):
            return {**_processing_cleanup_receipt(runtime), "defer_eviction": True}
        # Called under the job-store lock. Never hash or recover files here.
        receipt = _processing_cleanup_receipt(runtime)
        runtime.release_completed_objects(keep_context=False)
        return receipt

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
            or snapshot["state"] not in {"queued", "running"}
            or snapshot.get("cancel_requested")
        ):
            if snapshot["state"] not in TERMINAL_JOB_STATES:
                self._finish_cancelled(job_id, category, runtime)
            return
        self.store.transition(
            job_id,
            "running",
            category,
            started_at=utc_now_rfc3339(),
            progress_text="Processing algorithm started",
            algorithm_executed=True, phase="execute",
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
        if snapshot["state"] == "cancelling":
            self._safe_patch(job_id, category, algorithm_executed=True, phase="execute")
            return
        if snapshot["state"] not in {"queued", "running"}:
            return
        self.store.transition(
            job_id,
            "running",
            category,
            started_at=utc_now_rfc3339(),
            progress_text="Processing algorithm started",
            algorithm_executed=True, phase="execute",
        )

    def _on_task_executed(self, job_id, category, successful, results):
        try:
            runtime = self.store.get_runtime(job_id, category)
            snapshot = self.store.get(job_id, category)
        except RuntimeError:
            return
        if snapshot["state"] in TERMINAL_JOB_STATES or runtime.algorithm_result_received:
            return
        runtime.algorithm_result_received = True
        runtime.phase = "verify_outputs" if successful else "cleanup"
        self._safe_patch(
            job_id, category, phase=runtime.phase, cancellable=bool(successful),
            phase_progress={"bytes_completed": 0, "bytes_total": 0},
        )
        try:
            self._defer_continuation(
                runtime,
                lambda: self._algorithm_completion(
                    job_id, category, runtime, successful, results),
            )
        except Exception as err:
            self._continuation_schedule_failed(job_id, category, runtime, err)

    def _algorithm_completion(self, job_id, category, runtime, successful, results, *, terminated=False):
        cancelled = (runtime.cancel_event.is_set() or _call_boolean(runtime.feedback, "isCanceled")
                     or _call_boolean(runtime.task, "isCanceled") or runtime.generation != self._generation)
        if cancelled:
            self._finish_cancelled(job_id, category, runtime)
        elif successful:
            self._finish_succeeded(job_id, category, results, runtime)
        elif terminated:
            self._finish_failed(job_id, category, "task_terminated", "execute", "Processing task terminated unexpectedly", runtime)
        else:
            self._finish_failed(job_id, category, "algorithm_execution_failed", "execute",
                                "Processing algorithm reported an unsuccessful result", runtime,
                                details={"results": json_safe_value(results)})

    def _on_task_terminated(self, job_id, category):
        try:
            runtime = self.store.get_runtime(job_id, category)
            snapshot = self.store.get(job_id, category)
        except RuntimeError:
            return
        if (snapshot["state"] in TERMINAL_JOB_STATES or runtime.file_task is not None
                or runtime.algorithm_result_received or runtime.pending_callbacks
                or runtime.phase != "execute"):
            return
        runtime.algorithm_result_received = True
        runtime.phase = "cleanup"
        try:
            self._defer_continuation(
                runtime,
                lambda: self._algorithm_completion(
                    job_id, category, runtime, False, {}, terminated=True),
            )
        except Exception as err:
            self._continuation_schedule_failed(job_id, category, runtime, err)

    def _finish_succeeded(self, job_id, category, results, runtime):
        current = self.store.get(job_id, category)
        if current["state"] in TERMINAL_JOB_STATES:
            return current
        runtime.pending_results = results
        try:
            _validate_processing_versions(
                [v for v in runtime.authorized_input_versions if v.get("kind") == "project_layer"],
                "Processing project input changed while the job was running",
            )
            if runtime.output_stages:
                runtime.output_promotion_receipt = _new_processing_output_promotion_receipt()
            self._submit_file_work(job_id, category, runtime, "verify_outputs",
                                   lambda: _processing_verify_and_publish(runtime),
                                   lambda _: self._outputs_published(job_id, category, runtime))
        except Exception as err:
            return self._finish_failed(job_id, category, "output_postprocessing_failed", "postprocess", str(err), runtime)
        return self.store.get(job_id, category)

    def _outputs_published(self, job_id, category, runtime):
        self._safe_patch(job_id, category, phase="load_outputs", cancellable=False,
                         input_cache_changes=list(runtime.cache_changes))
        published_results = _rewrite_processing_output_results(runtime.pending_results, runtime.output_stages)
        _rewrite_processing_context_layer_destinations(runtime.context, runtime.output_stages)
        if runtime.add_outputs_to_project:
            runtime.postprocess_initial_layer_ids = _processing_project_layer_ids()
            loaded = self._post_processor()(runtime.algorithm, runtime.context, runtime.feedback, published_results)
            if loaded is False:
                raise RuntimeError("One or more result layers could not be loaded")
            runtime.postprocess_initial_layer_ids = None
        runtime.retain_context_for_results = bool(
            not runtime.add_outputs_to_project and _processing_results_require_context(published_results, runtime.context))
        runtime.pending_results = json_safe_value(published_results)
        runtime.output_promotion_state = "published_success"
        runtime.pending_terminal = {"state": "succeeded", "error": None,
                                    "result": runtime.pending_results,
                                    "progress_percent": 100.0,
                                    "progress_text": "Processing algorithm completed"}
        self._recover_runtime(job_id, category, runtime)

    def _finish_cancelled(self, job_id, category, runtime):
        runtime.cancel_event.set()
        runtime.pending_terminal = {"state": "cancelled", "error": None,
                                    "cancel_requested": True,
                                    "progress_text": "Processing job cancelled"}
        self._recover_runtime(job_id, category, runtime)
        return self.store.get(job_id, category)

    def _recover_runtime(self, job_id, category, runtime):
        if runtime.file_task is not None or runtime.scheduling_algorithm or runtime.pending_callbacks:
            return
        _cleanup_processing_project_layers(runtime)
        def recovered(_):
            receipt = _processing_cleanup_receipt(runtime)
            snapshot = self.store.get(job_id, category)
            if snapshot["state"] in TERMINAL_JOB_STATES:
                self.store.patch_cleanup_receipt(
                    job_id, receipt, category,
                    refresh_retention=receipt["complete"],
                )
            else:
                terminal = dict(runtime.pending_terminal or {"state": "failed", "error": {"code": "cleanup_failed"}})
                state = terminal.pop("state")
                if not receipt["complete"] and state != "succeeded":
                    terminal["error"] = {"code": "output_state_indeterminate", "stage": "cleanup",
                                         "message": "Output cleanup was incomplete, inspect residual_paths",
                                         "details": {"original_error": terminal.get("error"), "cleanup": receipt}}
                    state = "failed"
                self.store.transition(job_id, state, category, phase="complete", cancellable=False,
                                      finished_at=utc_now_rfc3339(), cleanup=receipt, **terminal)
            if runtime.task is None and receipt["complete"]:
                runtime.release_completed_objects(keep_context=runtime.retain_context_for_results)
        if not runtime.output_stages and runtime.output_promotion_receipt is None:
            previous_file_errors = set(runtime.cleanup_file_errors)
            runtime.cleanup_errors = [e for e in runtime.cleanup_errors if e not in previous_file_errors]
            runtime.cleanup_file_errors.clear()
            runtime.cleanup_residual_paths.clear()
            recovered(None)
            return
        try:
            self._submit_file_work(job_id, category, runtime, "cleanup",
                                   lambda: _cleanup_processing_runtime_file_outputs(runtime), recovered,
                                   recovery=True)
        except Exception as err:
            runtime.cleanup_file_errors.append(f"Could not schedule file recovery: {err}")
            runtime.cleanup_residual_paths = _processing_known_recovery_paths(runtime)
            recovered(None)

    def _finish_failed(self, job_id, category, code, stage, message, runtime, details=None):
        with runtime.lock:
            feedback_messages = list(runtime.feedback_messages)
        runtime.pending_terminal = {"state": "failed", "progress_text": message,
            "error": {"code": code, "stage": stage, "message": message,
                      "details": json_safe_value(details), "feedback_messages": feedback_messages}}
        self._recover_runtime(job_id, category, runtime)
        return self.store.get(job_id, category)

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

    def _release_finished_runtime(self, job_id, category, runtime):
        if runtime.file_task is not None or runtime.task is not None or runtime.pending_callbacks:
            return
        try:
            snapshot = self.store.get(job_id, category)
        except RuntimeError:
            return
        if snapshot["state"] not in TERMINAL_JOB_STATES:
            return
        if (snapshot.get("cleanup") or {}).get("complete", True):
            runtime.release_completed_objects(keep_context=runtime.retain_context_for_results)
        else:
            self._recover_runtime(job_id, category, runtime)

    def _algorithm_task_destroyed(self, job_id, category, runtime):
        runtime.task = None
        self._release_finished_runtime(job_id, category, runtime)

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
        if is_container and path.exists() and separator and options:
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
    *, defer_files: bool = False,
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
        ({"path": str(path), "_pending_file": True} if defer_files else _processing_file_version(path, deadline=deadline))
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
        if not in_file_work() and metadata.st_size > MAX_PROCESSING_VERSION_FILE_BYTES:
            raise RuntimeError(
                "Could not establish a bounded strong file version for "
                f"{path}: file exceeds {MAX_PROCESSING_VERSION_FILE_BYTES} bytes"
            )
        version["sha256"] = _processing_file_sha256(path, deadline=deadline)
        after = path.stat()
        if (metadata.st_dev, metadata.st_ino, metadata.st_size, metadata.st_mtime_ns, metadata.st_ctime_ns) != (
            after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns):
            raise RuntimeError(f"Processing file changed while establishing its version: {path}")
    elif path.is_dir():
        manifest = _processing_directory_manifest(path, deadline=deadline)
        version.update(manifest)
    return version


def _processing_file_sha256(path: Path, *, deadline: float | None = None, enforce_deadline: bool = False) -> str:
    background = in_file_work()
    deadline = time.monotonic() + MAX_PROCESSING_FINGERPRINT_SECONDS if deadline is None else float(deadline)
    def identity(stat):
        return (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns)
    before = path.stat()
    if not background and before.st_size > MAX_PROCESSING_VERSION_FILE_BYTES:
        raise RuntimeError(f"Could not establish a bounded strong file version for {path}: file exceeds {MAX_PROCESSING_VERSION_FILE_BYTES} bytes")
    # File timestamps can remain unchanged for a same-size concurrent write,
    # including after a Windows writer closes. Verify two complete observations
    # instead of treating unchanged metadata as evidence of unchanged content.
    # This remains a consistency check, not an atomic filesystem snapshot.
    total = before.st_size * 2
    report_file_progress(0, total)
    observed_digest = None
    for pass_index in range(2):
        digest = hashlib.sha256()
        completed = 0
        check_file_cancelled()
        with path.open("rb") as handle:
            opened = os.fstat(handle.fileno())
            if identity(before) != identity(opened):
                raise RuntimeError(f"Processing file changed before fingerprinting: {path}")
            while completed < before.st_size:
                check_file_cancelled()
                if not background or enforce_deadline:
                    _check_processing_fingerprint_deadline(deadline)
                chunk = handle.read(min(1024 * 1024, before.st_size - completed))
                if not chunk:
                    raise RuntimeError(f"Processing file was truncated during fingerprinting: {path}")
                digest.update(chunk)
                completed += len(chunk)
                report_file_progress(pass_index * before.st_size + completed, total)
            after_handle = os.fstat(handle.fileno())
            if handle.read(1) or identity(after_handle) != identity(opened) or after_handle.st_ctime_ns != opened.st_ctime_ns:
                raise RuntimeError(f"Processing file changed during fingerprinting: {path}")
        after_path = path.stat()
        if identity(after_path) != identity(before) or after_path.st_ctime_ns != before.st_ctime_ns:
            raise RuntimeError(f"Processing file changed after fingerprinting: {path}")
        check_file_cancelled()
        if not background or enforce_deadline:
            _check_processing_fingerprint_deadline(deadline)
        current_digest = digest.hexdigest()
        if observed_digest is not None and current_digest != observed_digest:
            raise RuntimeError(f"Processing file changed between fingerprint observations: {path}")
        observed_digest = current_digest
    return observed_digest


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
                            enforce_deadline=True,
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
    receipt: dict[str, Any] | None = None,
) -> dict[str, Any]:
    # The asynchronous caller owns the receipt throughout publication. The
    # legacy standalone helper still rolls back on failure before returning.
    caller_owns_receipt = receipt is not None
    receipt = receipt if receipt is not None else _new_processing_output_promotion_receipt()
    if receipt.get("published") or receipt.get("backups") or receipt.get("backup_roots"):
        raise RuntimeError("Processing output receipt already contains a publication")
    prepared = []
    target_identities = set()
    try:
        # Validate every stage before moving even the first existing target.
        for stage in stages:
            artifacts = sorted(stage["staging_root"].iterdir(), key=lambda item: item.name)
            if not artifacts:
                raise RuntimeError(
                    "Processing completed without creating the staged output for "
                    f"{stage['parameter']}"
                )
            if stage["staged_target"] not in artifacts:
                raise RuntimeError(
                    "Processing completed without creating its requested output file for "
                    f"{stage['parameter']}"
                )
            authorized_versions = {
                _path_identity(Path(item["path"])): item
                for item in stage["authorized_versions"]
            }
            existing = []
            for path in stage["family_paths"]:
                expected = authorized_versions.get(_path_identity(path))
                current = _processing_file_version(path)
                if expected is None or current != expected:
                    raise RuntimeError(
                        "Processing overwrite target changed after confirmation: "
                        f"{path}"
                    )
                if current.get("exists"):
                    existing.append(path)
            artifact_plans = []
            for artifact in artifacts:
                if artifact.is_symlink():
                    raise RuntimeError(
                        f"Processing staged publication does not support symbolic links: {artifact}"
                    )
                version = _processing_file_version(artifact)
                if not version.get("exists") or version.get("is_directory") or "sha256" not in version:
                    raise RuntimeError(
                        "Processing staged publication requires regular file outputs: "
                        f"{artifact}"
                    )
                target = stage["target"].parent / artifact.name
                identity = _path_identity(target)
                if identity in target_identities:
                    raise RuntimeError(f"Processing staged outputs resolve to the same target: {target}")
                target_identities.add(identity)
                if identity not in authorized_versions:
                    target_version = _processing_file_version(target)
                    if target_version.get("exists"):
                        raise RuntimeError(
                            "Processing produced an output sidecar which collided with an "
                            f"unpreviewed existing path: {target}"
                        )
                artifact_plans.append((artifact, target, version))
            if existing and not overwrite:
                raise FileExistsError(
                    "Processing output appeared after preflight and was not overwritten: "
                    + ", ".join(str(path) for path in existing)
                )
            prepared.append((stage, authorized_versions, existing, artifact_plans))

        for stage, authorized_versions, existing, artifact_plans in prepared:
            # Earlier stages may have taken a long time to hash. Recheck the
            # complete family, including paths which were originally absent.
            for path in stage["family_paths"]:
                if _processing_file_version(path) != authorized_versions[_path_identity(path)]:
                    raise RuntimeError(
                        f"Processing output family changed before publication: {path}"
                    )
            if existing:
                backup_root = Path(tempfile.mkdtemp(
                    prefix=f".{stage['target'].stem}.qcopilots-backup-",
                    dir=str(stage["target"].parent),
                ))
                receipt["backup_roots"].append(backup_root)
                for index, path in enumerate(existing):
                    original_version = authorized_versions[_path_identity(path)]
                    if _processing_file_version(path) != original_version:
                        raise RuntimeError(
                            f"Processing overwrite target changed before backup: {path}"
                        )
                    backup = backup_root / f"{index}-{path.name}"
                    os.replace(path, backup)
                    # Record the move before any hashing can fail or be cancelled.
                    receipt["backups"].append({
                        "backup": backup,
                        "target": path,
                        "original_version": original_version,
                    })
                    moved_version = _processing_file_version(backup)
                    moved_version["path"] = str(path)
                    if moved_version != original_version:
                        raise RuntimeError(
                            "Processing overwrite target changed while it was being "
                            f"backed up: {path}"
                        )
            for artifact, target, artifact_version in artifact_plans:
                if _processing_file_version(artifact) != artifact_version:
                    raise RuntimeError(f"Processing staged output changed before publication: {artifact}")
                published_version = dict(artifact_version, path=str(target))
                _link_processing_file_no_clobber(artifact, target)
                # The target exists from this instant. In particular, unlink of
                # the staged name must not precede receipt registration.
                receipt["published"].append(target)
                receipt["published_versions"].append({
                    "target": target,
                    "version": published_version,
                })
                artifact.unlink()
                if _processing_file_version(target) != published_version:
                    raise RuntimeError(f"Processing output changed during publication: {target}")
        for stage, _authorized_versions, _existing, artifact_plans in prepared:
            expected_targets = {
                _path_identity(target): dict(version, path=str(target))
                for _artifact, target, version in artifact_plans
            }
            final_family = set(stage["family_paths"])
            final_family.update(target for _artifact, target, _version in artifact_plans)
            for target in final_family:
                expected = expected_targets.get(
                    _path_identity(target), {"path": str(target), "exists": False}
                )
                if _processing_file_version(target) != expected:
                    raise RuntimeError(
                        f"Processing output family changed during publication: {target}"
                    )
        return receipt
    except Exception as err:
        if caller_owns_receipt:
            # The owner schedules recovery independently from operation cancel.
            raise
        with _processing_file_work_context(cancel_event=None, progress=None):
            rollback = _rollback_processing_output_promotion(receipt)
        if not rollback["complete"]:
            raise RuntimeError(
                f"{err}. Processing output rollback was incomplete, residual paths: "
                + ", ".join(rollback["residual_paths"])
            ) from err
        raise




def _rollback_processing_output_promotion(receipt: dict[str, Any]) -> dict[str, Any]:
    # Every attempt replaces the previous attempt's errors. A successful retry
    # must not be permanently poisoned by an old sharing violation.
    errors = []
    backups_by_target = {
        _path_identity(item["target"]): item for item in receipt.get("backups", [])
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
            if current.get("exists"):
                _remove_processing_path(path)
        except Exception as err:
            errors.append(f"remove {path}: {err}")
    for item in reversed(receipt.get("backups", [])):
        backup, target = item["backup"], item["target"]
        original_signature = _processing_version_signature(item["original_version"])
        try:
            target_version = _processing_file_version(target)
            backup_version = _processing_file_version(backup)
            if _processing_version_signature(target_version) == original_signature:
                if backup_version.get("exists"):
                    if _processing_version_signature(backup_version) != original_signature:
                        raise RuntimeError("backup content changed and was preserved")
                    backup.unlink()
                continue
            if target_version.get("exists"):
                errors.append(
                    f"retain concurrent target {target}: original backup remains at {backup}"
                )
                continue
            if _processing_version_signature(backup_version) != original_signature:
                raise RuntimeError("backup is missing or no longer matches its original snapshot")
            _link_processing_file_no_clobber(backup, target)
            # Verify before dropping the last private name of the backup.
            if _processing_version_signature(_processing_file_version(target)) != original_signature:
                raise RuntimeError("restored content does not match its backup snapshot")
            backup.unlink()
        except Exception as err:
            errors.append(f"restore {target}: {err}")
    for root in receipt.get("backup_roots", []):
        try:
            if root.exists():
                # Never recursively remove an unexpected file from a backup
                # directory while claiming that all original data was restored.
                root.rmdir()
        except Exception as err:
            errors.append(f"remove backup directory {root}: {err}")
    residual_paths = _processing_receipt_residual_paths(receipt)
    errors.extend(receipt.get("residual_errors", []))
    receipt["rollback_errors"] = errors
    receipt["residual_paths"] = residual_paths
    receipt["rollback_complete"] = not errors and not residual_paths
    return {
        "complete": receipt["rollback_complete"],
        "errors": list(errors),
        "residual_paths": list(residual_paths),
    }




def _commit_processing_output_promotion(receipt: dict[str, Any]) -> list[str]:
    """Discard only known, unchanged backups after successful publication."""

    residuals = set()
    errors = []
    for item in receipt.get("backups", []):
        backup = item["backup"]
        try:
            version = _processing_file_version(backup)
            if not version.get("exists"):
                continue
            if _processing_version_signature(version) != _processing_version_signature(item["original_version"]):
                raise RuntimeError("backup content changed and was preserved")
            backup.unlink()
        except Exception as err:
            errors.append(f"remove committed backup {backup}: {err}")
            residuals.add(str(backup))
    for root in receipt.get("backup_roots", []):
        try:
            if root.exists():
                root.rmdir()
        except Exception as err:
            errors.append(f"remove committed backup directory {root}: {err}")
            residuals.add(str(root))
    receipt["commit_errors"] = errors
    return sorted(residuals)




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
    """Keep auditing other paths even when one target cannot be fingerprinted."""

    errors = []
    residuals = set()
    backups_by_target = {
        _path_identity(item["target"]): item for item in receipt.get("backups", [])
    }

    def include_if_present(path):
        try:
            path.lstat()
        except FileNotFoundError:
            return
        except Exception as err:
            errors.append(f"inspect residual {path}: {err}")
        residuals.add(str(path))

    for path in receipt.get("published", []):
        if _path_identity(path) not in backups_by_target:
            include_if_present(path)
    for item in backups_by_target.values():
        target = item["target"]
        try:
            current = _processing_file_version(target)
            restored = _processing_version_signature(current) == (
                _processing_version_signature(item["original_version"])
            )
            if not restored:
                # A missing original is an unresolved target too. Do not hide
                # it merely because Path.exists() returns False.
                residuals.add(str(target))
        except Exception as err:
            errors.append(f"inspect restored target {target}: {err}")
            residuals.add(str(target))
        include_if_present(item["backup"])
    for root in receipt.get("backup_roots", []):
        include_if_present(root)
    receipt["residual_errors"] = errors
    return sorted(residuals)




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
        or runtime.cleanup_file_errors
        or runtime.output_stages
        or runtime.output_promotion_receipt is not None
        or runtime.file_task is not None
        or runtime.scheduling_algorithm
    )
    return {
        "complete": complete,
        "residual_paths": list(runtime.cleanup_residual_paths),
        "residual_layer_ids": list(runtime.cleanup_residual_layer_ids),
        "errors": list(dict.fromkeys(runtime.cleanup_errors + runtime.cleanup_file_errors)),
        "retry_recommended": not complete,
    }


def _processing_runtime_requires_retention(runtime: Any) -> bool:
    """Keep complete recovery ownership until all tasks and cleanup finish."""
    return isinstance(runtime, _ProcessingRuntime) and bool(
        runtime.task is not None
        or runtime.file_task is not None
        or runtime.file_callbacks
        or runtime.pending_callbacks
        or runtime.scheduling_algorithm
        or not _processing_cleanup_receipt(runtime)["complete"]
    )


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
        "operation",
        "phase",
        "phase_progress",
        "algorithm_executed",
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


def _processing_raster_band_counts(templates):
    counts = {}
    for item in templates:
        if not item.get("_pending_file"):
            continue
        path = Path(item["path"])
        if path.suffix.lower() not in {".tif", ".tiff", ".vrt"} or not path.is_file():
            continue
        # Constructing a layer also creates a default renderer, which can
        # calculate statistics and write PAM caches before the input snapshot.
        # A provider-only metadata read has no renderer or project ownership.
        from qgis.core import QgsDataProvider, QgsProviderRegistry
        provider = QgsProviderRegistry.instance().createProvider(
            "gdal", str(path), QgsDataProvider.ProviderOptions())
        if provider is not None and provider.isValid():
            counts[_path_identity(path)] = int(provider.bandCount())
    return counts


def _processing_input_file_version(path, band_counts):
    version = _processing_file_version(path)
    if str(path).lower().endswith(".aux.xml"):
        primary = Path(str(path)[:-8])
        cache = capture_input_cache(path, band_count=band_counts.get(_path_identity(primary)))
        if cache is not None:
            # A second read must describe the exact version just fingerprinted.
            same = not version.get("exists") and not cache.get("exists")
            if version.get("exists") and cache.get("exists"):
                signature = cache.get("file_signature") or {}
                same = (cache.get("file_sha256") == version.get("sha256")
                        and signature.get("size") == version.get("size")
                        and signature.get("modified_ns") == version.get("modified_ns"))
            if same:
                version["input_cache"] = cache
    return version


def _processing_exact_file_version(version):
    return {key: value for key, value in version.items() if key != "input_cache"}


def _processing_known_recovery_paths(runtime):
    """Conservative paths requiring verification, without doing I/O or hashing."""
    paths = list(runtime.cleanup_residual_paths)
    paths.extend(str(stage["staging_root"]) for stage in runtime.output_stages)
    receipt = runtime.output_promotion_receipt or {}
    paths.extend(str(path) for path in receipt.get("published", []))
    paths.extend(str(path) for path in receipt.get("backup_roots", []))
    for item in receipt.get("backups", []):
        paths.extend((str(item["backup"]), str(item["target"])))
    return sorted(set(paths))


def _processing_file_preflight(runtime):
    inputs = [
        _processing_input_file_version(Path(item["path"]), runtime.band_counts)
        if item.get("_pending_file") else copy.deepcopy(item)
        for item in runtime.input_templates
    ]
    input_paths = {_path_identity(Path(v["path"])) for v in inputs if v.get("kind", "file") == "file"}
    output_paths = set()
    outputs = []
    for plan in runtime.output_plans:
        for path in plan["family_paths"]:
            identity = _path_identity(path)
            if identity in input_paths:
                raise RuntimeError(f"Processing input and output paths must be different: {path}")
            if identity in output_paths:
                raise RuntimeError(f"Processing output parameters resolve to the same file family: {path}")
            output_paths.add(identity)
            outputs.append({"parameter": plan["parameter"], **_processing_file_version(path)})
    collisions = [v for v in outputs if v["exists"]]
    approval = {"inputs": inputs, "outputs": outputs}
    if collisions and any(v.get("kind") == "project_layer" and not v.get("immutable_file_snapshot", False) for v in inputs):
        raise RuntimeError("Confirmed Processing overwrites require immutable local file snapshots for every QGIS project layer input")
    if runtime.operation == "preview":
        return {"input_versions": inputs, "collisions": collisions, "approval_snapshot": approval}
    if runtime.approved_snapshot is not None:
        if approval != runtime.approved_snapshot:
            raise RuntimeError("Processing inputs or overwrite targets changed after preview. Request a new confirmation token.")
    elif collisions:
        raise FileExistsError("Processing output destination already exists. Request an exact overwrite preview before approving these targets: " + ", ".join(v["path"] for v in collisions))
    parameters = dict(runtime.parameters)
    stages = runtime.output_stages
    for plan in runtime.output_plans:
        check_file_cancelled()
        target = plan["target"]
        target.parent.mkdir(parents=True, exist_ok=True)
        root = Path(tempfile.mkdtemp(prefix=f".{target.stem}.qcopilots-stage-", dir=str(target.parent)))
        stage = {**plan, "staging_root": root, "staged_target": root / target.name,
                 "authorized_versions": [{k: v for k, v in item.items() if k != "parameter"}
                                         for item in outputs if item["parameter"] == plan["parameter"]]}
        stages.append(stage)
        parameters[plan["parameter"]] = _replace_processing_output_path(parameters[plan["parameter"]], stage["staged_target"])
    return {"input_versions": inputs, "collisions": collisions, "parameters": parameters, "stages": stages}


def _processing_verify_input_files(runtime):
    expected_files = [v for v in runtime.authorized_input_versions if v.get("kind", "file") == "file"]
    current_files = {_path_identity(Path(v["path"])): _processing_input_file_version(Path(v["path"]), runtime.band_counts) for v in expected_files}
    expected_by_path = {_path_identity(Path(v["path"])): v for v in expected_files}
    changed = []
    for expected in expected_files:
        path = Path(expected["path"])
        current = current_files[_path_identity(path)]
        if _processing_exact_file_version(current) == _processing_exact_file_version(expected):
            continue
        accepted = None
        if str(path).lower().endswith(".aux.xml"):
            primary_key = _path_identity(Path(str(path)[:-8]))
            before_primary = expected_by_path.get(primary_key)
            now_primary = current_files.get(primary_key)
            if before_primary is not None and now_primary == before_primary:
                accepted = compare_input_cache(expected.get("input_cache"), current.get("input_cache"))
        if accepted is None:
            changed.append(str(path))
        else:
            runtime.cache_changes.extend(accepted)
    if changed:
        raise RuntimeError("Processing input changed while the job was running: " + ", ".join(changed))


def _processing_verify_and_publish(runtime):
    _processing_verify_input_files(runtime)
    _validate_processing_stage_targets(runtime.output_stages)
    check_file_cancelled()
    if runtime.output_stages:
        runtime.phase = "publish"
        _promote_processing_outputs(runtime.output_stages, overwrite=runtime.overwrite_outputs,
                                    receipt=runtime.output_promotion_receipt)
        runtime.output_promotion_state = "promoted"


def _new_processing_output_promotion_receipt() -> dict[str, Any]:
    """Create the receipt before the first mutation of a public output."""

    return {
        "published": [],
        "published_versions": [],
        "backups": [],
        "backup_roots": [],
        "rollback_complete": None,
        "rollback_errors": [],
        "residual_errors": [],
        "residual_paths": [],
    }




def _cleanup_processing_runtime_file_outputs(
    runtime: _ProcessingRuntime,
    *,
    terminal_state: str | None = None,
) -> None:
    """File-only recovery, called after project-layer cleanup on its owner thread."""

    cleanup_residuals = []
    file_errors = []
    if runtime.output_promotion_receipt is not None:
        successful_publication = (
            runtime.output_promotion_state == "published_success"
            or terminal_state == "succeeded"
        )
        if successful_publication:
            runtime.output_promotion_state = "published_success"
            cleanup_residuals.extend(_commit_processing_output_promotion(runtime.output_promotion_receipt))
            file_errors.extend(runtime.output_promotion_receipt.get("commit_errors", []))
            if not cleanup_residuals and not file_errors:
                runtime.output_promotion_receipt = None
                runtime.output_promotion_state = "none"
        else:
            rollback = _rollback_processing_output_promotion(runtime.output_promotion_receipt)
            cleanup_residuals.extend(rollback["residual_paths"])
            file_errors.extend(rollback["errors"])
            if rollback["complete"]:
                runtime.output_promotion_receipt = None
                runtime.output_promotion_state = "none"
    residuals = _cleanup_processing_stages(runtime.output_stages)
    cleanup_residuals.extend(residuals)
    if not residuals:
        runtime.output_stages.clear()
    else:
        file_errors.extend(f"remove Processing staging directory {path}: cleanup incomplete" for path in residuals)
    previous_file_errors = set(getattr(runtime, "cleanup_file_errors", []))
    runtime.cleanup_errors = [error for error in runtime.cleanup_errors if error not in previous_file_errors]
    runtime.cleanup_file_errors = list(file_errors)
    runtime.cleanup_errors.extend(file_errors)
    runtime.cleanup_residual_paths = sorted(set(cleanup_residuals))
