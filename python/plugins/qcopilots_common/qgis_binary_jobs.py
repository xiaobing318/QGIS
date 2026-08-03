"""Asynchronous, catalog-backed QGIS binary execution jobs.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import hashlib
import json
import locale
import math
import os
import re
import signal
import subprocess
import threading
import time
import uuid
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath, PureWindowsPath
from typing import Any

from qcopilots_common.async_jobs import (
    AsyncJobStore,
    PUBLIC_JOB_STATES,
    TERMINAL_JOB_STATES,
    utc_now_rfc3339,
)


QGIS_BINARY_CATEGORY = "qgis_binary"
MAX_BINARY_STDIN_BYTES = 1024 * 1024
MAX_BINARY_OUTPUT_BYTES = 16 * 1024 * 1024
MAX_BINARY_TIMEOUT_SECONDS = 24 * 60 * 60
DEFAULT_LIST_LIMIT = 100
MAX_LIST_LIMIT = 200


class BinaryCatalogError(RuntimeError):
    """Raised when the explicit QGIS binary catalog is invalid or stale."""


class BinaryExecutionError(RuntimeError):
    """Raised when a configured binary cannot safely be scheduled."""


class _ByteRingBuffer:
    def __init__(self, capacity: int):
        self.capacity = max(1, int(capacity))
        self._data = bytearray()
        self.total_bytes = 0
        self.truncated = False
        self._lock = threading.Lock()

    def append(self, value: bytes) -> None:
        if not value:
            return
        with self._lock:
            self.total_bytes += len(value)
            if len(value) >= self.capacity:
                self._data = bytearray(value[-self.capacity :])
                self.truncated = self.total_bytes > self.capacity
                return
            overflow = len(self._data) + len(value) - self.capacity
            if overflow > 0:
                del self._data[:overflow]
                self.truncated = True
            self._data.extend(value)

    def snapshot(self) -> tuple[bytes, bool, int]:
        with self._lock:
            return bytes(self._data), self.truncated, self.total_bytes


class _ProgressParser:
    def __init__(self, config: Any):
        self._config = _normalize_progress_parser(config)
        self._tail = ""
        self._lock = threading.Lock()
        self._regex = None
        if self._config and self._config["type"] == "percent_regex":
            self._regex = re.compile(self._config["pattern"])

    def feed(self, value: bytes) -> float | None:
        if not self._config:
            return None
        with self._lock:
            text = value.decode("utf-8", errors="replace")
            self._tail = (self._tail + text)[-8192:]
            if self._config["type"] == "gdal_dotted":
                matches = re.findall(
                    r"(?<!\d)(100|[0-9]{1,2})"
                    r"(?=\s*(?:\.\.\.|%|-\s*done\b))",
                    self._tail,
                    flags=re.IGNORECASE,
                )
                return float(matches[-1]) if matches else None
            matches = list(self._regex.finditer(self._tail)) if self._regex else []
            if not matches:
                return None
            match = matches[-1]
            try:
                if "percent" in match.re.groupindex:
                    value_text = match.group("percent")
                elif match.lastindex:
                    value_text = match.group(1)
                else:
                    value_text = match.group(0)
                progress = float(value_text)
            except (TypeError, ValueError):
                return None
            if not math.isfinite(progress):
                return None
            return min(100.0, max(0.0, progress))


@dataclass
class _BinaryOutcome:
    exit_code: int | None = None
    timed_out: bool = False
    cancelled: bool = False
    process_alive: bool = False
    start_error: str | None = None
    environment_error: str | None = None
    termination_error: str | None = None
    started_monotonic: float | None = None
    finished_monotonic: float | None = None


class BinarySubprocessExecution:
    """Runs one configured executable without a shell in a worker thread."""

    def __init__(
        self,
        *,
        executable: str,
        arguments: list[str],
        working_directory: str,
        environment_loader,
        stdin_bytes: bytes | None,
        timeout_seconds: float,
        success_exit_codes: list[int],
        max_output_bytes: int,
        progress_parser: Any,
        progress_callback=None,
        popen_factory=None,
        run_factory=None,
        monotonic=None,
        sleep=None,
        os_name: str | None = None,
        comspec: str | None = None,
        taskkill_executable: str | None = None,
        graceful_cancel_seconds: float = 1.0,
        force_kill_seconds: float = 5.0,
    ):
        self.executable = executable
        self.arguments = list(arguments)
        self.working_directory = working_directory
        self.environment_loader = environment_loader
        self.stdin_bytes = stdin_bytes
        self.timeout_seconds = float(timeout_seconds)
        self.success_exit_codes = frozenset(int(code) for code in success_exit_codes)
        self.stdout = _ByteRingBuffer(max_output_bytes)
        self.stderr = _ByteRingBuffer(max_output_bytes)
        self._progress_parser = _ProgressParser(progress_parser)
        self._progress_callback = progress_callback
        self._popen_factory = popen_factory or subprocess.Popen
        self._run_factory = run_factory or subprocess.run
        self._monotonic = monotonic or time.monotonic
        self._sleep = sleep or time.sleep
        self._os_name = os_name or os.name
        self._comspec = comspec
        self._taskkill_executable = taskkill_executable or (
            _trusted_windows_executable("taskkill.exe")
            if self._os_name == "nt"
            else "taskkill.exe"
        )
        self._graceful_cancel_seconds = max(0.0, float(graceful_cancel_seconds))
        self._force_kill_seconds = max(0.1, float(force_kill_seconds))
        self._cancel_event = threading.Event()
        self._process_lock = threading.RLock()
        self._process = None
        self._outcome_lock = threading.RLock()
        self._outcome = _BinaryOutcome()

    def cancel(self) -> None:
        with self._process_lock:
            self._cancel_event.set()

    def is_cancelled(self) -> bool:
        return self._cancel_event.is_set()

    def run(self) -> bool:
        with self._process_lock:
            if self._finish_cancelled_before_start():
                return False
        try:
            environment = self.environment_loader()
        except Exception as err:
            with self._outcome_lock:
                self._outcome.environment_error = str(err)
                self._outcome.finished_monotonic = self._monotonic()
            return False

        command = [self.executable, *self.arguments]
        kwargs: dict[str, Any] = {
            "cwd": self.working_directory,
            "env": environment,
            "stdin": subprocess.PIPE if self.stdin_bytes is not None else subprocess.DEVNULL,
            "stdout": subprocess.PIPE,
            "stderr": subprocess.PIPE,
            "shell": False,
            "bufsize": 0,
        }
        if self._os_name == "nt":
            kwargs["creationflags"] = (
                subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NO_WINDOW
            )
        with self._process_lock:
            if self._finish_cancelled_before_start():
                return False
            try:
                process = self._popen_factory(command, **kwargs)
            except Exception as err:
                with self._outcome_lock:
                    self._outcome.start_error = str(err)
                    self._outcome.finished_monotonic = self._monotonic()
                return False
            self._process = process
            started = self._monotonic()
            with self._outcome_lock:
                self._outcome.started_monotonic = started
                self._outcome.process_alive = True

        readers = [
            threading.Thread(
                target=self._read_pipe,
                args=(process.stdout, self.stdout),
                daemon=True,
                name="QCopilotsBinaryStdout",
            ),
            threading.Thread(
                target=self._read_pipe,
                args=(process.stderr, self.stderr),
                daemon=True,
                name="QCopilotsBinaryStderr",
            ),
        ]
        for reader in readers:
            reader.start()

        writer = None
        if self.stdin_bytes is not None:
            writer = threading.Thread(
                target=self._write_stdin,
                args=(process.stdin, self.stdin_bytes),
                daemon=True,
                name="QCopilotsBinaryStdin",
            )
            writer.start()

        timed_out = False
        cancelled = False
        termination_error = None
        while process.poll() is None:
            if self._cancel_event.is_set():
                cancelled = True
                termination_error = self._terminate_process_tree(process)
                break
            if self._monotonic() - started >= self.timeout_seconds:
                timed_out = True
                termination_error = self._terminate_process_tree(process)
                break
            self._sleep(0.05)

        try:
            exit_code = process.wait(timeout=self._force_kill_seconds)
        except Exception:
            exit_code = process.poll()
        if writer:
            writer.join(timeout=0.5)
        for reader in readers:
            reader.join(timeout=1.0)

        finished = self._monotonic()
        process_alive = process.poll() is None
        with self._process_lock:
            self._process = process if process_alive else None
        with self._outcome_lock:
            self._outcome.exit_code = exit_code
            self._outcome.timed_out = timed_out
            self._outcome.cancelled = (
                cancelled and termination_error is None and not process_alive
            )
            self._outcome.process_alive = process_alive
            self._outcome.termination_error = termination_error
            self._outcome.finished_monotonic = finished
        return (
            not timed_out
            and not self._outcome.cancelled
            and not process_alive
            and exit_code in self.success_exit_codes
        )

    def _finish_cancelled_before_start(self) -> bool:
        if not self._cancel_event.is_set():
            return False
        with self._outcome_lock:
            self._outcome.cancelled = True
            self._outcome.process_alive = False
            self._outcome.finished_monotonic = self._monotonic()
        return True

    def outcome_snapshot(self) -> dict[str, Any]:
        with self._outcome_lock:
            outcome = _BinaryOutcome(**vars(self._outcome))
        stdout, stdout_truncated, stdout_total = self.stdout.snapshot()
        stderr, stderr_truncated, stderr_total = self.stderr.snapshot()
        duration = None
        if (
            outcome.started_monotonic is not None
            and outcome.finished_monotonic is not None
        ):
            duration = max(0.0, outcome.finished_monotonic - outcome.started_monotonic)
        encoding = locale.getpreferredencoding(False) or "utf-8"
        return {
            "exit_code": outcome.exit_code,
            "stdout": stdout.decode(encoding, errors="replace"),
            "stderr": stderr.decode(encoding, errors="replace"),
            "stdout_truncated": stdout_truncated,
            "stderr_truncated": stderr_truncated,
            "stdout_total_bytes": stdout_total,
            "stderr_total_bytes": stderr_total,
            "duration_seconds": duration,
            "timed_out": outcome.timed_out,
            "cancelled": outcome.cancelled,
            "cancel_requested": self._cancel_event.is_set(),
            "process_alive": outcome.process_alive,
            "start_error": outcome.start_error,
            "environment_error": outcome.environment_error,
            "termination_error": outcome.termination_error,
        }

    def _read_pipe(self, pipe, buffer: _ByteRingBuffer) -> None:
        if pipe is None:
            return
        try:
            while True:
                chunk = pipe.read(8192)
                if not chunk:
                    break
                if isinstance(chunk, str):
                    chunk = chunk.encode("utf-8", errors="replace")
                buffer.append(chunk)
                progress = self._progress_parser.feed(chunk)
                if progress is not None and self._progress_callback:
                    self._progress_callback(progress)
        except Exception:
            return
        finally:
            try:
                pipe.close()
            except Exception:
                pass

    @staticmethod
    def _write_stdin(pipe, value: bytes) -> None:
        if pipe is None:
            return
        try:
            pipe.write(value)
            pipe.flush()
        except Exception:
            pass
        finally:
            try:
                pipe.close()
            except Exception:
                pass

    def _terminate_process_tree(self, process) -> str | None:
        errors = []
        if self._os_name == "nt":
            try:
                process.send_signal(signal.CTRL_BREAK_EVENT)
                process.wait(timeout=self._graceful_cancel_seconds)
                return None
            except Exception as err:
                errors.append(str(err))
            if process.poll() is None:
                try:
                    completed = self._run_factory(
                        [
                            self._taskkill_executable,
                            "/PID",
                            str(process.pid),
                            "/T",
                            "/F",
                        ],
                        shell=False,
                        stdin=subprocess.DEVNULL,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                        timeout=self._force_kill_seconds,
                        creationflags=subprocess.CREATE_NO_WINDOW,
                        check=False,
                    )
                    if getattr(completed, "returncode", 1) == 0:
                        try:
                            process.wait(timeout=self._force_kill_seconds)
                        except Exception:
                            pass
                        if process.poll() is not None:
                            return None
                    errors.append(
                        "taskkill.exe did not terminate the process tree"
                    )
                except Exception as err:
                    errors.append(str(err))
            if process.poll() is None:
                try:
                    process.kill()
                    process.wait(timeout=self._force_kill_seconds)
                    if process.poll() is not None:
                        return None
                except Exception as err:
                    errors.append(str(err))
        else:
            try:
                process.terminate()
                process.wait(timeout=self._graceful_cancel_seconds)
                return None
            except Exception as err:
                errors.append(str(err))
            if process.poll() is None:
                try:
                    process.kill()
                    process.wait(timeout=self._force_kill_seconds)
                    if process.poll() is not None:
                        return None
                except Exception as err:
                    errors.append(str(err))
        return " | ".join(error for error in errors if error) or None


try:
    from qgis.core import QgsTask as _QgsTask
except Exception:
    _QgsTask = None


if _QgsTask is not None:

    class QGISBinaryTask(_QgsTask):
        """QgsTask wrapper around :class:`BinarySubprocessExecution`."""

        def __init__(self, description: str, execution: BinarySubprocessExecution):
            flags = _QgsTask.Flag.CanCancel | _QgsTask.Flag.CancelWithoutPrompt
            super().__init__(description, flags)
            self.execution = execution

        def run(self) -> bool:
            return self.execution.run()

        def cancel(self) -> None:
            self.execution.cancel()
            super().cancel()

        def outcome_snapshot(self) -> dict[str, Any]:
            return self.execution.outcome_snapshot()

else:

    class QGISBinaryTask:
        """Import-safe fallback used by subprocess-only unit tests."""

        def __init__(self, description: str, execution: BinarySubprocessExecution):
            self.description = description
            self.execution = execution

        def run(self) -> bool:
            return self.execution.run()

        def cancel(self) -> None:
            self.execution.cancel()

        def isCanceled(self) -> bool:
            return self.execution.is_cancelled()

        def outcome_snapshot(self) -> dict[str, Any]:
            return self.execution.outcome_snapshot()


@dataclass
class _BinaryRuntime:
    binary: dict[str, Any]
    task: Any
    generation: int
    task_id: int | None = None
    lock: threading.RLock = field(default_factory=threading.RLock)

    def release_task(self) -> None:
        with self.lock:
            self.task = None


class QGISBinaryJobManager:
    """Catalog, task, and job owner for QGIS binary MCP tools."""

    def __init__(
        self,
        iface: Any = None,
        catalog_path: str | Path | None = None,
        package_root: str | Path | None = None,
        store: AsyncJobStore | None = None,
        dependencies: dict[str, Any] | None = None,
    ):
        self.iface = iface
        self._dependencies = dict(dependencies or {})
        self._lock = threading.RLock()
        self._environment_lock = threading.RLock()
        self._environment_cache: dict[str, dict[str, str]] = {}
        self._accepting = True
        self._generation = 0
        self.store = store or AsyncJobStore(scope_field="category")
        self.catalog_path = Path(catalog_path) if catalog_path else _default_catalog_path()
        self._raw_catalog = self._load_catalog_document(self.catalog_path)
        _validate_catalog_header(self._raw_catalog)
        self.package_root = _resolve_package_root(
            package_root,
            self._raw_catalog,
            self._dependencies,
        )
        self._catalog, self._binaries = _validate_catalog(
            self._raw_catalog,
            self.package_root,
        )

    @property
    def accepting(self) -> bool:
        with self._lock:
            return self._accepting and self.store.accepting

    def load_catalog(self) -> dict[str, Any]:
        return _public_catalog(self._catalog)

    def list_binaries(self, arguments: dict[str, Any]) -> dict[str, Any]:
        cursor = arguments.get("cursor", "0")
        if cursor in (None, ""):
            cursor = "0"
        if (
            not isinstance(cursor, str)
            or len(cursor) > 2048
            or not cursor.isdecimal()
        ):
            raise ValueError("cursor must be a non-negative decimal string")
        offset = int(cursor)
        limit = arguments.get("limit", DEFAULT_LIST_LIMIT)
        if (
            isinstance(limit, bool)
            or not isinstance(limit, int)
            or not 1 <= limit <= MAX_LIST_LIMIT
        ):
            raise ValueError("limit must be an integer from 1 through 200")
        query_value = arguments.get("query")
        if query_value is not None and not isinstance(query_value, str):
            raise ValueError("query must be a string")
        if isinstance(query_value, str) and len(query_value) > 256:
            raise ValueError("query must contain at most 256 characters")
        query = (query_value or "").strip().casefold()
        group = arguments.get("group")
        risk = arguments.get("risk")
        enabled = arguments.get("enabled")
        if enabled is not None and not isinstance(enabled, bool):
            raise ValueError("enabled must be a boolean")
        if group is not None:
            group = _required_string(group, "group")
            if len(group) > 128:
                raise ValueError("group must contain at most 128 characters")
        if risk is not None:
            risk = _normalize_risk(risk)

        matches = []
        for binary in self._binaries.values():
            if enabled is not None and binary["enabled"] is not enabled:
                continue
            if group is not None and binary["group"] != group:
                continue
            if risk is not None and binary["risk"] != risk:
                continue
            if query and query not in " ".join(
                str(binary.get(key, ""))
                for key in ("id", "name", "description", "path", "group")
            ).casefold():
                continue
            matches.append(binary)
        matches.sort(key=lambda item: item["id"].casefold())
        page = matches[offset : offset + limit]
        next_offset = offset + len(page)
        return {
            "binaries": [_binary_summary(item) for item in page],
            "next_cursor": str(next_offset) if next_offset < len(matches) else None,
            "total": len(matches),
        }

    def get_binary_details(self, arguments: dict[str, Any] | str) -> dict[str, Any]:
        binary_id = (
            _required_string(arguments.get("binary_id"), "binary_id")
            if isinstance(arguments, dict)
            else _required_string(arguments, "binary_id")
        )
        binary = self._binaries.get(binary_id)
        if binary is None:
            raise RuntimeError(f"qgis_binary_not_found: {binary_id}")
        return _public_binary(binary)

    def start(self, arguments: dict[str, Any]) -> dict[str, Any]:
        with self._lock:
            if not self._accepting:
                raise RuntimeError("QCopilots QGIS binary job manager is stopping")
            generation = self._generation
        binary_id = _required_string(arguments.get("binary_id"), "binary_id")
        argv = arguments.get("arguments", [])
        if argv is None:
            argv = []
        if not isinstance(argv, list) or any(not isinstance(item, str) for item in argv):
            raise ValueError("arguments must be an array of strings")
        if len(argv) > 256 or any(len(item) > 32768 for item in argv):
            raise ValueError("arguments exceed the configured command line limits")
        working_directory_value = arguments.get("working_directory")
        stdin_value = arguments.get("stdin")
        if stdin_value is not None and not isinstance(stdin_value, str):
            raise ValueError("stdin must be a string")
        timeout_value = arguments.get("timeout_seconds")
        confirmed_risk = arguments.get("confirmed_risk", False)
        if not isinstance(confirmed_risk, bool):
            raise ValueError("confirmed_risk must be a boolean")
        client_request_id = _client_request_id(arguments.get("client_request_id"))

        fingerprint = _fingerprint(
            {
                "binary_id": binary_id,
                "arguments": argv,
                "working_directory": working_directory_value,
                "stdin": stdin_value,
                "timeout_seconds": timeout_value,
                "confirmed_risk": confirmed_risk,
            }
        )
        existing = self.store.lookup_idempotent(
            QGIS_BINARY_CATEGORY, client_request_id, fingerprint
        )
        if existing is not None:
            return existing

        binary = self._binaries.get(binary_id)
        if binary is None:
            raise RuntimeError(f"qgis_binary_not_found: {binary_id}")
        if binary["risk"] == "R3":
            raise BinaryExecutionError("qgis_binary_risk_blocked: R3 binaries cannot run")
        if not binary["enabled"]:
            reason = binary.get("disabled_reason") or "disabled by catalog policy"
            raise BinaryExecutionError(f"qgis_binary_disabled: {reason}")
        if binary["risk"] == "R2" and not confirmed_risk:
            raise BinaryExecutionError(
                "qgis_binary_risk_confirmation_required: confirmed_risk must be true"
            )

        executable = _resolve_catalog_executable(self.package_root, binary["path"])
        working_directory = _resolve_working_directory(
            working_directory_value, self.package_root
        )
        stdin_bytes = stdin_value.encode("utf-8") if stdin_value is not None else None
        if stdin_bytes is not None and len(stdin_bytes) > binary["max_stdin_bytes"]:
            raise ValueError(
                f"stdin exceeds the {binary['max_stdin_bytes']} byte catalog limit"
            )
        timeout_seconds = _bounded_timeout(timeout_value, binary["timeout_seconds"])
        job_id = str(uuid.uuid4())
        execution = self._create_execution(
            job_id=job_id,
            binary=binary,
            executable=str(executable),
            arguments=argv,
            working_directory=str(working_directory),
            stdin_bytes=stdin_bytes,
            timeout_seconds=timeout_seconds,
        )
        runtime = _BinaryRuntime(binary=binary, task=None, generation=generation)
        initial = {
            "job_id": job_id,
            "category": QGIS_BINARY_CATEGORY,
            "client_request_id": client_request_id,
            "state": "queued",
            "execution_mode": "qgs_task",
            "cancellable": True,
            "binary": _binary_summary(binary),
            "progress_percent": None,
            "progress_text": "",
            "processed_count": None,
            "cancel_requested": False,
            "created_at": utc_now_rfc3339(),
            "started_at": None,
            "finished_at": None,
            "error": None,
        }
        snapshot, created = self.store.create(
            QGIS_BINARY_CATEGORY,
            initial,
            client_request_id=client_request_id,
            request_fingerprint=fingerprint,
            runtime=runtime,
        )
        if not created:
            return snapshot
        with self._lock:
            current = self.store.get(job_id, QGIS_BINARY_CATEGORY)
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
                        QGIS_BINARY_CATEGORY,
                        cancel_requested=True,
                        finished_at=utc_now_rfc3339(),
                        progress_text="Cancelled before QGIS task scheduling",
                    )
                return current
            try:
                task = self._create_task(binary, execution)
                runtime.task = task
                task.begun.connect(lambda: self._on_begun(job_id))
                task.taskCompleted.connect(lambda: self._on_finished(job_id, True))
                task.taskTerminated.connect(lambda: self._on_finished(job_id, False))
                _connect_signal(
                    task,
                    "destroyed",
                    lambda *_: runtime.release_task(),
                )
                task_id = self._task_manager().addTask(task)
                if not task_id:
                    failed = self._finish_failed(
                        job_id,
                        "task_start_failed",
                        "schedule",
                        "QGIS task manager rejected the binary task",
                        {},
                    )
                    runtime.release_task()
                    return failed
                runtime.task_id = int(task_id)
            except Exception as err:
                failed = self._finish_failed(
                    job_id,
                    "task_start_failed",
                    "schedule",
                    str(err),
                    {},
                )
                runtime.release_task()
                return failed
        return self.store.get(job_id, QGIS_BINARY_CATEGORY)

    def get(self, arguments: dict[str, Any] | str) -> dict[str, Any]:
        job_id = (
            _required_string(arguments.get("job_id"), "job_id")
            if isinstance(arguments, dict)
            else _required_string(arguments, "job_id")
        )
        return self.store.get(job_id, QGIS_BINARY_CATEGORY)

    def list_jobs(self, arguments: dict[str, Any]) -> dict[str, Any]:
        states = arguments.get("states")
        if states is not None:
            if not isinstance(states, list) or not states:
                raise ValueError("states must be a non-empty array")
            if len({str(state) for state in states}) != len(states):
                raise ValueError("states must not contain duplicates")
            invalid = {str(state) for state in states} - PUBLIC_JOB_STATES
            if invalid:
                raise ValueError("Invalid public job states: " + ", ".join(sorted(invalid)))
        limit = arguments.get("limit", DEFAULT_LIST_LIMIT)
        if (
            isinstance(limit, bool)
            or not isinstance(limit, int)
            or not 1 <= limit <= MAX_LIST_LIMIT
        ):
            raise ValueError("limit must be an integer from 1 through 200")
        snapshots = self.store.list(
            QGIS_BINARY_CATEGORY, states=states, limit=limit
        )
        return {"jobs": [_binary_job_summary(snapshot) for snapshot in snapshots]}

    def cancel(self, arguments: dict[str, Any] | str) -> dict[str, Any]:
        job_id = (
            _required_string(arguments.get("job_id"), "job_id")
            if isinstance(arguments, dict)
            else _required_string(arguments, "job_id")
        )
        with self._lock:
            snapshot, runtime, requested = self.store.request_cancel(
                job_id, QGIS_BINARY_CATEGORY
            )
            if requested and runtime and runtime.task:
                try:
                    runtime.task.cancel()
                except Exception as err:
                    return self._finish_failed(
                        job_id,
                        "cancel_failed",
                        "cancel",
                        str(err),
                        {},
                    )
            return self.store.get(job_id, QGIS_BINARY_CATEGORY)

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
            if category != QGIS_BINARY_CATEGORY or runtime is None:
                continue
            try:
                self.store.request_cancel(job_id, category)
            except RuntimeError:
                self.store.patch(job_id, category, cancel_requested=True)
            task = runtime.task
            if task is None:
                self.store.transition(
                    job_id,
                    "cancelled",
                    category,
                    cancel_requested=True,
                    finished_at=utc_now_rfc3339(),
                )
                continue
            try:
                task.cancel()
            except Exception:
                pass
            tasks.append((job_id, task))
        deadline = time.monotonic() + timeout
        for job_id, task in tasks:
            remaining_ms = max(0, int((deadline - time.monotonic()) * 1000))
            wait = getattr(task, "waitForFinished", None)
            if callable(wait) and remaining_ms > 0:
                try:
                    wait(remaining_ms)
                except Exception:
                    pass
            self._settle_after_shutdown_wait(job_id, task)

    def _settle_after_shutdown_wait(self, job_id: str, task: Any) -> dict[str, Any]:
        current = self.store.get(job_id, QGIS_BINARY_CATEGORY)
        if current["state"] in TERMINAL_JOB_STATES:
            return current
        outcome_getter = getattr(task, "outcome_snapshot", None)
        if not callable(outcome_getter):
            return current
        try:
            outcome = outcome_getter()
        except Exception:
            return current
        if not isinstance(outcome, dict):
            return current
        if outcome.get("process_alive"):
            if outcome.get("termination_error"):
                return self._finish_failed(
                    job_id,
                    "process_termination_failed",
                    "cancel",
                    "QGIS binary process tree could not be terminated",
                    outcome,
                    cancel_requested=True,
                )
            return current
        has_process_outcome = any(
            (
                outcome.get("exit_code") is not None,
                bool(outcome.get("environment_error")),
                bool(outcome.get("start_error")),
                bool(outcome.get("timed_out")),
                bool(outcome.get("termination_error")),
            )
        )
        cancel_requested = bool(
            current.get("cancel_requested") or outcome.get("cancel_requested")
        )
        if outcome.get("cancelled") or (
            cancel_requested and not has_process_outcome
        ):
            return self.store.transition(
                job_id,
                "cancelled",
                QGIS_BINARY_CATEGORY,
                cancel_requested=True,
                finished_at=utc_now_rfc3339(),
                progress_text="Cancelled while QGIS bridge was stopping",
            )
        if has_process_outcome:
            self._on_finished(job_id, False, outcome)
        return self.store.get(job_id, QGIS_BINARY_CATEGORY)

    def _create_execution(self, **values) -> BinarySubprocessExecution:
        dependency = self._dependencies.get("execution_factory")
        if dependency:
            return dependency(**values)
        binary = values.pop("binary")
        job_id = values.pop("job_id")
        return BinarySubprocessExecution(
            **values,
            environment_loader=lambda: self._environment_for(binary["environment"]),
            success_exit_codes=binary["success_exit_codes"],
            max_output_bytes=binary["max_output_bytes"],
            progress_parser=binary["progress_parser"],
            progress_callback=lambda progress: self._on_progress(job_id, progress),
            popen_factory=self._dependencies.get("popen_factory"),
            run_factory=self._dependencies.get("run_factory"),
            monotonic=self._dependencies.get("monotonic"),
            sleep=self._dependencies.get("sleep"),
            os_name=self._dependencies.get("os_name"),
            comspec=self._dependencies.get("comspec"),
            taskkill_executable=self._dependencies.get("taskkill_executable"),
        )

    def _create_task(self, binary: dict[str, Any], execution):
        factory = self._dependencies.get("task_factory")
        description = f"QCopilots: {binary.get('name') or binary['id']}"
        if factory:
            return factory(
                description=description,
                execution=execution,
                binary=binary,
            )
        if _QgsTask is None:
            raise RuntimeError("QGIS task support is unavailable")
        return QGISBinaryTask(description, execution)

    def _task_manager(self):
        dependency = self._dependencies.get("task_manager")
        if dependency is not None:
            return dependency() if callable(dependency) else dependency
        from qgis.core import QgsApplication

        return QgsApplication.taskManager()

    def _on_begun(self, job_id: str) -> None:
        try:
            snapshot = self.store.get(job_id, QGIS_BINARY_CATEGORY)
        except RuntimeError:
            return
        if snapshot["state"] != "queued":
            return
        self.store.transition(
            job_id,
            "running",
            QGIS_BINARY_CATEGORY,
            started_at=utc_now_rfc3339(),
            progress_text=f"Running {snapshot['binary']['id']}",
        )

    def _on_progress(self, job_id: str, progress: float) -> None:
        try:
            self.store.patch(
                job_id,
                QGIS_BINARY_CATEGORY,
                progress_percent=progress,
                progress_text=f"{progress:g}%",
            )
            runtime = self.store.get_runtime(job_id, QGIS_BINARY_CATEGORY)
            if runtime and runtime.task and hasattr(runtime.task, "setProgress"):
                runtime.task.setProgress(progress)
        except RuntimeError:
            pass

    def _on_finished(
        self,
        job_id: str,
        _task_success: bool,
        outcome_override: dict[str, Any] | None = None,
    ) -> None:
        try:
            runtime = self.store.get_runtime(job_id, QGIS_BINARY_CATEGORY)
            snapshot = self.store.get(job_id, QGIS_BINARY_CATEGORY)
        except RuntimeError:
            return
        if snapshot["state"] in TERMINAL_JOB_STATES:
            return
        task = runtime.task if runtime else None
        outcome = (
            dict(outcome_override)
            if outcome_override is not None
            else task.outcome_snapshot() if task else {}
        )
        cancel_requested = bool(
            snapshot.get("cancel_requested") or outcome.get("cancel_requested")
        )
        has_process_outcome = any(
            (
                outcome.get("exit_code") is not None,
                bool(outcome.get("environment_error")),
                bool(outcome.get("start_error")),
                bool(outcome.get("timed_out")),
                bool(outcome.get("termination_error")),
                bool(outcome.get("process_alive")),
            )
        )
        if outcome.get("cancelled") or (
            cancel_requested and not has_process_outcome
        ):
            self.store.transition(
                job_id,
                "cancelled",
                QGIS_BINARY_CATEGORY,
                cancel_requested=True,
                finished_at=utc_now_rfc3339(),
                progress_text="QGIS binary job cancelled",
            )
            return
        success_exit_codes = (
            runtime.binary.get("success_exit_codes", [0]) if runtime else [0]
        )
        process_succeeded = (
            outcome.get("exit_code") in success_exit_codes
            and not outcome.get("environment_error")
            and not outcome.get("start_error")
            and not outcome.get("timed_out")
        )
        if process_succeeded:
            result = {
                key: outcome.get(key)
                for key in (
                    "exit_code",
                    "stdout",
                    "stderr",
                    "stdout_truncated",
                    "stderr_truncated",
                    "stdout_total_bytes",
                    "stderr_total_bytes",
                    "duration_seconds",
                )
            }
            self.store.transition(
                job_id,
                "succeeded",
                QGIS_BINARY_CATEGORY,
                progress_percent=(
                    100.0
                    if runtime and runtime.binary.get("progress_parser") is not None
                    else snapshot.get("progress_percent")
                ),
                progress_text="QGIS binary completed",
                finished_at=utc_now_rfc3339(),
                error=None,
                result=result,
                cancel_requested=cancel_requested,
            )
            return
        if outcome.get("termination_error") and (
            outcome.get("process_alive") or outcome.get("exit_code") is None
        ):
            code, stage, message = (
                "process_termination_failed",
                "cancel",
                "QGIS binary process tree could not be terminated",
            )
        elif outcome.get("environment_error"):
            code, stage, message = (
                "environment_setup_failed",
                "environment",
                outcome["environment_error"],
            )
        elif outcome.get("start_error"):
            code, stage, message = (
                "process_start_failed",
                "start",
                outcome["start_error"],
            )
        elif outcome.get("timed_out"):
            code, stage, message = (
                "process_timed_out",
                "execute",
                "QGIS binary exceeded its timeout",
            )
        else:
            code, stage, message = (
                "process_exit_failed",
                "execute",
                f"QGIS binary exited with code {outcome.get('exit_code')}",
            )
        self._finish_failed(
            job_id,
            code,
            stage,
            message,
            outcome,
            cancel_requested=cancel_requested,
        )

    def _finish_failed(
        self,
        job_id: str,
        code: str,
        stage: str,
        message: str,
        details: dict[str, Any],
        cancel_requested: bool | None = None,
    ) -> dict[str, Any]:
        changes = {
            "finished_at": utc_now_rfc3339(),
            "progress_text": message,
            "error": {
                "code": code,
                "stage": stage,
                "message": message,
                "details": details,
                "feedback_messages": [],
            },
        }
        if cancel_requested is not None:
            changes["cancel_requested"] = cancel_requested
        return self.store.transition(
            job_id,
            "failed",
            QGIS_BINARY_CATEGORY,
            **changes,
        )

    def _environment_for(self, profile_id: str) -> dict[str, str]:
        with self._environment_lock:
            cached = self._environment_cache.get(profile_id)
            if cached is not None:
                return dict(cached)
            profile = self._catalog["environment_profiles"].get(profile_id)
            if profile is None:
                raise BinaryCatalogError(f"Unknown environment profile: {profile_id}")
            base_dependency = self._dependencies.get("environment")
            if profile["inherit_environment"]:
                if base_dependency is None:
                    environment = dict(os.environ)
                else:
                    environment = dict(
                        base_dependency()
                        if callable(base_dependency)
                        else base_dependency
                    )
            else:
                environment = {}
            setup_scripts = profile.get("setup_scripts", [])
            if setup_scripts:
                environment = self._capture_environment(setup_scripts, environment)
            for name, value in profile.get("variables", {}).items():
                environment[str(name)] = _expand_package_root(
                    str(value), self.package_root
                )
            self._environment_cache[profile_id] = dict(environment)
            return dict(environment)

    def _capture_environment(
        self, setup_scripts: list[str], environment: dict[str, str]
    ) -> dict[str, str]:
        trusted = []
        for value in setup_scripts:
            script = _resolve_trusted_setup_script(self.package_root, value)
            trusted.append(script)
        comspec = self._dependencies.get("comspec") or _trusted_windows_executable(
            "cmd.exe", environment
        )
        run_factory = self._dependencies.get("run_factory") or subprocess.run
        kwargs: dict[str, Any] = {
            "shell": False,
            "stdin": subprocess.DEVNULL,
            "stdout": subprocess.PIPE,
            "stderr": subprocess.PIPE,
            "env": environment,
            "timeout": 30,
            "check": False,
        }
        if os.name == "nt":
            kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
        capture_environment = dict(environment)
        capture_names = []
        command_parts = []
        for index, script in enumerate(trusted):
            name = f"QCOPILOTS_CAPTURE_SCRIPT_{index}"
            capture_names.append(name)
            capture_environment[name] = str(script)
            command_parts.append(f'call "%{name}%" >nul')
        command_parts.append("set")
        command = " && ".join(command_parts)
        raw_command = (
            subprocess.list2cmdline([str(comspec)])
            + f' /u /d /s /c "{command}"'
        )
        kwargs["env"] = capture_environment
        completed = run_factory(raw_command, **kwargs)
        capture_encoding = "utf-16-le"
        if completed.returncode != 0:
            stderr = completed.stderr
            if isinstance(stderr, bytes):
                stderr = stderr.decode(capture_encoding, errors="replace")
            raise RuntimeError(f"QGIS environment setup failed: {stderr}")
        stdout = completed.stdout
        if isinstance(stdout, bytes):
            stdout = stdout.decode(capture_encoding, errors="replace")
        captured = {}
        for line in str(stdout).splitlines():
            if "=" not in line:
                continue
            name, value = line.split("=", 1)
            if name:
                captured[name] = value
        for name in capture_names:
            captured.pop(name, None)
        if not captured:
            raise RuntimeError("QGIS environment setup returned an empty environment")
        return captured

    def _load_catalog_document(self, path: Path) -> dict[str, Any]:
        loader = self._dependencies.get("catalog_loader")
        if loader:
            document = loader(path)
        else:
            try:
                document = json.loads(path.read_text(encoding="utf-8"))
            except Exception as err:
                raise BinaryCatalogError(f"Could not load QGIS binary catalog: {err}") from err
        if not isinstance(document, dict):
            raise BinaryCatalogError("QGIS binary catalog must be an object")
        return document


def _validate_catalog(
    document: dict[str, Any], package_root: Path
) -> tuple[dict[str, Any], dict[str, dict[str, Any]]]:
    profiles = document.get("environment_profiles")
    groups = document.get("groups")
    entries = document.get("binaries")
    if not isinstance(profiles, dict) or not profiles:
        raise BinaryCatalogError("environment_profiles must be a non-empty object")
    if not isinstance(groups, dict):
        raise BinaryCatalogError("groups must be an object")
    if not isinstance(entries, list):
        raise BinaryCatalogError("binaries must be an array")
    expected = document.get("expected_counts")
    if not isinstance(expected, dict) or set(expected) != {
        "configured",
        "enabled",
        "disabled",
    }:
        raise BinaryCatalogError(
            "expected_counts must contain configured, enabled, and disabled"
        )
    if any(
        isinstance(expected[key], bool)
        or not isinstance(expected[key], int)
        or expected[key] < 0
        for key in expected
    ):
        raise BinaryCatalogError("expected_counts values must be non-negative integers")
    normalized_profiles = {}
    for profile_id, raw_profile in profiles.items():
        if not isinstance(raw_profile, dict):
            raise BinaryCatalogError(f"Invalid environment profile: {profile_id}")
        scripts = raw_profile.get("setup_scripts", [])
        if isinstance(scripts, str):
            scripts = [scripts]
        if not isinstance(scripts, list) or any(not isinstance(item, str) for item in scripts):
            raise BinaryCatalogError(f"Invalid setup_scripts for profile: {profile_id}")
        for script in scripts:
            _resolve_trusted_setup_script(package_root, script, require_exists=False)
        variables = raw_profile.get("variables", {})
        if not isinstance(variables, dict):
            raise BinaryCatalogError(f"Invalid variables for profile: {profile_id}")
        inherit_environment = raw_profile.get("inherit_environment")
        if not isinstance(inherit_environment, bool):
            raise BinaryCatalogError(
                f"inherit_environment must be boolean for profile: {profile_id}"
            )
        normalized_profiles[str(profile_id)] = {
            **raw_profile,
            "inherit_environment": inherit_environment,
            "setup_scripts": scripts,
            "variables": {str(key): str(value) for key, value in variables.items()},
        }

    binaries: dict[str, dict[str, Any]] = {}
    configured_paths: dict[str, str] = {}
    enabled_count = 0
    for raw_entry in entries:
        if not isinstance(raw_entry, dict):
            raise BinaryCatalogError("Every binary entry must be an object")
        binary_id = _required_string(raw_entry.get("id"), "binary id")
        if binary_id in binaries:
            raise BinaryCatalogError(f"Duplicate binary id: {binary_id}")
        group_id = _required_string(raw_entry.get("group"), f"group for {binary_id}")
        if group_id not in groups:
            raise BinaryCatalogError(f"Unknown group {group_id} for {binary_id}")
        group_config = groups[group_id]
        if not isinstance(group_config, dict):
            raise BinaryCatalogError(f"Invalid group definition: {group_id}")
        defaults = group_config.get("defaults", group_config)
        if not isinstance(defaults, dict):
            raise BinaryCatalogError(f"Invalid group defaults: {group_id}")
        merged = {**defaults, **raw_entry}
        relative_path = _normalize_relative_executable_path(merged.get("path"))
        path_key = relative_path.casefold()
        if path_key in configured_paths:
            raise BinaryCatalogError(
                f"Duplicate binary path: {relative_path} and {configured_paths[path_key]}"
            )
        configured_paths[path_key] = relative_path
        environment = _required_string(
            merged.get("environment"), f"environment for {binary_id}"
        )
        if environment not in normalized_profiles:
            raise BinaryCatalogError(
                f"Unknown environment profile {environment} for {binary_id}"
            )
        enabled = merged.get("enabled")
        if not isinstance(enabled, bool):
            raise BinaryCatalogError(f"enabled must be boolean for {binary_id}")
        disabled_reason = merged.get("disabled_reason")
        if not enabled and not isinstance(disabled_reason, str):
            raise BinaryCatalogError(f"disabled_reason is required for {binary_id}")
        risk = _normalize_risk(merged.get("risk"))
        if risk == "R3" and enabled:
            raise BinaryCatalogError(f"R3 binary must be disabled: {binary_id}")
        probe = _validate_probe(merged.get("probe"), binary_id)
        success_codes = merged.get("success_exit_codes", [0])
        if not isinstance(success_codes, list) or not success_codes or any(
            isinstance(code, bool) or not isinstance(code, int) for code in success_codes
        ):
            raise BinaryCatalogError(f"Invalid success_exit_codes for {binary_id}")
        timeout_seconds = _positive_catalog_number(
            merged.get("timeout_seconds", 60),
            f"timeout_seconds for {binary_id}",
            MAX_BINARY_TIMEOUT_SECONDS,
        )
        max_output_bytes = _positive_catalog_integer(
            merged.get("max_output_bytes", 128 * 1024),
            f"max_output_bytes for {binary_id}",
            MAX_BINARY_OUTPUT_BYTES,
        )
        max_stdin_bytes = _nonnegative_catalog_integer(
            merged.get("max_stdin_bytes", MAX_BINARY_STDIN_BYTES),
            f"max_stdin_bytes for {binary_id}",
            MAX_BINARY_STDIN_BYTES,
        )
        progress_parser = _normalize_progress_parser(merged.get("progress_parser"))
        target = _resolve_catalog_executable(
            package_root, relative_path, require_exists=False
        )
        normalized = {
            **merged,
            "id": binary_id,
            "name": str(merged.get("name") or Path(relative_path).stem),
            "description": str(merged.get("description") or ""),
            "path": relative_path,
            "resolved_path": str(target),
            "group": group_id,
            "environment": environment,
            "enabled": enabled,
            "disabled_reason": disabled_reason if not enabled else None,
            "risk": risk,
            "probe": probe,
            "success_exit_codes": success_codes,
            "timeout_seconds": timeout_seconds,
            "max_output_bytes": max_output_bytes,
            "max_stdin_bytes": max_stdin_bytes,
            "progress_parser": progress_parser,
        }
        binaries[binary_id] = normalized
        if enabled:
            enabled_count += 1

    actual_paths = _actual_executable_paths(package_root)
    configured_set = set(configured_paths)
    missing = sorted(configured_set - actual_paths)
    extra = sorted(actual_paths - configured_set)
    if missing or extra:
        raise BinaryCatalogError(
            "QGIS binary catalog does not match the package executable set. "
            f"missing={missing[:20]}, extra={extra[:20]}, "
            f"missing_count={len(missing)}, extra_count={len(extra)}"
        )
    actual_counts = {
        "configured": len(binaries),
        "enabled": enabled_count,
        "disabled": len(binaries) - enabled_count,
    }
    for key, actual in actual_counts.items():
        if expected[key] != actual:
            raise BinaryCatalogError(
                f"QGIS binary catalog count mismatch for {key}: "
                f"expected {expected[key]}, actual {actual}"
            )
    normalized_document = {
        **document,
        "environment_profiles": normalized_profiles,
        "binaries": list(binaries.values()),
    }
    return normalized_document, binaries


def _validate_catalog_header(document: dict[str, Any]) -> None:
    if document.get("version") != 1:
        raise BinaryCatalogError("QGIS binary catalog version must be 1")
    resolution = document.get("package_root_resolution")
    if not isinstance(resolution, dict):
        raise BinaryCatalogError("package_root_resolution must be an object")
    if resolution.get("strategy") != "qgis_prefix_ancestor":
        raise BinaryCatalogError(
            "package_root_resolution.strategy must be qgis_prefix_ancestor"
        )


def _default_catalog_path() -> Path:
    return (
        Path(__file__).resolve().parents[1]
        / "qcopilots_mcp_server_qgis_binary"
        / "qgis_binaries.json"
    )


def _resolve_package_root(
    configured: str | Path | None,
    document: dict[str, Any],
    dependencies: dict[str, Any],
) -> Path:
    resolution = document.get("package_root_resolution")
    if not isinstance(resolution, dict):
        raise BinaryCatalogError("package_root_resolution must be an object")
    markers = resolution.get("markers")
    max_parent_levels = resolution.get("max_parent_levels")
    if not isinstance(markers, list) or not markers or any(
        not isinstance(marker, str) for marker in markers
    ):
        raise BinaryCatalogError("package_root_resolution.markers must be non-empty")
    if (
        isinstance(max_parent_levels, bool)
        or not isinstance(max_parent_levels, int)
        or not 0 <= max_parent_levels <= 32
    ):
        raise BinaryCatalogError(
            "package_root_resolution.max_parent_levels must be from 0 through 32"
        )
    resolver = dependencies.get("package_root_resolver")
    if resolver:
        value = resolver(configured, document)
        return _existing_directory(value, "resolved package root")
    if configured is not None:
        return _existing_directory(configured, "package root")
    names = resolution.get(
        "environment_variables", ["QGIS_PACKAGE_ROOT", "OSGEO4W_ROOT"]
    )
    for name in names:
        value = os.environ.get(str(name))
        if value:
            root = _existing_directory(value, f"package root from {name}")
            if _root_matches_markers(root, markers):
                return root
            raise BinaryCatalogError(
                f"Package root from {name} does not satisfy catalog markers"
            )
    try:
        from qgis.core import QgsApplication

        prefix = Path(QgsApplication.prefixPath()).resolve(strict=True)
    except Exception as err:
        raise BinaryCatalogError(
            f"Could not obtain the QGIS prefix for package root discovery: {err}"
        ) from err
    candidates = [prefix, *list(prefix.parents)[:max_parent_levels]]
    matches = [candidate for candidate in candidates if _root_matches_markers(candidate, markers)]
    unique_matches = list(dict.fromkeys(matches))
    if len(unique_matches) != 1:
        raise BinaryCatalogError(
            "QGIS package root discovery requires exactly one marker match, "
            f"found {len(unique_matches)}"
        )
    return _existing_directory(unique_matches[0], "discovered package root")


def _existing_directory(value: str | Path, name: str) -> Path:
    try:
        candidate = Path(value).expanduser().resolve(strict=True)
    except Exception as err:
        raise BinaryCatalogError(f"Invalid {name}: {err}") from err
    if not candidate.is_dir():
        raise BinaryCatalogError(f"{name} is not a directory: {candidate}")
    return candidate


def _root_matches_markers(root: Path, markers: list[str]) -> bool:
    if not root.is_dir():
        return False
    for marker in markers:
        marker_text = marker.replace("\\", "/")
        relative = PurePosixPath(marker_text)
        if (
            relative.is_absolute()
            or PureWindowsPath(marker_text).is_absolute()
            or ".." in relative.parts
        ):
            raise BinaryCatalogError(f"Invalid package root marker: {marker}")
        if not root.joinpath(*relative.parts).exists():
            return False
    return True


def _actual_executable_paths(package_root: Path) -> set[str]:
    paths = set()
    for candidate in package_root.rglob("*"):
        if not candidate.is_file() or candidate.suffix.casefold() != ".exe":
            continue
        resolved = candidate.resolve(strict=True)
        try:
            resolved.relative_to(package_root)
        except ValueError as err:
            raise BinaryCatalogError(f"Executable escapes package root: {candidate}") from err
        paths.add(candidate.relative_to(package_root).as_posix().casefold())
    return paths


def _normalize_relative_executable_path(value: Any) -> str:
    path_text = _required_string(value, "binary path").replace("\\", "/")
    posix = PurePosixPath(path_text)
    windows = PureWindowsPath(path_text)
    if posix.is_absolute() or windows.is_absolute() or windows.drive:
        raise BinaryCatalogError(f"Binary path must be relative: {path_text}")
    if any(part in ("", ".", "..") for part in posix.parts):
        raise BinaryCatalogError(f"Invalid binary path: {path_text}")
    if posix.suffix.casefold() != ".exe":
        raise BinaryCatalogError(f"Binary path must identify an .exe file: {path_text}")
    return posix.as_posix()


def _resolve_catalog_executable(
    package_root: Path, relative_path: str, *, require_exists: bool = True
) -> Path:
    normalized = _normalize_relative_executable_path(relative_path)
    candidate = package_root.joinpath(*PurePosixPath(normalized).parts).resolve(
        strict=require_exists
    )
    try:
        candidate.relative_to(package_root)
    except ValueError as err:
        raise BinaryCatalogError(f"Binary path escapes package root: {relative_path}") from err
    if require_exists and not candidate.is_file():
        raise BinaryExecutionError(f"Configured QGIS binary is missing: {relative_path}")
    return candidate


def _resolve_working_directory(value: Any, package_root: Path) -> Path:
    if value is None or value == "":
        return package_root
    if not isinstance(value, str):
        raise ValueError("working_directory must be a string")
    candidate = Path(value).expanduser()
    if not candidate.is_absolute():
        candidate = package_root / candidate
    candidate = candidate.resolve(strict=True)
    if not candidate.is_dir():
        raise ValueError("working_directory must identify an existing directory")
    return candidate


def _resolve_trusted_setup_script(
    package_root: Path, value: Any, *, require_exists: bool = True
) -> Path:
    path_text = _required_string(value, "environment setup script").replace("\\", "/")
    posix = PurePosixPath(path_text)
    if posix.is_absolute() or PureWindowsPath(path_text).is_absolute() or ".." in posix.parts:
        raise BinaryCatalogError(f"Environment setup script must be relative: {path_text}")
    if posix.as_posix().casefold() not in {
        "bin/qgis-qt6-env.bat",
        "bin/o4w_env.bat",
    }:
        raise BinaryCatalogError(f"Untrusted environment setup script: {path_text}")
    candidate = package_root.joinpath(*posix.parts).resolve(strict=require_exists)
    try:
        candidate.relative_to(package_root)
    except ValueError as err:
        raise BinaryCatalogError(f"Setup script escapes package root: {path_text}") from err
    if require_exists and not candidate.is_file():
        raise BinaryCatalogError(f"Environment setup script is missing: {path_text}")
    return candidate


def _normalize_progress_parser(value: Any) -> dict[str, str] | None:
    if value is None or value == "none":
        return None
    if isinstance(value, str):
        value = {"type": value}
    if not isinstance(value, dict):
        raise BinaryCatalogError("progress_parser must be null or an object")
    parser_type = value.get("type")
    if parser_type == "gdal_dotted":
        return {"type": "gdal_dotted"}
    if parser_type == "percent_regex":
        pattern = _required_string(value.get("pattern"), "progress parser pattern")
        try:
            re.compile(pattern)
        except re.error as err:
            raise BinaryCatalogError(f"Invalid progress parser pattern: {err}") from err
        return {"type": "percent_regex", "pattern": pattern}
    raise BinaryCatalogError(f"Unknown progress parser type: {parser_type}")


def _validate_probe(value: Any, binary_id: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise BinaryCatalogError(f"probe must be an object for {binary_id}")
    mode = value.get("mode")
    if mode == "argv":
        arguments = value.get("arguments")
        success_codes = value.get("success_exit_codes")
        if not isinstance(arguments, list) or not arguments or any(
            not isinstance(argument, str) for argument in arguments
        ):
            raise BinaryCatalogError(f"Invalid argv probe arguments for {binary_id}")
        if not isinstance(success_codes, list) or not success_codes or any(
            isinstance(code, bool) or not isinstance(code, int)
            for code in success_codes
        ):
            raise BinaryCatalogError(
                f"Invalid argv probe success_exit_codes for {binary_id}"
            )
        timeout = _positive_catalog_number(
            value.get("timeout_seconds"),
            f"probe timeout_seconds for {binary_id}",
            MAX_BINARY_TIMEOUT_SECONDS,
        )
        return {
            "mode": "argv",
            "arguments": list(arguments),
            "success_exit_codes": list(success_codes),
            "timeout_seconds": timeout,
        }
    if mode == "static":
        checks = value.get("checks")
        sha256 = value.get("sha256")
        required_checks = {"exists", "readable", "pe_machine", "sha256"}
        if not isinstance(checks, list) or set(checks) != required_checks:
            raise BinaryCatalogError(f"Invalid static probe checks for {binary_id}")
        if not isinstance(sha256, str) or not re.fullmatch(r"[0-9a-f]{64}", sha256):
            raise BinaryCatalogError(f"Invalid static probe sha256 for {binary_id}")
        return {"mode": "static", "checks": list(checks), "sha256": sha256}
    raise BinaryCatalogError(f"Unknown probe mode for {binary_id}: {mode}")


def _binary_summary(binary: dict[str, Any]) -> dict[str, Any]:
    return {
        key: binary.get(key)
        for key in (
            "id",
            "name",
            "description",
            "path",
            "group",
            "enabled",
            "disabled_reason",
            "risk",
            "environment",
        )
    }


def _public_binary(binary: dict[str, Any]) -> dict[str, Any]:
    return {
        key: json.loads(json.dumps(value, ensure_ascii=False))
        for key, value in binary.items()
        if key != "resolved_path"
    }


def _public_catalog(catalog: dict[str, Any]) -> dict[str, Any]:
    result = {
        key: json.loads(json.dumps(value, ensure_ascii=False))
        for key, value in catalog.items()
        if key != "binaries"
    }
    result["binaries"] = [_public_binary(binary) for binary in catalog["binaries"]]
    return result


def _binary_job_summary(snapshot: dict[str, Any]) -> dict[str, Any]:
    keys = (
        "job_id",
        "category",
        "client_request_id",
        "state",
        "execution_mode",
        "cancellable",
        "binary",
        "progress_percent",
        "progress_text",
        "processed_count",
        "cancel_requested",
        "created_at",
        "started_at",
        "finished_at",
    )
    result = {key: snapshot.get(key) for key in keys}
    error = snapshot.get("error")
    result["error"] = (
        {
            "code": error.get("code"),
            "stage": error.get("stage"),
            "message": error.get("message"),
        }
        if isinstance(error, dict)
        else None
    )
    return result


def _fingerprint(value: dict[str, Any]) -> str:
    encoded = json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def _bounded_timeout(value: Any, configured_maximum: float) -> float:
    if value is None:
        return float(configured_maximum)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("timeout_seconds must be a number")
    timeout = float(value)
    if not math.isfinite(timeout) or timeout <= 0 or timeout > configured_maximum:
        raise ValueError(
            f"timeout_seconds must be greater than zero and at most {configured_maximum:g}"
        )
    return timeout


def _required_string(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{name} must be a non-empty string")
    return value.strip()


def _client_request_id(value: Any) -> str | None:
    if value is None:
        return None
    request_id = _required_string(value, "client_request_id")
    if len(request_id) > 128:
        raise ValueError("client_request_id must contain at most 128 characters")
    return request_id


def _normalize_risk(value: Any) -> str:
    risk = _required_string(value, "risk").upper()
    if risk not in {"R1", "R2", "R3"}:
        raise BinaryCatalogError(f"Unknown binary risk level: {risk}")
    return risk


def _positive_catalog_number(value: Any, name: str, maximum: float) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise BinaryCatalogError(f"{name} must be a number")
    result = float(value)
    if not math.isfinite(result) or result <= 0 or result > maximum:
        raise BinaryCatalogError(f"{name} must be greater than zero and at most {maximum:g}")
    return result


def _positive_catalog_integer(value: Any, name: str, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 1 <= value <= maximum:
        raise BinaryCatalogError(f"{name} must be an integer from 1 through {maximum}")
    return value


def _nonnegative_catalog_integer(value: Any, name: str, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= maximum:
        raise BinaryCatalogError(f"{name} must be an integer from 0 through {maximum}")
    return value


def _expand_package_root(value: str, package_root: Path) -> str:
    root = str(package_root)
    return value.replace("${package_root}", root).replace("{package_root}", root)


def _trusted_windows_executable(
    filename: str, environment: dict[str, str] | None = None
) -> str:
    sources = [environment or {}, os.environ]
    system_root = None
    for source in sources:
        for name, value in source.items():
            if str(name).casefold() == "systemroot" and value:
                system_root = str(value)
                break
        if system_root:
            break
    if not system_root:
        raise RuntimeError("SystemRoot is unavailable for trusted Windows tools")
    candidate = (Path(system_root) / "System32" / filename).resolve(strict=True)
    if not candidate.is_file():
        raise RuntimeError(f"Trusted Windows executable is missing: {candidate}")
    return str(candidate)


def _connect_signal(owner: Any, name: str, callback) -> None:
    try:
        getattr(owner, name).connect(callback)
    except (AttributeError, TypeError):
        pass
