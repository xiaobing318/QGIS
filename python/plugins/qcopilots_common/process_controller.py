"""Start, stop and inspect QCopilots MCP service process trees.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import base64
import json
import math
import os
import socket
import subprocess
import threading
import time
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any
from urllib.request import urlopen

from qcopilots_common.constants import (
    BRIDGE_URL_ENV,
    CORS_ORIGINS_ENV,
    DEFAULT_CORS_ORIGINS,
    DEFAULT_HOST,
    DEFAULT_MCP_PATH,
    DEFAULT_SERVICE_PORTS,
    SERVICE_DESCRIPTION_ENV,
    SERVICE_ICON_ENV,
    SERVICE_TITLE_ENV,
    services_state_root,
)
from qcopilots_common.logging import rotate_log_file, service_log_file
from qcopilots_common.manifest import ServiceManifest, ServiceTransport, manifest_to_dict
from qcopilots_common.service_id import is_safe_service_id, safe_service_state_name
from qcopilots_common.subprocess_utils import hidden_subprocess_kwargs, service_creationflags
from qcopilots_common.uv_runtime import UvRuntime


QGIS_PACKAGE_ROOT_ENV = "QCOPILOTS_QGIS_PACKAGE_ROOT"
STARTUP_TIMEOUT_ENV = "QCOPILOTS_SERVICE_STARTUP_TIMEOUT_SECONDS"
DEFAULT_STARTUP_TIMEOUT_SECONDS = 30.0
DEFAULT_STARTUP_POLL_INTERVAL_SECONDS = 0.25
DEFAULT_PROCESS_RECONCILE_TIMEOUT_SECONDS = 5.0
MAX_STARTUP_LOG_TAIL_BYTES = 16 * 1024
MAX_STARTUP_LOG_TAIL_CHARS = 4000

DEPENDENCY_PREPARATION_FAILED = "dependency-preparation-failed"
PROCESS_LAUNCH_FAILED = "process-launch-failed"
PROCESS_EXITED_BEFORE_HEALTHY = "process-exited-before-healthy"
HTTP_HEALTH_TIMEOUT = "http-health-timeout"


@dataclass
class ProcessCommand:
    executable: str
    arguments: list[str] = field(default_factory=list)
    cwd: str | Path | None = None
    env: dict[str, str] = field(default_factory=dict)
    log_file: str | Path | None = None

    def to_argv(self) -> list[str]:
        return [self.executable, *self.arguments]

    def resolved_environment(self) -> dict[str, str]:
        environment = os.environ.copy()
        environment.update(self.env)
        return environment


@dataclass
class ServiceStatus:
    service_id: str
    running: bool
    pid: int | None
    port: int
    url: str
    log_file: str
    health: str
    process_tree: list[int] = field(default_factory=list)
    owner_match: bool = False
    runtime_python: str = ""
    startup_phase: str = ""
    diagnostic: str = ""
    exit_code: int | None = None
    log_tail: str = ""


class ProcessController:
    def __init__(
        self,
        state_root: str | Path | ProcessCommand | None = None,
        runtime: UvRuntime | None = None,
        service_id: str | None = None,
        qgis_executable: str | Path | None = None,
        startup_timeout_seconds: float | None = None,
        startup_poll_interval_seconds: float = DEFAULT_STARTUP_POLL_INTERVAL_SECONDS,
    ):
        self.command: ProcessCommand | None = state_root if isinstance(state_root, ProcessCommand) else None
        self.service_id = service_id or "qcopilots-service"
        self.process: subprocess.Popen | None = None
        self._manifest_processes: dict[str, subprocess.Popen] = {}
        self.exit_code: int | None = None
        self.owner_token = uuid.uuid4().hex
        self._log_handle = None
        if self.command is not None:
            self.state_root = Path(self.command.cwd or Path.cwd())
        else:
            self.state_root = Path(state_root) if state_root else services_state_root()
        self.state_root.mkdir(parents=True, exist_ok=True)
        self.runtime = runtime or UvRuntime()
        self.qgis_executable = Path(qgis_executable) if qgis_executable else None
        self.startup_timeout_seconds = _configured_startup_timeout(startup_timeout_seconds)
        self.startup_poll_interval_seconds = _positive_timeout(
            startup_poll_interval_seconds,
            "startup_poll_interval_seconds",
        )
        self._lock = threading.RLock()

    def status(self, manifest: ServiceManifest, deep: bool = True) -> ServiceStatus:
        with self._lock:
            state = self._read_state(manifest)
            pid = state.get("pid")
            port = int(state.get("port") or manifest.default_port)
            owner_match = bool(state.get("owner_token") and state.get("owner_token") == self.owner_token)
            tracked_process = self._tracked_manifest_process(manifest)
            tracked_process_live = bool(
                tracked_process is not None and tracked_process.poll() is None
            )
            tracked_pid_matches = bool(
                pid
                and tracked_process is not None
                and tracked_process.pid == int(pid)
            )
            owns_live_handle = tracked_pid_matches and tracked_process_live
            running = bool(
                pid
                and (
                    owns_live_handle
                    or is_process_running(int(pid))
                )
            )
            if (
                deep
                and running
                and state.get("process_identity")
                and not owns_live_handle
                and not process_matches_state(int(pid), state)
            ):
                running = False
            if (
                deep
                and running
                and not owner_match
                and not owns_live_handle
                and not process_matches_state(int(pid), state)
            ):
                running = False
            process_tree = process_tree_pids(int(pid)) if deep and running and pid else []
            configuration_match = _service_configuration_matches_state(manifest, state)
            startup_failure = str(state.get("startup_failure") or "")
            health = startup_failure or "stopped"
            if running:
                if startup_failure:
                    health = startup_failure
                else:
                    health = "starting"
                    if not deep:
                        health = "running"
                    elif self._health_ok(manifest.host, port):
                        health = "ok"
                if not configuration_match:
                    health = "configuration-mismatch"
                elif state.get("stop_refused"):
                    health = "owner-mismatch"
            return ServiceStatus(
                service_id=manifest.service_id,
                running=running,
                pid=int(pid) if pid else None,
                port=port,
                url=_status_url(manifest, state, port, running, configuration_match),
                log_file=str(state.get("log_file") or service_log_file(manifest.service_id)),
                health=health,
                process_tree=process_tree,
                owner_match=owner_match,
                runtime_python=str(state.get("runtime_python", "")),
                startup_phase=str(state.get("startup_phase") or ""),
                diagnostic=str(state.get("diagnostic") or ""),
                exit_code=_optional_int(state.get("exit_code")),
                log_tail=str(state.get("log_tail") or ""),
            )

    def start(
        self,
        manifest: ServiceManifest | None = None,
        bridge_url: str | None = None,
        extra_env: dict[str, str] | None = None,
        startup_timeout_seconds: float | None = None,
    ) -> ServiceStatus | None:
        with self._lock:
            if manifest is None:
                self._start_command()
                return None

            state = self._read_state(manifest)
            self._reconcile_manifest_process_before_start(manifest, state)
            state = self._read_state(manifest)
            current = self.status(manifest)
            if current.running:
                if not _service_configuration_matches_state(manifest, state):
                    if current.owner_match:
                        stopped = self.stop(manifest)
                        if stopped and stopped.running:
                            return stopped
                    else:
                        state["stop_refused"] = "configuration_mismatch"
                        state["stop_refused_at"] = time.time()
                        self._write_state(manifest, state)
                        return self.status(manifest)
                else:
                    return current

            port = find_available_port(manifest.host, manifest.default_port)
            log_file = service_log_file(manifest.service_id)
            log_file.parent.mkdir(parents=True, exist_ok=True)
            rotate_log_file(log_file)
            timeout_seconds = (
                self.startup_timeout_seconds
                if startup_timeout_seconds is None
                else _positive_timeout(startup_timeout_seconds, "startup_timeout_seconds")
            )
            state = {
                "pid": None,
                "port": port,
                "url": manifest.url(port),
                "log_file": str(log_file),
                "started_at": time.time(),
                "manifest": manifest_to_dict(manifest, port),
                "command": [],
                "runtime_python": "",
                "service_python": "",
                "owner_token": self.owner_token,
                "manager_pid": os.getpid(),
                "startup_phase": "process-preparation",
            }
            self._write_state(manifest, state)
            try:
                service_python = resolve_service_python_executable(self.qgis_executable)
            except Exception as err:
                _append_startup_log(log_file, f"Service process preparation failed: {err}")
                return self._record_startup_failure(
                    manifest,
                    state,
                    PROCESS_LAUNCH_FAILED,
                    log_file,
                    exit_code=_exception_exit_code(err),
                    summary="Service process preparation failed",
                )

            state["service_python"] = str(service_python)
            try:
                if hasattr(self.runtime, "service_command"):
                    command = self.runtime.service_command(manifest, service_python)
                    runtime_python = service_python
                else:
                    state["startup_phase"] = "dependency-preparation"
                    self._write_state(manifest, state)
                    runtime_python = self.runtime.ensure_runtime(manifest, str(service_python))
                    command = [str(runtime_python), str(manifest.entry_path)]
            except Exception as err:
                _append_startup_log(log_file, f"Dependency preparation failed: {err}")
                return self._record_startup_failure(
                    manifest,
                    state,
                    DEPENDENCY_PREPARATION_FAILED,
                    log_file,
                    exit_code=_exception_exit_code(err),
                    summary="Dependency preparation failed",
                )

            env = os.environ.copy()
            cors_origins = _merged_cors_origins(manifest.cors_origins or DEFAULT_CORS_ORIGINS)
            env.update(
                {
                    "PYTHONUNBUFFERED": "1",
                    "QCOPILOTS_SERVICE_HOST": manifest.host,
                    "QCOPILOTS_SERVICE_PORT": str(port),
                    "QCOPILOTS_MCP_PATH": manifest.mcp_path,
                    "QCOPILOTS_LOG_FILE": str(log_file),
                    CORS_ORIGINS_ENV: os.pathsep.join(cors_origins),
                    SERVICE_DESCRIPTION_ENV: manifest.description,
                    SERVICE_TITLE_ENV: manifest.plugin_name,
                }
            )
            icon_descriptors = manifest.icon_descriptors()
            if icon_descriptors:
                env[SERVICE_ICON_ENV] = json.dumps(icon_descriptors, ensure_ascii=False)
            if bridge_url:
                env[BRIDGE_URL_ENV] = bridge_url
            if extra_env:
                env.update(extra_env)

            creationflags = _service_creationflags()
            preexec_fn = None if os.name == "nt" else os.setsid
            try:
                with log_file.open("a", encoding="utf-8") as log_handle:
                    if port != manifest.default_port:
                        log_handle.write(
                            f"Preferred port {manifest.default_port} was busy, using {port}\n"
                        )
                    process = subprocess.Popen(
                        command,
                        cwd=str(manifest.plugin_dir),
                        env=env,
                        stdout=log_handle,
                        stderr=subprocess.STDOUT,
                        creationflags=creationflags,
                        preexec_fn=preexec_fn,
                    )
            except Exception as err:
                _append_startup_log(log_file, f"Service process launch failed: {err}")
                state.update(
                    {
                        "command": command,
                        "runtime_python": str(runtime_python),
                        "service_python": str(service_python),
                    }
                )
                return self._record_startup_failure(
                    manifest,
                    state,
                    PROCESS_LAUNCH_FAILED,
                    log_file,
                    exit_code=_exception_exit_code(err),
                    summary="Service process launch failed",
                )

            self.process = process
            self._manifest_processes[manifest.service_id] = process
            self.exit_code = None
            state.update(
                {
                    "pid": process.pid,
                    "command": command,
                    "runtime_python": str(runtime_python),
                    "service_python": str(service_python),
                    "startup_phase": (
                        "dependency-preparation"
                        if _uses_runtime_dependency_overlay(command)
                        else "process-starting"
                    ),
                }
            )
            self._write_state(manifest, state)
            deadline = time.monotonic() + timeout_seconds
            while True:
                exit_code = process.poll()
                if exit_code is not None:
                    self.exit_code = int(exit_code)
                    return self._record_startup_failure(
                        manifest,
                        state,
                        _classify_process_exit(command, log_file),
                        log_file,
                        exit_code=int(exit_code),
                    )

                if self._health_ok(manifest.host, port):
                    exit_code = process.poll()
                    if exit_code is not None:
                        self.exit_code = int(exit_code)
                        return self._record_startup_failure(
                            manifest,
                            state,
                            _classify_process_exit(command, log_file),
                            log_file,
                            exit_code=int(exit_code),
                        )
                    state["startup_phase"] = "ready"
                    state["process_identity"] = process_identity(process.pid)
                    exit_code = process.poll()
                    if exit_code is not None:
                        self.exit_code = int(exit_code)
                        return self._record_startup_failure(
                            manifest,
                            state,
                            _classify_process_exit(command, log_file),
                            log_file,
                            exit_code=int(exit_code),
                        )
                    _clear_startup_failure(state)
                    self._write_state(manifest, state)
                    return self.status(manifest)

                remaining_seconds = deadline - time.monotonic()
                if remaining_seconds <= 0:
                    return self._record_startup_failure(
                        manifest,
                        state,
                        HTTP_HEALTH_TIMEOUT,
                        log_file,
                        timeout_seconds=timeout_seconds,
                    )
                time.sleep(min(self.startup_poll_interval_seconds, remaining_seconds))

    def stop(self, manifest: ServiceManifest | None = None, timeout_seconds: float = 5) -> ServiceStatus | None:
        with self._lock:
            if manifest is None:
                self._stop_command(timeout_seconds)
                return None

            state = self._read_state(manifest)
            tracked_process = self._tracked_manifest_process(manifest)
            legacy_process = self.process if not self._manifest_processes else None
            pid = state.get("pid")
            tracked_process_live = bool(
                tracked_process is not None and tracked_process.poll() is None
            )
            legacy_process_live = bool(
                legacy_process is not None and legacy_process.poll() is None
            )
            if not pid and tracked_process_live:
                pid = tracked_process.pid
            if not pid and legacy_process_live:
                pid = legacy_process.pid
            owner_match = bool(state.get("owner_token") and state.get("owner_token") == self.owner_token)
            pid_int = int(pid) if pid else None
            pid_running = bool(pid_int and is_process_running(pid_int))
            tracked_pid_matches = bool(
                pid_int
                and tracked_process is not None
                and tracked_process.pid == pid_int
            )
            legacy_pid_matches = bool(
                pid_int
                and legacy_process is not None
                and legacy_process.pid == pid_int
            )
            owns_live_handle = bool(
                (tracked_pid_matches and tracked_process_live)
                or (legacy_pid_matches and legacy_process_live)
            )
            if tracked_pid_matches and tracked_process is not None and not tracked_process_live:
                self._reap_process_handle(tracked_process, timeout_seconds)
                self._forget_manifest_process(manifest)
                if owner_match:
                    state["stopped_at"] = time.time()
                    state["pid"] = None
                    state.pop("stop_refused", None)
                    self._write_state(manifest, state)
                return self.status(manifest)
            if pid_int and tracked_process is not None and tracked_process.pid != pid_int:
                self._stop_unrecorded_manifest_process(manifest, tracked_process, timeout_seconds)
                if not pid_running:
                    state["stopped_at"] = time.time()
                    state["pid"] = None
                    state.pop("stop_refused", None)
                    self._write_state(manifest, state)
                return self.status(manifest)
            if (
                pid_int
                and pid_running
                and state.get("process_identity")
                and not process_matches_state(pid_int, state)
                and not owns_live_handle
            ):
                state["stop_refused"] = "process_identity_mismatch"
                state["stop_refused_at"] = time.time()
                self._write_state(manifest, state)
                return self.status(manifest)
            if pid_int and pid_running and not state.get("process_identity") and not owns_live_handle:
                state["stop_refused"] = "missing_process_identity"
                state["stop_refused_at"] = time.time()
                self._write_state(manifest, state)
                return self.status(manifest)
            if (
                pid_int
                and pid_running
                and not owner_match
                and not process_matches_state(pid_int, state)
                and not owns_live_handle
            ):
                state["stop_refused"] = "process_identity_mismatch"
                state["stop_refused_at"] = time.time()
                self._write_state(manifest, state)
                return self.status(manifest)
            if pid_int and pid_running and not owner_match and not owns_live_handle:
                manager_pid = int(state.get("manager_pid") or 0)
                if manager_pid <= 0 or is_process_running(manager_pid):
                    state["stop_refused"] = "owner_mismatch"
                    state["stop_refused_at"] = time.time()
                    self._write_state(manifest, state)
                    return self.status(manifest)
                state["stale_owner_recovered_at"] = time.time()
            if pid_int:
                matching_process = None
                if tracked_pid_matches:
                    matching_process = tracked_process
                elif legacy_pid_matches:
                    matching_process = legacy_process

                if pid_running or owns_live_handle:
                    self._terminate_process_tree_and_reap(
                        pid_int, matching_process, timeout_seconds
                    )
                elif matching_process is not None:
                    self._reap_process_handle(matching_process, timeout_seconds)

                if tracked_pid_matches:
                    self._forget_manifest_process(manifest)
            elif tracked_process is not None:
                if tracked_process_live:
                    self._terminate_process_tree_and_reap(
                        tracked_process.pid, tracked_process, timeout_seconds
                    )
                else:
                    self.exit_code = tracked_process.returncode
                self._forget_manifest_process(manifest)
            elif legacy_process is not None:
                if legacy_process_live:
                    self._terminate_process_tree_and_reap(
                        legacy_process.pid, legacy_process, timeout_seconds
                    )
                else:
                    self.exit_code = legacy_process.returncode
            state["stopped_at"] = time.time()
            state["pid"] = None
            state.pop("stop_refused", None)
            self._write_state(manifest, state)
            return self.status(manifest)

    def wait(self, timeout_seconds: float | None = None) -> int | None:
        if not self.process:
            return self.exit_code
        try:
            self.exit_code = self.process.wait(timeout=timeout_seconds)
        finally:
            self._close_log_handle()
        return self.exit_code

    def snapshot(self) -> dict[str, Any]:
        if self.command is None:
            return {"service_id": self.service_id, "status": "manifest-controller"}
        if self.process is None:
            return {"service_id": self.service_id, "status": "stopped", "pid": None, "exit_code": self.exit_code}
        if self.exit_code is None:
            return {"service_id": self.service_id, "status": "running", "pid": self.process.pid}
        return {
            "service_id": self.service_id,
            "status": "exited",
            "pid": self.process.pid,
            "exit_code": self.exit_code,
        }

    def stored_manifests(self) -> list[ServiceManifest]:
        with self._lock:
            manifests: list[ServiceManifest] = []
            for path in sorted(self.state_root.glob("*.json")):
                try:
                    state = json.loads(path.read_text(encoding="utf-8"))
                    manifest = _manifest_from_state(path, state.get("manifest") or {})
                except Exception:
                    continue
                if manifest:
                    manifests.append(manifest)
            return sorted(manifests, key=lambda manifest: manifest.service_id)

    def _start_command(self) -> None:
        if not self.command:
            raise ValueError("ProcessCommand is not configured")
        if self.process is not None and self.exit_code is None:
            return
        cwd = Path(self.command.cwd or Path.cwd())
        log_file = Path(self.command.log_file) if self.command.log_file else None
        stdout = subprocess.DEVNULL
        if log_file:
            log_file.parent.mkdir(parents=True, exist_ok=True)
            self._log_handle = log_file.open("a", encoding="utf-8")
            stdout = self._log_handle
        self.exit_code = None
        self.process = subprocess.Popen(
            self.command.to_argv(),
            cwd=str(cwd),
            env=self.command.resolved_environment(),
            stdout=stdout,
            stderr=subprocess.STDOUT,
            **_hidden_subprocess_kwargs(),
        )

    def _stop_command(self, timeout_seconds: float) -> None:
        if not self.process:
            self._close_log_handle()
            return
        if self.exit_code is not None:
            self._close_log_handle()
            return
        if self.process.poll() is not None:
            self.exit_code = self.process.returncode
            self._close_log_handle()
            return
        self.process.terminate()
        try:
            self.exit_code = self.process.wait(timeout=timeout_seconds)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.exit_code = self.process.wait(timeout=timeout_seconds)
        finally:
            self._close_log_handle()

    def _reap_process_handle(self, process: subprocess.Popen, timeout_seconds: float) -> None:
        try:
            if process.poll() is None:
                self.exit_code = process.wait(timeout=timeout_seconds)
            else:
                self.exit_code = process.returncode
        except subprocess.TimeoutExpired:
            process.kill()
            self.exit_code = process.wait(timeout=timeout_seconds)

    def _terminate_process_tree_and_reap(
        self,
        pid: int,
        process: subprocess.Popen | None,
        timeout_seconds: float,
    ) -> None:
        terminate_process_tree(pid, force=False)
        if is_process_running(pid):
            terminate_process_tree(pid, force=True)
        if process is not None and process.pid == pid:
            self._reap_process_handle(process, timeout_seconds)

    def _stop_unrecorded_manifest_process(
        self,
        manifest: ServiceManifest,
        process: subprocess.Popen,
        timeout_seconds: float,
    ) -> None:
        if process.poll() is None:
            self._terminate_process_tree_and_reap(process.pid, process, timeout_seconds)
        else:
            self.exit_code = process.returncode
        self._forget_manifest_process(manifest)

    def _tracked_manifest_process(self, manifest: ServiceManifest) -> subprocess.Popen | None:
        return self._manifest_processes.get(manifest.service_id)

    def _forget_manifest_process(self, manifest: ServiceManifest) -> None:
        process = self._manifest_processes.pop(manifest.service_id, None)
        if process is not None and self.process is process:
            self.process = None

    def _reconcile_manifest_process_before_start(
        self,
        manifest: ServiceManifest,
        state: dict[str, Any],
    ) -> None:
        process = self._tracked_manifest_process(manifest)
        if process is None:
            return

        state_pid = _optional_int(state.get("pid"))
        exit_code = process.poll()
        if exit_code is None:
            if state_pid == process.pid:
                return
            self._stop_unrecorded_manifest_process(
                manifest,
                process,
                DEFAULT_PROCESS_RECONCILE_TIMEOUT_SECONDS,
            )
            return

        self.exit_code = int(exit_code)
        self._reap_process_handle(process, DEFAULT_PROCESS_RECONCILE_TIMEOUT_SECONDS)
        self._forget_manifest_process(manifest)
        if state_pid != process.pid:
            return

        state["last_pid"] = process.pid
        state["pid"] = None
        state["exit_code"] = int(exit_code)
        state["exited_at"] = time.time()
        self._write_state(manifest, state)

    def _close_log_handle(self) -> None:
        if self._log_handle:
            self._log_handle.close()
            self._log_handle = None

    def _record_startup_failure(
        self,
        manifest: ServiceManifest,
        state: dict[str, Any],
        failure_code: str,
        log_file: Path,
        *,
        exit_code: int | None = None,
        timeout_seconds: float | None = None,
        summary: str | None = None,
    ) -> ServiceStatus:
        log_tail = _read_log_tail(log_file)
        if failure_code in (PROCESS_EXITED_BEFORE_HEALTHY, DEPENDENCY_PREPARATION_FAILED):
            pid = state.get("pid")
            if pid:
                state["last_pid"] = pid
            state["pid"] = None
            state["exited_at"] = time.time()
            self._forget_manifest_process(manifest)

        state["startup_failure"] = failure_code
        state["startup_phase"] = _startup_failure_phase(failure_code)
        state["startup_failed_at"] = time.time()
        state["exit_code"] = exit_code
        state["log_tail"] = log_tail
        state["diagnostic"] = _startup_diagnostic(
            failure_code,
            log_file,
            log_tail,
            exit_code=exit_code,
            timeout_seconds=timeout_seconds,
            summary=summary,
        )
        self._write_state(manifest, state)
        return self.status(manifest)

    def _state_path(self, manifest: ServiceManifest) -> Path:
        safe_name = safe_service_state_name(manifest.service_id)
        path = (self.state_root / f"{safe_name}.json").resolve()
        state_root = self.state_root.resolve()
        try:
            path.relative_to(state_root)
        except ValueError as err:
            raise ValueError(f"Service state path escapes state root: {manifest.service_id}") from err
        return path

    def _read_state(self, manifest: ServiceManifest) -> dict[str, Any]:
        path = self._state_path(manifest)
        if not path.exists():
            return {}
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except Exception:
            return {}

    def _write_state(self, manifest: ServiceManifest, state: dict[str, Any]) -> None:
        self.state_root.mkdir(parents=True, exist_ok=True)
        path = self._state_path(manifest)
        temp_path = path.with_name(f"{path.name}.{os.getpid()}.{threading.get_ident()}.tmp")
        try:
            temp_path.write_text(json.dumps(state, indent=2), encoding="utf-8")
            temp_path.replace(path)
        finally:
            temp_path.unlink(missing_ok=True)

    def _health_ok(self, host: str, port: int) -> bool:
        try:
            health_url = f"http://{_health_check_host(host)}:{port}/health"
            with urlopen(health_url, timeout=1.5) as response:
                return response.status == 200
        except Exception:
            return False


def _configured_startup_timeout(value: float | None) -> float:
    if value is not None:
        return _positive_timeout(value, "startup_timeout_seconds")
    configured = os.environ.get(STARTUP_TIMEOUT_ENV)
    if configured is None:
        return DEFAULT_STARTUP_TIMEOUT_SECONDS
    try:
        return _positive_timeout(float(configured), STARTUP_TIMEOUT_ENV)
    except (TypeError, ValueError):
        return DEFAULT_STARTUP_TIMEOUT_SECONDS


def _positive_timeout(value: float, label: str) -> float:
    timeout = float(value)
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError(f"{label} must be a finite value greater than zero")
    return timeout


def _optional_int(value: Any) -> int | None:
    if value is None or isinstance(value, bool):
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def _exception_exit_code(error: Exception) -> int | None:
    return _optional_int(getattr(error, "returncode", None))


def _uses_runtime_dependency_overlay(command: list[str]) -> bool:
    return "--with-requirements" in command or "--with" in command


def _classify_process_exit(command: list[str], log_file: Path) -> str:
    if not _uses_runtime_dependency_overlay(command):
        return PROCESS_EXITED_BEFORE_HEALTHY
    log_tail = _read_log_tail(log_file).lower()
    dependency_markers = (
        "no solution found when resolving dependencies",
        "was not found in the cache",
        "failed to download",
        "failed to fetch",
        "could not find a version that satisfies",
        "distribution was not found",
    )
    if any(marker in log_tail for marker in dependency_markers):
        return DEPENDENCY_PREPARATION_FAILED
    return PROCESS_EXITED_BEFORE_HEALTHY


def _startup_failure_phase(failure_code: str) -> str:
    return {
        DEPENDENCY_PREPARATION_FAILED: "dependency-preparation",
        PROCESS_LAUNCH_FAILED: "process-launch",
        PROCESS_EXITED_BEFORE_HEALTHY: "process-exit",
        HTTP_HEALTH_TIMEOUT: "http-health",
    }.get(failure_code, "startup")


def _startup_diagnostic(
    failure_code: str,
    log_file: Path,
    log_tail: str,
    *,
    exit_code: int | None,
    timeout_seconds: float | None,
    summary: str | None,
) -> str:
    default_summary = {
        DEPENDENCY_PREPARATION_FAILED: "Dependency preparation failed",
        PROCESS_LAUNCH_FAILED: "Service process launch failed",
        PROCESS_EXITED_BEFORE_HEALTHY: "Service process exited before the HTTP health check succeeded",
        HTTP_HEALTH_TIMEOUT: "HTTP health check timed out while the service process was still running",
    }.get(failure_code, "Service startup failed")
    lines = [summary or default_summary, f"Failure: {failure_code}"]
    if exit_code is not None:
        lines.append(f"Exit code: {exit_code}")
    if timeout_seconds is not None:
        lines.append(f"Startup timeout: {timeout_seconds:g} seconds")
    lines.append(f"Log file: {log_file}")
    lines.append("Log tail:")
    lines.append(log_tail or "<empty>")
    return "\n".join(lines)


def _clear_startup_failure(state: dict[str, Any]) -> None:
    for key in (
        "startup_failure",
        "startup_failed_at",
        "exit_code",
        "diagnostic",
        "log_tail",
    ):
        state.pop(key, None)


def _append_startup_log(log_file: Path, message: str) -> None:
    try:
        with log_file.open("a", encoding="utf-8") as handle:
            handle.write(f"{message}\n")
    except OSError:
        return


def _read_log_tail(
    log_file: Path,
    *,
    max_bytes: int = MAX_STARTUP_LOG_TAIL_BYTES,
    max_chars: int = MAX_STARTUP_LOG_TAIL_CHARS,
) -> str:
    try:
        with log_file.open("rb") as handle:
            handle.seek(0, os.SEEK_END)
            size = handle.tell()
            offset = max(0, size - max_bytes)
            handle.seek(offset)
            payload = handle.read(max_bytes)
    except OSError:
        return ""

    text = payload.decode("utf-8", errors="replace")
    if offset and "\n" in text:
        text = text.split("\n", 1)[1]
    return text[-max_chars:].strip()


def find_available_port(host: str, preferred_port: int) -> int:
    bind_host = _bind_check_host(host)
    if preferred_port <= 0:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.bind((bind_host, 0))
            return int(sock.getsockname()[1])
    for port in range(preferred_port, preferred_port + 100):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            try:
                sock.bind((bind_host, port))
            except OSError:
                continue
            else:
                return port
    raise RuntimeError(f"No available port near {preferred_port}")


def _bind_check_host(host: str) -> str:
    return (host or DEFAULT_HOST).strip() or DEFAULT_HOST


def _health_check_host(host: str) -> str:
    candidate = _bind_check_host(host)
    if candidate == "0.0.0.0":
        return DEFAULT_HOST
    return candidate


def _status_url(
    manifest: ServiceManifest,
    state: dict[str, Any],
    port: int,
    running: bool,
    configuration_match: bool,
) -> str:
    if running and not configuration_match:
        return _state_manifest_url(state.get("manifest"), manifest, port)
    return manifest.url(port)


def _state_manifest_url(value: Any, fallback_manifest: ServiceManifest, port: int) -> str:
    if not isinstance(value, dict):
        return fallback_manifest.url(port)
    host = str(
        value.get("advertised_host")
        or value.get("bind_host")
        or value.get("host")
        or fallback_manifest.host
    )
    path = str(value.get("mcp_path") or fallback_manifest.mcp_path)
    return f"http://{host}:{port}{path}"


def _service_configuration_matches_state(
    manifest: ServiceManifest,
    state: dict[str, Any],
) -> bool:
    stored = state.get("manifest")
    if not isinstance(stored, dict):
        return True

    stored_bind_host = str(stored.get("bind_host") or stored.get("host") or DEFAULT_HOST)
    if stored_bind_host != manifest.host:
        return False
    if str(stored.get("advertised_host") or "") != manifest.advertised_host:
        return False
    if str(stored.get("mcp_path") or DEFAULT_MCP_PATH) != manifest.mcp_path:
        return False

    stored_entry_point = stored.get("entry_point")
    if stored_entry_point:
        stored_entry_point = str(stored_entry_point)
        if stored_entry_point not in (str(manifest.entry_path), manifest.entry_point):
            return False

    stored_capabilities = _state_string_list(
        stored.get("capabilities"),
        manifest.capabilities,
    )
    if stored_capabilities != list(manifest.capabilities):
        return False

    stored_cors_origins = _state_string_list(
        stored.get("cors_origins"),
        DEFAULT_CORS_ORIGINS,
    )
    if stored_cors_origins != list(manifest.cors_origins or DEFAULT_CORS_ORIGINS):
        return False

    return True


def _state_string_list(value: Any, fallback: list[str] | tuple[str, ...]) -> list[str]:
    if not isinstance(value, list):
        return list(fallback)
    return [item for item in value if isinstance(item, str)]


def _merged_cors_origins(base_origins: list[str] | tuple[str, ...]) -> list[str]:
    origins = list(base_origins)
    configured = os.environ.get(CORS_ORIGINS_ENV)
    if configured:
        origins.extend(item.strip() for item in configured.split(os.pathsep) if item.strip())
    return list(dict.fromkeys(origins))


def _manifest_from_state(path: Path, data: dict[str, Any]) -> ServiceManifest | None:
    service_id = str(data.get("service_id") or path.stem.replace("_", ".")).strip()
    if not is_safe_service_id(service_id):
        return None

    plugin_dir = Path(data.get("plugin_dir") or path.parent)
    entry_point = str(data.get("entry_point") or "server.py")
    entry_path = Path(entry_point)
    if entry_path.is_absolute():
        try:
            entry_point = str(entry_path.relative_to(plugin_dir))
        except ValueError:
            entry_point = str(entry_path)

    try:
        port = int(data.get("port") or DEFAULT_SERVICE_PORTS.get(service_id) or 0)
    except (TypeError, ValueError):
        port = 0
    if port <= 0:
        return None

    return ServiceManifest(
        service_id=service_id,
        display_name=str(data.get("display_name") or service_id),
        description=str(data.get("description") or ""),
        plugin_name=str(data.get("plugin_name") or plugin_dir.name),
        plugin_dir=plugin_dir,
        manifest_path=plugin_dir / "qcopilots_service.json",
        entry_point=entry_point,
        transport=ServiceTransport(
            host=str(data.get("bind_host") or data.get("host") or DEFAULT_HOST),
            port=port,
            path=str(data.get("mcp_path") or DEFAULT_MCP_PATH),
            advertised_host=str(data.get("advertised_host") or ""),
        ),
        category=str(data.get("category") or "qcopilots"),
        capabilities=list(data.get("capabilities") or []),
        cors_origins=_state_cors_origins(data.get("cors_origins")),
        enabled=True,
    )


def _state_cors_origins(value: Any) -> list[str]:
    if not isinstance(value, list):
        return list(DEFAULT_CORS_ORIGINS)
    origins = [origin.strip() for origin in value if isinstance(origin, str) and origin.strip()]
    return origins or list(DEFAULT_CORS_ORIGINS)


def resolve_service_python_executable(qgis_executable: str | Path | None = None) -> Path:
    candidates: list[Path] = []
    package_root = os.environ.get(QGIS_PACKAGE_ROOT_ENV)
    if package_root:
        candidates.append(Path(package_root))
    if qgis_executable:
        candidates.append(Path(qgis_executable))

    for candidate in candidates:
        qgis_layout_python = _python_from_qgis_layout(candidate)
        if qgis_layout_python:
            return qgis_layout_python

    raise RuntimeError(
        "QCopilots MCP services require the Python interpreter bundled with the active QGIS package."
    )


def _python_from_qgis_layout(path: Path) -> Path | None:
    roots = [path.parent, *path.parents] if path.suffix else [path, *path.parents]
    seen: set[Path] = set()
    for root in roots:
        if root in seen:
            continue
        seen.add(root)
        apps_dir = root / "apps"
        if not apps_dir.exists():
            continue
        for python_dir in ("Python312", "Python311", "Python310"):
            candidate = apps_dir / python_dir / ("python.exe" if os.name == "nt" else "python")
            if candidate.exists():
                return candidate
        for candidate in sorted(apps_dir.glob("Python*/python.exe" if os.name == "nt" else "Python*/bin/python")):
            if candidate.exists():
                return candidate
    return None


def is_process_running(pid: int) -> bool:
    if pid <= 0:
        return False
    if os.name == "nt":
        try:
            import ctypes
            from ctypes import wintypes

            process_query_limited_information = 0x1000
            still_active = 259
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            handle = kernel32.OpenProcess(process_query_limited_information, False, pid)
            if not handle:
                return False
            try:
                exit_code = wintypes.DWORD()
                if not kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
                    return False
                return exit_code.value == still_active
            finally:
                kernel32.CloseHandle(handle)
        except Exception:
            return False
    try:
        os.kill(pid, 0)
        return True
    except PermissionError:
        return True
    except OSError:
        return False


def process_identity(pid: int) -> dict[str, str]:
    if pid <= 0 or not is_process_running(pid):
        return {}
    if os.name == "nt":
        data = _first_powershell_record(
            _run_powershell_json(
                (
                    "$result = @(Get-CimInstance Win32_Process "
                    f"-Filter 'ProcessId = {pid}' | "
                    "Select-Object ProcessId,CreationDate,CommandLine)"
                )
            )
        )
        if data:
            process_id = _strict_process_id(data.get("ProcessId"))
            if process_id == pid:
                return {
                    "pid": str(process_id),
                    "creation_date": str(data.get("CreationDate", "")),
                    "command_line": str(data.get("CommandLine", "")),
                }

        data = _first_powershell_record(
            _run_powershell_json(
                (
                    f"$result = @(Get-Process -Id {pid} -ErrorAction SilentlyContinue | "
                    "Select-Object Id,StartTime,Path)"
                )
            )
        )
        if data:
            process_id = _strict_process_id(data.get("Id"))
            if process_id == pid:
                return {
                    "pid": str(process_id),
                    "creation_date": str(data.get("StartTime", "")),
                    "command_line": str(data.get("Path", "")),
                }
        return {}
    try:
        stat_path = Path(f"/proc/{pid}/stat")
        cmdline_path = Path(f"/proc/{pid}/cmdline")
        return {
            "pid": str(pid),
            "stat": stat_path.read_text(encoding="utf-8", errors="replace"),
            "command_line": cmdline_path.read_text(encoding="utf-8", errors="replace"),
        }
    except Exception:
        return {"pid": str(pid)}


def _powershell_executable() -> str:
    system_root = os.environ.get("SystemRoot") or os.environ.get("WINDIR")
    if system_root:
        candidate = Path(system_root) / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe"
        if candidate.exists():
            return str(candidate)
    return "powershell.exe"


def _run_powershell_json(result_script: str, timeout_seconds: float = 5) -> Any:
    encoding_script = (
        "$ErrorActionPreference = 'Stop'; "
        f"{result_script}; "
        "if ($null -ne $result) { "
        "$json = ConvertTo-Json -InputObject $result -Compress -Depth 4; "
        "$bytes = [Text.Encoding]::UTF8.GetBytes([string]$json); "
        "[Console]::Out.Write([Convert]::ToBase64String($bytes)) "
        "}"
    )
    try:
        completed = subprocess.run(
            [
                _powershell_executable(),
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                encoding_script,
            ],
            capture_output=True,
            timeout=timeout_seconds,
            check=False,
            **_hidden_subprocess_kwargs(),
        )
        if getattr(completed, "returncode", 0) != 0 or not completed.stdout:
            return None
        encoded = completed.stdout
        if isinstance(encoded, str):
            encoded = encoded.encode("ascii", errors="strict")
        json_bytes = base64.b64decode(encoded.strip(), validate=True)
        return json.loads(json_bytes.decode("utf-8", errors="strict"))
    except (OSError, subprocess.SubprocessError, UnicodeError, ValueError):
        return None


def _first_powershell_record(value: Any) -> dict[str, Any] | None:
    if isinstance(value, dict):
        return value
    if isinstance(value, list) and value and isinstance(value[0], dict):
        return value[0]
    return None


def _strict_process_id(value: Any, *, allow_zero: bool = False) -> int | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        result = value
    elif isinstance(value, str) and value.isascii() and value.isdigit():
        result = int(value)
    else:
        return None
    if result < 0 or (result == 0 and not allow_zero):
        return None
    return result


def process_matches_state(pid: int, state: dict[str, Any]) -> bool:
    expected = state.get("process_identity") or {}
    if not expected:
        return False
    current = process_identity(pid)
    if not current:
        return False
    matched_strong_identity = False
    for key in ("creation_date", "stat"):
        if expected.get(key) and current.get(key) != expected.get(key):
            return False
        if expected.get(key):
            matched_strong_identity = True
    expected_command = expected.get("command_line")
    current_command = current.get("command_line")
    if not matched_strong_identity and expected_command:
        if not current_command or current_command != expected_command:
            return False
        return True
    return matched_strong_identity


def process_tree_pids(pid: int) -> list[int]:
    if pid <= 0 or not is_process_running(pid):
        return []
    if os.name != "nt":
        return [pid]

    data = _run_powershell_json(
        "$result = @(Get-CimInstance Win32_Process | "
        "Select-Object ProcessId,ParentProcessId)"
    )
    records = data if isinstance(data, list) else [data]
    children_by_parent: dict[int, list[int]] = {}
    for record in records:
        if not isinstance(record, dict):
            continue
        process_id = _strict_process_id(record.get("ProcessId"))
        parent_id = _strict_process_id(record.get("ParentProcessId"), allow_zero=True)
        if process_id is None or parent_id is None:
            continue
        children_by_parent.setdefault(parent_id, []).append(process_id)

    result: list[int] = []
    pending = [pid]
    seen: set[int] = set()
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        result.append(current)
        pending.extend(reversed(children_by_parent.get(current, [])))
    return result


def _service_creationflags() -> int:
    return service_creationflags()


def _hidden_subprocess_kwargs() -> dict[str, int]:
    return hidden_subprocess_kwargs()


def terminate_process_tree(pid: int, force: bool = False) -> None:
    if not is_process_running(pid):
        return

    if os.name == "nt":
        command = ["taskkill", "/PID", str(pid), "/T"]
        if force:
            command.append("/F")
        subprocess.run(
            command,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=30,
            check=False,
            **_hidden_subprocess_kwargs(),
        )
        return

    try:
        os.killpg(pid, 9 if force else 15)
    except OSError:
        try:
            os.kill(pid, 9 if force else 15)
        except OSError:
            return
