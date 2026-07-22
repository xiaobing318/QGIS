"""Start, stop and inspect QCopilots MCP service process trees.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import json
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


class ProcessController:
    def __init__(
        self,
        state_root: str | Path | ProcessCommand | None = None,
        runtime: UvRuntime | None = None,
        service_id: str | None = None,
        qgis_executable: str | Path | None = None,
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
        self._lock = threading.RLock()

    def status(self, manifest: ServiceManifest, deep: bool = True) -> ServiceStatus:
        with self._lock:
            state = self._read_state(manifest)
            pid = state.get("pid")
            port = int(state.get("port") or manifest.default_port)
            owner_match = bool(state.get("owner_token") and state.get("owner_token") == self.owner_token)
            running = bool(pid and is_process_running(int(pid)))
            if deep and running and state.get("process_identity") and not process_matches_state(int(pid), state):
                running = False
            if deep and running and not owner_match and not process_matches_state(int(pid), state):
                running = False
            process_tree = process_tree_pids(int(pid)) if deep and running and pid else []
            configuration_match = _service_configuration_matches_state(manifest, state)
            health = "stopped"
            if running:
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
                log_file=str(service_log_file(manifest.service_id)),
                health=health,
                process_tree=process_tree,
                owner_match=owner_match,
                runtime_python=str(state.get("runtime_python", "")),
            )

    def start(
        self,
        manifest: ServiceManifest | None = None,
        bridge_url: str | None = None,
        extra_env: dict[str, str] | None = None,
    ) -> ServiceStatus | None:
        with self._lock:
            if manifest is None:
                self._start_command()
                return None

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
            service_python = resolve_service_python_executable(self.qgis_executable)
            if hasattr(self.runtime, "service_command"):
                command = self.runtime.service_command(manifest, service_python)
                runtime_python = service_python
            else:
                runtime_python = self.runtime.ensure_runtime(manifest, str(service_python))
                command = [str(runtime_python), str(manifest.entry_path)]

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

            self.process = process
            self._manifest_processes[manifest.service_id] = process
            self.exit_code = None
            state = {
                "pid": process.pid,
                "port": port,
                "url": manifest.url(port),
                "log_file": str(log_file),
                "started_at": time.time(),
                "manifest": manifest_to_dict(manifest, port),
                "command": command,
                "runtime_python": str(runtime_python),
                "service_python": str(service_python),
                "owner_token": self.owner_token,
                "manager_pid": os.getpid(),
                "process_identity": process_identity(process.pid),
            }
            self._write_state(manifest, state)
            for _ in range(20):
                status = self.status(manifest)
                if status.health == "ok":
                    return status
                time.sleep(0.25)
            return self.status(manifest)

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
            text=True,
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
        self._manifest_processes.pop(manifest.service_id, None)

    def _close_log_handle(self) -> None:
        if self._log_handle:
            self._log_handle.close()
            self._log_handle = None

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
        try:
            completed = subprocess.run(
                [
                    _powershell_executable(),
                    "-NoProfile",
                    "-Command",
                    (
                        "$p = Get-CimInstance Win32_Process -Filter \"ProcessId = "
                        f"{pid}\"; "
                        "if ($p) { "
                        "$p | Select-Object ProcessId,CreationDate,CommandLine | ConvertTo-Json -Compress "
                        "}"
                    ),
                ],
                text=True,
                capture_output=True,
                timeout=5,
                check=False,
                **_hidden_subprocess_kwargs(),
            )
            if completed.stdout.strip():
                data = json.loads(completed.stdout)
                return {
                    "pid": str(data.get("ProcessId", pid)),
                    "creation_date": str(data.get("CreationDate", "")),
                    "command_line": str(data.get("CommandLine", "")),
                }
        except Exception:
            pass
        try:
            completed = subprocess.run(
                [
                    _powershell_executable(),
                    "-NoProfile",
                    "-Command",
                    (
                        f"$p = Get-Process -Id {pid} -ErrorAction SilentlyContinue; "
                        "if ($p) { "
                        "$p | Select-Object Id,StartTime,Path | ConvertTo-Json -Compress "
                        "}"
                    ),
                ],
                text=True,
                capture_output=True,
                timeout=5,
                check=False,
                **_hidden_subprocess_kwargs(),
            )
            if completed.stdout.strip():
                data = json.loads(completed.stdout)
                return {
                    "pid": str(data.get("Id", pid)),
                    "creation_date": str(data.get("StartTime", "")),
                    "command_line": str(data.get("Path", "")),
                }
        except Exception:
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

    children: list[int] = []
    try:
        completed = subprocess.run(
            ["wmic", "process", "where", f"(ParentProcessId={pid})", "get", "ProcessId", "/VALUE"],
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
            **_hidden_subprocess_kwargs(),
        )
        for line in completed.stdout.splitlines():
            if line.startswith("ProcessId="):
                value = line.split("=", 1)[1].strip()
                if value.isdigit():
                    children.append(int(value))
    except Exception:
        children = []
    result = [pid]
    for child in children:
        result.extend(process_tree_pids(child))
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
