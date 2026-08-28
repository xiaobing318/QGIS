"""Launcher-based live acceptance runner for all public QCopilots MCP tools.

Run this script only after QGIS has been started with ``Launcher.ps1`` and all
seven configured MCP services report healthy.  The runner reads the shared
Bearer token from the current user's manager configuration.  It never prints
the token, accepts no token command-line argument, and redacts it from errors.

The acceptance owns only resources created below its unique temporary root.
Every output is registered before the corresponding tool call.  Processing
and binary jobs, QGIS layers, MCP sessions, and the temporary root are cleaned
in ``finally`` paths.  The final layout and project file name are explicitly
registered as requiring the dedicated QGIS process to exit.  Incomplete
runner-owned cleanup is itself an acceptance failure.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
import tempfile
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable
from urllib.error import HTTPError, URLError
from urllib.request import (
    HTTPRedirectHandler,
    ProxyHandler,
    Request,
    build_opener,
)


MCP_PROTOCOL_VERSION = "2025-06-18"
MANAGER_CONFIG_FILENAME = "qcopilots_manager_config.json"
TOKEN_PATTERN = re.compile(r"^[A-Za-z0-9_-]{32,256}$")
TERMINAL_JOB_STATES = frozenset({"succeeded", "failed", "cancelled"})


class _RejectRedirectHandler(HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, new_url):
        del request, message
        raise HTTPError(
            new_url,
            code,
            "Loopback MCP redirects are not permitted",
            headers,
            fp,
        )


_LOOPBACK_HTTP_OPENER = build_opener(ProxyHandler({}), _RejectRedirectHandler())


def loopback_urlopen(request: Request, *, timeout: float):
    return _LOOPBACK_HTTP_OPENER.open(request, timeout=timeout)


@dataclass(frozen=True)
class ServiceSpec:
    key: str
    service_id: str
    port: int


SERVICES = (
    ServiceSpec(
        "qcopilots_mcp_server_builtin_tools",
        "qcopilots.mcp_server_builtin_tools",
        48211,
    ),
    ServiceSpec(
        "qcopilots_mcp_server_skills",
        "qcopilots.mcp_server_skills",
        48212,
    ),
    ServiceSpec(
        "qcopilots_mcp_server_interactive_tools",
        "qcopilots.mcp_server_interactive_tools",
        48213,
    ),
    ServiceSpec(
        "qcopilots_mcp_server_processing_vector",
        "qcopilots.mcp_server_processing_vector",
        48214,
    ),
    ServiceSpec(
        "qcopilots_mcp_server_processing_raster",
        "qcopilots.mcp_server_processing_raster",
        48215,
    ),
    ServiceSpec(
        "qcopilots_mcp_server_qgis_binary",
        "qcopilots.mcp_server_qgis_binary",
        48216,
    ),
    ServiceSpec(
        "qcopilots_mcp_server_processing_general",
        "qcopilots.mcp_server_processing_general",
        48217,
    ),
)

EXPECTED_TOOLS = {
    "qcopilots_mcp_server_builtin_tools": (
        "read_file",
        "get_file_metadata",
        "copy_file",
        "file_glob_search",
        "grep_search",
        "exec_shell_command",
        "write_file",
        "edit_file",
        "get_datetime",
    ),
    "qcopilots_mcp_server_skills": (
        "list_skills",
        "read_skill",
        "list_skill_resources",
        "read_skill_resource",
        "qgis-skills-creator",
    ),
    "qcopilots_mcp_server_interactive_tools": (
        "list_layers",
        "set_layer_visibility",
        "zoom_to_layer",
        "zoom_to_extent",
        "zoom_in",
        "zoom_out",
        "zoom_full",
        "zoom_to_selection",
        "zoom_to_native_resolution",
        "zoom_to_last_extent",
        "zoom_to_next_extent",
        "save_project",
        "refresh_canvas",
        "export_map_image",
        "describe_layer_sources",
        "list_map_layers",
        "load_map_layer",
        "remove_map_layers",
        "get_layer_metadata",
        "query_vector_features",
        "set_vector_selection",
        "delete_vector_features",
        "get_project_crs",
        "set_project_crs",
        "get_raster_statistics",
        "apply_layer_style",
        "configure_vector_labels",
        "create_print_layout",
        "list_print_layouts",
        "export_print_layout",
        "create_vector_layer",
        "add_vector_features",
        "update_vector_features",
    ),
    "qcopilots_mcp_server_processing_vector": (
        "list_vector_processing_algorithms",
        "get_vector_processing_algorithm_details",
        "start_vector_processing_algorithm",
        "get_vector_processing_job",
        "list_vector_processing_jobs",
        "cancel_vector_processing_job",
    ),
    "qcopilots_mcp_server_processing_raster": (
        "list_raster_processing_algorithms",
        "get_raster_processing_algorithm_details",
        "start_raster_processing_algorithm",
        "get_raster_processing_job",
        "list_raster_processing_jobs",
        "cancel_raster_processing_job",
    ),
    "qcopilots_mcp_server_qgis_binary": (
        "list_qgis_binaries",
        "get_qgis_binary_details",
        "start_qgis_binary",
        "get_qgis_binary_job",
        "list_qgis_binary_jobs",
        "cancel_qgis_binary_job",
    ),
    "qcopilots_mcp_server_processing_general": (
        "list_general_processing_algorithms",
        "get_general_processing_algorithm_details",
        "start_general_processing_algorithm",
        "get_general_processing_job",
        "list_general_processing_jobs",
        "cancel_general_processing_job",
    ),
}


class AcceptanceFailure(RuntimeError):
    """A deterministic live acceptance or cleanup failure."""


@dataclass(frozen=True, repr=False)
class ManagerAcceptanceConfig:
    auth_token: str
    security_mode: str
    shell_enabled: bool


def validate_expected_inventory() -> None:
    if tuple(service.port for service in SERVICES) != tuple(range(48211, 48218)):
        raise AcceptanceFailure("Service ports must be exactly 48211 through 48217")
    if set(EXPECTED_TOOLS) != {service.key for service in SERVICES}:
        raise AcceptanceFailure("Expected tool inventory does not match service inventory")
    names = [name for tools in EXPECTED_TOOLS.values() for name in tools]
    if len(names) != 71 or len(set(names)) != 71:
        raise AcceptanceFailure(
            "Expected tool inventory must contain exactly 71 unique tool names"
        )


def default_manager_config_path(environ: dict[str, str] | None = None) -> Path:
    environment = os.environ if environ is None else environ
    configured_home = str(environment.get("QCOPILOTS_HOME") or "").strip()
    if configured_home:
        return Path(configured_home).expanduser().resolve() / MANAGER_CONFIG_FILENAME
    appdata = str(environment.get("APPDATA") or "").strip()
    if appdata:
        return Path(appdata) / "QGIS" / "QCopilots" / MANAGER_CONFIG_FILENAME
    return Path.home() / ".qcopilots" / MANAGER_CONFIG_FILENAME


def load_manager_acceptance_config(
    config_path: str | Path | None = None,
) -> ManagerAcceptanceConfig:
    path = (
        Path(config_path).expanduser().resolve()
        if config_path is not None
        else default_manager_config_path()
    )
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as err:
        raise AcceptanceFailure(f"Manager user configuration is missing: {path}") from err
    except (OSError, UnicodeError, json.JSONDecodeError) as err:
        raise AcceptanceFailure(
            f"Manager user configuration could not be read: {path}"
        ) from err
    try:
        token = document["browser_access"]["auth_token"]
    except (KeyError, TypeError) as err:
        raise AcceptanceFailure(
            "Manager user configuration has no browser_access.auth_token"
        ) from err
    if not isinstance(token, str) or TOKEN_PATTERN.fullmatch(token) is None:
        raise AcceptanceFailure(
            "Manager browser access token is absent or invalid. Start QGIS once "
            "so the manager can persist a valid token."
        )
    security_policy = document.get("security_policy")
    if not isinstance(security_policy, dict):
        raise AcceptanceFailure(
            "Manager user configuration has no valid security_policy"
        )
    security_mode = security_policy.get("mode")
    shell = security_policy.get("shell")
    if security_mode not in {"compatible", "formal_restricted"}:
        raise AcceptanceFailure(
            "Manager security_policy.mode is invalid for live acceptance"
        )
    if not isinstance(shell, dict) or not isinstance(shell.get("enabled"), bool):
        raise AcceptanceFailure(
            "Manager security_policy.shell.enabled is invalid for live acceptance"
        )
    return ManagerAcceptanceConfig(
        auth_token=token,
        security_mode=security_mode,
        shell_enabled=bool(shell["enabled"]),
    )


def load_manager_auth_token(config_path: str | Path | None = None) -> str:
    return load_manager_acceptance_config(config_path).auth_token


def _redacted_message(value: Any, token: str = "") -> str:
    message = str(value)
    if token:
        message = message.replace(token, "<redacted>")
    return message


class CoverageLedger:
    def __init__(self) -> None:
        self._calls = {key: set() for key in EXPECTED_TOOLS}

    def mark(self, service_key: str, tool_name: str) -> None:
        if service_key not in self._calls:
            raise AcceptanceFailure(f"Unknown service coverage key: {service_key}")
        if tool_name not in EXPECTED_TOOLS[service_key]:
            raise AcceptanceFailure(
                f"Unexpected tool call for {service_key}: {tool_name}"
            )
        self._calls[service_key].add(tool_name)

    def assert_service_complete(self, service_key: str) -> None:
        missing = set(EXPECTED_TOOLS[service_key]) - self._calls[service_key]
        if missing:
            raise AcceptanceFailure(
                f"{service_key} did not call tools: {', '.join(sorted(missing))}"
            )

    def assert_all_complete(self) -> None:
        for service_key in EXPECTED_TOOLS:
            self.assert_service_complete(service_key)
        if sum(len(calls) for calls in self._calls.values()) != 71:
            raise AcceptanceFailure("Live tool coverage did not reach exactly 71 tools")


class McpSession:
    """One authenticated MCP lifecycle session with secret-safe failures."""

    def __init__(
        self,
        service: ServiceSpec,
        token: str,
        *,
        timeout_seconds: float,
        ledger: CoverageLedger | None = None,
        opener: Callable[..., Any] = loopback_urlopen,
    ):
        self.service = service
        self._token = token
        self.timeout_seconds = timeout_seconds
        self.ledger = ledger
        self._opener = opener
        self.session_id = ""
        self._initialized = False
        self._listed = False
        self._request_id = 0

    @property
    def endpoint(self) -> str:
        return f"http://127.0.0.1:{self.service.port}/mcp"

    def __repr__(self) -> str:
        return (
            f"McpSession(service={self.service.service_id!r}, "
            f"port={self.service.port}, authenticated=True)"
        )

    def __enter__(self) -> "McpSession":
        self.start()
        return self

    def __exit__(self, exc_type, exc, traceback) -> bool:
        del exc_type, traceback
        try:
            self.close()
        except Exception as cleanup_err:
            cleanup_message = _redacted_message(cleanup_err, self._token)
            if exc is not None:
                raise AcceptanceFailure(
                    f"{_redacted_message(exc, self._token)}. "
                    f"MCP session cleanup also failed: {cleanup_message}"
                ) from exc
            raise AcceptanceFailure(cleanup_message) from cleanup_err
        return False

    def start(self) -> None:
        if self.session_id:
            raise AcceptanceFailure("MCP session was initialized more than once")
        result, headers, status = self._post_rpc(
            "initialize",
            {
                "protocolVersion": MCP_PROTOCOL_VERSION,
                "capabilities": {},
                "clientInfo": {
                    "name": "qcopilots-live-acceptance",
                    "version": "1.0",
                },
            },
            include_session=False,
        )
        session_id = str(headers.get("Mcp-Session-Id") or "").strip()
        if not session_id:
            raise AcceptanceFailure(
                f"{self.service.service_id} initialize returned no MCP session id"
            )
        self.session_id = session_id
        try:
            if status != 200:
                raise AcceptanceFailure(
                    f"{self.service.service_id} initialize returned HTTP {status}"
                )
            if result.get("protocolVersion") != MCP_PROTOCOL_VERSION:
                raise AcceptanceFailure(
                    f"{self.service.service_id} negotiated an unexpected "
                    "protocol version"
                )
            status, body = self._notification("notifications/initialized")
            if status != 202 or body:
                raise AcceptanceFailure(
                    f"{self.service.service_id} initialized notification must "
                    "return an empty HTTP 202 response"
                )
            self._initialized = True
        except Exception as err:
            try:
                self.close()
            except Exception as cleanup_err:
                raise AcceptanceFailure(
                    f"{_redacted_message(err, self._token)}. Partial MCP session "
                    "cleanup also failed: "
                    f"{_redacted_message(cleanup_err, self._token)}"
                ) from err
            raise

    def list_tools(self) -> list[dict[str, Any]]:
        if not self._initialized:
            raise AcceptanceFailure("tools/list requires an initialized MCP session")
        tools: list[dict[str, Any]] = []
        cursor = ""
        while True:
            params = {"cursor": cursor} if cursor else {}
            result, _headers, status = self._post_rpc("tools/list", params)
            if status != 200:
                raise AcceptanceFailure(
                    f"{self.service.service_id} tools/list returned HTTP {status}"
                )
            page = result.get("tools")
            if not isinstance(page, list) or any(not isinstance(item, dict) for item in page):
                raise AcceptanceFailure(
                    f"{self.service.service_id} tools/list returned an invalid page"
                )
            tools.extend(page)
            next_cursor = result.get("nextCursor")
            if not next_cursor:
                break
            if not isinstance(next_cursor, str) or next_cursor == cursor:
                raise AcceptanceFailure(
                    f"{self.service.service_id} tools/list returned an invalid cursor"
                )
            cursor = next_cursor
        self._listed = True
        return tools

    def call_tool(self, name: str, arguments: dict[str, Any]) -> dict[str, Any]:
        if not self._listed:
            raise AcceptanceFailure("tools/call requires tools/list first")
        result, _headers, status = self._post_rpc(
            "tools/call",
            {"name": name, "arguments": arguments},
        )
        if status != 200:
            raise AcceptanceFailure(
                f"{self.service.service_id} {name} returned HTTP {status}"
            )
        if not isinstance(result, dict):
            raise AcceptanceFailure(
                f"{self.service.service_id} {name} returned an invalid tool result"
            )
        if self.ledger is not None:
            self.ledger.mark(self.service.key, name)
        return result

    def close(self) -> None:
        if not self.session_id:
            return
        failures = []
        for _attempt in range(3):
            request = Request(
                self.endpoint,
                headers=self._headers(include_session=True),
                method="DELETE",
            )
            try:
                with self._opener(request, timeout=self.timeout_seconds) as response:
                    body = response.read()
                    status = int(response.status)
                if status == 204 and body == b"":
                    break
                failures.append(
                    f"HTTP {status} with {'non-empty' if body else 'empty'} body"
                )
            except Exception as err:
                failures.append(_redacted_message(err, self._token))
        else:
            raise AcceptanceFailure(
                f"{self.service.service_id} session DELETE failed after three "
                f"attempts: {failures[-1] if failures else 'unknown failure'}"
            ) from None
        if status == 204 and body == b"":
            self.session_id = ""
            self._initialized = False
            self._listed = False

    def _post_rpc(
        self,
        method: str,
        params: dict[str, Any],
        *,
        include_session: bool = True,
    ) -> tuple[dict[str, Any], Any, int]:
        self._request_id += 1
        payload = {
            "jsonrpc": "2.0",
            "id": f"live-{self.service.port}-{self._request_id}",
            "method": method,
            "params": params,
        }
        status, headers, body = self._request(
            payload,
            include_session=include_session,
        )
        if not isinstance(body, dict):
            raise AcceptanceFailure(
                f"{self.service.service_id} {method} returned no JSON-RPC object"
            )
        if "error" in body:
            error = body.get("error")
            if isinstance(error, dict):
                code = error.get("code")
                message = _redacted_message(error.get("message", ""), self._token)
            else:
                code = "unknown"
                message = _redacted_message(error, self._token)
            raise AcceptanceFailure(
                f"{self.service.service_id} {method} returned JSON-RPC error "
                f"{code}: {message}"
            )
        result = body.get("result")
        if not isinstance(result, dict):
            raise AcceptanceFailure(
                f"{self.service.service_id} {method} returned no result object"
            )
        return result, headers, status

    def _notification(self, method: str) -> tuple[int, bytes]:
        payload = {"jsonrpc": "2.0", "method": method}
        status, _headers, body = self._request(payload, include_session=True)
        if body is None:
            return status, b""
        raise AcceptanceFailure(
            f"{self.service.service_id} {method} returned an unexpected body"
        )

    def _request(
        self,
        payload: dict[str, Any],
        *,
        include_session: bool,
    ) -> tuple[int, Any, dict[str, Any] | None]:
        request = Request(
            self.endpoint,
            data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
            headers=self._headers(include_session=include_session),
            method="POST",
        )
        failure_message = ""
        try:
            with self._opener(request, timeout=self.timeout_seconds) as response:
                raw = response.read()
                status = int(response.status)
                headers = response.headers
        except HTTPError as err:
            try:
                detail = err.read(65536).decode("utf-8", errors="replace")
            except Exception:
                detail = ""
            detail = _redacted_message(detail, self._token).strip()
            suffix = f": {detail}" if detail else ""
            failure_message = (
                f"{self.service.service_id} HTTP {err.code}{suffix}"
            )
        except (URLError, OSError, TimeoutError) as err:
            failure_message = (
                f"{self.service.service_id} request failed: "
                f"{_redacted_message(err, self._token)}"
            )
        if failure_message:
            raise AcceptanceFailure(failure_message)
        if not raw:
            return status, headers, None
        try:
            body = json.loads(raw.decode("utf-8"))
        except (UnicodeError, json.JSONDecodeError) as err:
            raise AcceptanceFailure(
                f"{self.service.service_id} returned invalid JSON"
            ) from err
        return status, headers, body

    def _headers(self, *, include_session: bool) -> dict[str, str]:
        headers = {
            "Accept": "application/json, text/event-stream",
            "Authorization": f"Bearer {self._token}",
            "Content-Type": "application/json",
        }
        if include_session and self.session_id:
            headers.update(
                {
                    "MCP-Protocol-Version": MCP_PROTOCOL_VERSION,
                    "MCP-Session-Id": self.session_id,
                }
            )
        return headers


def require_success(result: dict[str, Any], label: str) -> dict[str, Any]:
    is_error = result.get("isError", False)
    if not isinstance(is_error, bool):
        raise AcceptanceFailure(f"{label} returned a non-boolean isError")
    if is_error:
        raise AcceptanceFailure(f"{label} unexpectedly returned isError=true")
    structured = result.get("structuredContent")
    if isinstance(structured, dict):
        return structured
    for item in result.get("content") or []:
        if not isinstance(item, dict) or item.get("type") != "text":
            continue
        text = item.get("text")
        if not isinstance(text, str):
            continue
        try:
            parsed = json.loads(text)
        except json.JSONDecodeError:
            continue
        if isinstance(parsed, dict):
            return parsed
    return {}


def require_business_error(
    result: dict[str, Any],
    label: str,
    expected_fragments: tuple[str, ...],
) -> str:
    if result.get("isError") is not True:
        raise AcceptanceFailure(f"{label} must return isError=true")
    texts = [
        str(item.get("text") or "")
        for item in result.get("content") or []
        if isinstance(item, dict) and item.get("type") == "text"
    ]
    message = "\n".join(texts).strip()
    if not message:
        raise AcceptanceFailure(f"{label} returned isError=true without an error message")
    lowered = message.casefold()
    if expected_fragments and not any(
        fragment.casefold() in lowered for fragment in expected_fragments
    ):
        raise AcceptanceFailure(f"{label} returned an unexpected business rejection")
    return message


def require_cleanup_complete(payload: dict[str, Any], label: str) -> None:
    cleanup = payload.get("cleanup")
    if cleanup is None:
        return
    if not isinstance(cleanup, dict) or cleanup.get("complete") is not True:
        raise AcceptanceFailure(f"{label} reported incomplete cleanup")


@dataclass
class JobRegistration:
    service: ServiceSpec
    get_tool: str
    cancel_tool: str
    list_tool: str
    client_request_id: str
    job_id: str = ""


class CleanupRegistry:
    """Track acceptance-owned resources before they can acquire side effects."""

    RUN_ROOT_PREFIX = "qcopilots-mcp-live-acceptance-"
    OWNER_MARKER_NAME = ".qcopilots-mcp-live-acceptance-owner"

    def __init__(
        self,
        root: Path,
        workspace_base: Path,
        *,
        _ownership_token: str | None = None,
    ):
        requested_root = Path(root)
        requested_base = Path(workspace_base)
        if not requested_root.is_absolute() or not requested_base.is_absolute():
            raise AcceptanceFailure(
                "Acceptance workspace base and run root must be absolute paths"
            )
        self.workspace_base = requested_base.resolve(strict=True)
        self.root = requested_root.resolve(strict=True)
        if not self.workspace_base.is_dir():
            raise AcceptanceFailure(
                f"Acceptance workspace base must be a directory: {self.workspace_base}"
            )
        if not self.root.is_dir():
            raise AcceptanceFailure(
                f"Acceptance run root must be a directory: {self.root}"
            )
        if (
            self.root == self.workspace_base
            or self.root.parent != self.workspace_base
            or not self.root.name.startswith(self.RUN_ROOT_PREFIX)
        ):
            raise AcceptanceFailure(
                "Acceptance run root must be an owned direct child of the workspace base"
            )
        ownership_token = str(_ownership_token or "")
        owner_marker = self.root / self.OWNER_MARKER_NAME
        try:
            marker_token = (
                owner_marker.read_text(encoding="utf-8")
                if not owner_marker.is_symlink() and owner_marker.is_file()
                else ""
            )
        except OSError as err:
            raise AcceptanceFailure(
                "Acceptance run root ownership marker could not be read"
            ) from err
        if not ownership_token or marker_token != ownership_token:
            raise AcceptanceFailure(
                "Acceptance run root must be created through CleanupRegistry.create"
            )
        root_stat = self.root.stat()
        self._root_identity = (root_stat.st_dev, root_stat.st_ino)
        self._ownership_token = ownership_token
        self._owner_marker = owner_marker
        self.paths: set[Path] = {self.root}
        self.layer_ids: set[str] = set()
        self.layer_names: set[str] = set()
        self.jobs: list[JobRegistration] = []
        self.qgis_touched = False
        self.original_crs_selector = ""
        self.original_crs_identity = ""
        self.requires_process_exit: dict[str, str] = {}

    @classmethod
    def create(
        cls,
        workspace_base: str | Path | None = None,
    ) -> "CleanupRegistry":
        if workspace_base is None:
            root = Path(tempfile.mkdtemp(prefix=cls.RUN_ROOT_PREFIX))
            base = root.resolve(strict=True).parent
        else:
            requested_base = Path(workspace_base)
            if not requested_base.is_absolute():
                raise AcceptanceFailure(
                    "Acceptance workspace base must be an absolute path"
                )
            try:
                base = requested_base.resolve(strict=True)
            except (OSError, RuntimeError) as err:
                raise AcceptanceFailure(
                    f"Acceptance workspace base does not exist: {requested_base}"
                ) from err
            if not base.is_dir():
                raise AcceptanceFailure(
                    f"Acceptance workspace base must be a directory: {base}"
                )
            try:
                root = Path(
                    tempfile.mkdtemp(
                        prefix=cls.RUN_ROOT_PREFIX,
                        dir=str(base),
                    )
                )
            except OSError as err:
                raise AcceptanceFailure(
                    f"Acceptance run root could not be created under: {base}"
                ) from err
        ownership_token = uuid.uuid4().hex
        owner_marker = root / cls.OWNER_MARKER_NAME
        root_stat = root.stat()
        created_identity = (root_stat.st_dev, root_stat.st_ino)
        try:
            with owner_marker.open("x", encoding="utf-8") as marker_file:
                marker_file.write(ownership_token)
            return cls(
                root,
                base,
                _ownership_token=ownership_token,
            )
        except Exception as err:
            try:
                current_stat = root.stat()
                current_identity = (current_stat.st_dev, current_stat.st_ino)
                if (
                    current_identity == created_identity
                    and not root.is_symlink()
                    and root.parent == base
                    and root.name.startswith(cls.RUN_ROOT_PREFIX)
                ):
                    shutil.rmtree(root)
            except Exception:
                pass
            if isinstance(err, AcceptanceFailure):
                raise
            raise AcceptanceFailure(
                "Acceptance run root ownership could not be established"
            ) from err

    def reserve_path(self, relative_name: str) -> Path:
        candidate = (self.root / relative_name).resolve(strict=False)
        try:
            candidate.relative_to(self.root)
        except ValueError as err:
            raise AcceptanceFailure(
                f"Temporary resource escapes the acceptance root: {relative_name}"
            ) from err
        self.paths.add(candidate)
        return candidate

    def reserve_output_path(self, relative_name: str) -> Path:
        candidate = self.reserve_path(relative_name)
        candidate.parent.mkdir(parents=True, exist_ok=True)
        return candidate

    def register_layer(self, layer_id: str) -> None:
        value = str(layer_id or "").strip()
        if not value:
            raise AcceptanceFailure("A created QGIS layer returned no layer id")
        self.layer_ids.add(value)
        self.qgis_touched = True

    def register_pending_layer_name(self, layer_name: str) -> None:
        value = str(layer_name or "").strip()
        if not value:
            raise AcceptanceFailure("A pending QGIS layer requires a name")
        self.layer_names.add(value)
        self.qgis_touched = True

    def mark_layers_removed(self, layer_ids: list[str] | set[str]) -> None:
        self.layer_ids.difference_update(str(value) for value in layer_ids)

    def mark_layer_names_removed(self, layer_names: list[str] | set[str]) -> None:
        self.layer_names.difference_update(str(value) for value in layer_names)

    def register_crs_restore(self, selector: str, identity: str) -> None:
        if not selector or not identity:
            raise AcceptanceFailure("A project CRS restore point must be valid")
        self.original_crs_selector = selector
        self.original_crs_identity = identity
        self.qgis_touched = True

    def clear_crs_restore(self) -> None:
        self.original_crs_selector = ""
        self.original_crs_identity = ""

    def register_job(self, registration: JobRegistration) -> None:
        self.jobs.append(registration)

    def unregister_job(self, registration: JobRegistration) -> None:
        if registration in self.jobs:
            self.jobs.remove(registration)

    def register_requires_process_exit(self, kind: str, identity: str) -> None:
        resource_kind = str(kind or "").strip()
        resource_identity = str(identity or "").strip()
        if not resource_kind or not resource_identity:
            raise AcceptanceFailure(
                "A process-exit resource requires a kind and identity"
            )
        existing = self.requires_process_exit.get(resource_kind)
        if existing is not None and existing != resource_identity:
            raise AcceptanceFailure(
                f"Process-exit resource kind was registered twice: {resource_kind}"
            )
        self.requires_process_exit[resource_kind] = resource_identity

    def cleanup_filesystem(self) -> list[str]:
        errors = []
        if (
            self.root == self.workspace_base
            or self.root.parent != self.workspace_base
            or not self.root.name.startswith(self.RUN_ROOT_PREFIX)
        ):
            return [
                "Temporary root removal refused because the run root is not an "
                "owned direct child of the workspace base"
            ]
        if not self.root.exists():
            return []
        try:
            root_stat = self.root.stat()
            current_identity = (root_stat.st_dev, root_stat.st_ino)
            marker_token = (
                self._owner_marker.read_text(encoding="utf-8")
                if not self.root.is_symlink()
                and not self._owner_marker.is_symlink()
                and self._owner_marker.is_file()
                else ""
            )
        except OSError as err:
            return [f"Temporary root ownership validation failed: {err}"]
        if (
            current_identity != self._root_identity
            or marker_token != self._ownership_token
        ):
            return [
                "Temporary root removal refused because ownership validation failed"
            ]
        try:
            if self.root.exists():
                shutil.rmtree(self.root)
        except Exception as err:
            errors.append(f"Temporary root removal failed: {err}")
        residuals = sorted(str(path) for path in self.paths if path.exists())
        if residuals:
            errors.append("Temporary resources remain: " + ", ".join(residuals))
        return errors


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
TEST_DATA_ROOT = REPOSITORY_ROOT / "tests" / "testdata"


class AcceptanceRunner:
    def __init__(
        self,
        token: str,
        *,
        request_timeout_seconds: float = 60.0,
        job_timeout_seconds: float = 120.0,
        security_mode: str = "compatible",
        shell_enabled: bool = True,
        workspace_base: str | Path | None = None,
        output: Callable[[str], None] = print,
        opener: Callable[..., Any] = loopback_urlopen,
    ):
        self._token = token
        self.request_timeout_seconds = request_timeout_seconds
        self.job_timeout_seconds = job_timeout_seconds
        self.security_mode = security_mode
        self.shell_enabled = shell_enabled
        self.output = output
        self._opener = opener
        self.ledger = CoverageLedger()
        self.cleanup = CleanupRegistry.create(workspace_base)
        self._service_by_port = {service.port: service for service in SERVICES}

    def run(self) -> None:
        validate_expected_inventory()
        primary_error: Exception | None = None
        cleanup_errors: list[str] = []
        try:
            for service in SERVICES:
                self._exercise_service(service)
            self.ledger.assert_all_complete()
        except Exception as err:
            primary_error = err
        finally:
            cleanup_errors.extend(self._cleanup_registered_jobs())
            cleanup_errors.extend(self._cleanup_qgis_state())
            cleanup_errors.extend(self.cleanup.cleanup_filesystem())

        if primary_error is not None or cleanup_errors:
            messages = []
            if primary_error is not None:
                messages.append(_redacted_message(primary_error, self._token))
            messages.extend(_redacted_message(item, self._token) for item in cleanup_errors)
            raise AcceptanceFailure(". ".join(message for message in messages if message))
        self.output(
            "PASS: all 71 QCopilots MCP tools completed live acceptance, "
            f"requires_process_exit={bool(self.cleanup.requires_process_exit)}, "
            "process_exit_resource_count="
            f"{len(self.cleanup.requires_process_exit)}"
        )

    def _session(
        self,
        service: ServiceSpec,
        *,
        with_coverage: bool,
    ) -> McpSession:
        return McpSession(
            service,
            self._token,
            timeout_seconds=self.request_timeout_seconds,
            ledger=self.ledger if with_coverage else None,
            opener=self._opener,
        )

    def _exercise_service(self, service: ServiceSpec) -> None:
        with self._session(service, with_coverage=True) as session:
            descriptors = session.list_tools()
            actual_names = [str(item.get("name") or "") for item in descriptors]
            expected_names = list(EXPECTED_TOOLS[service.key])
            if actual_names != expected_names:
                raise AcceptanceFailure(
                    f"{service.service_id} tools/list differs from the exact expected "
                    f"inventory of {len(expected_names)} tools"
                )
            if service.key == "qcopilots_mcp_server_builtin_tools":
                self._exercise_builtin(session)
            elif service.key == "qcopilots_mcp_server_skills":
                self._exercise_skills(session)
            elif service.key == "qcopilots_mcp_server_interactive_tools":
                self._exercise_interactive(session)
            elif service.key == "qcopilots_mcp_server_qgis_binary":
                self._exercise_binary(session)
            elif "processing" in service.key:
                category = service.key.rsplit("_", 1)[-1]
                self._exercise_processing(session, category)
            else:
                raise AcceptanceFailure(f"No live scenario for {service.service_id}")
            self.ledger.assert_service_complete(service.key)
        self.output(
            f"PASS: {service.service_id} ({len(EXPECTED_TOOLS[service.key])} tools)"
        )

    def _success(
        self,
        session: McpSession,
        tool_name: str,
        arguments: dict[str, Any],
    ) -> dict[str, Any]:
        return require_success(
            session.call_tool(tool_name, arguments),
            f"{session.service.service_id}.{tool_name}",
        )

    def _business_error(
        self,
        session: McpSession,
        tool_name: str,
        arguments: dict[str, Any],
        expected_fragments: tuple[str, ...],
    ) -> str:
        return require_business_error(
            session.call_tool(tool_name, arguments),
            f"{session.service.service_id}.{tool_name}",
            expected_fragments,
        )

    def _exercise_builtin(self, session: McpSession) -> None:
        source = self.cleanup.reserve_path("builtin/source.txt")
        copied = self.cleanup.reserve_path("builtin/copied.txt")
        written = self.cleanup.reserve_path("builtin/written.txt")
        raster = self.cleanup.reserve_path("inputs/acceptance.asc")
        source_content = "alpha\nneedle\n"
        written_content = "created by live acceptance\n"
        raster_content = (
            "ncols 2\n"
            "nrows 2\n"
            "xllcorner 0\n"
            "yllcorner 0\n"
            "cellsize 1\n"
            "NODATA_value -9999\n"
            "1 2\n"
            "3 4\n"
        )

        source_write = self._success(
            session,
            "write_file",
            {"path": str(source), "content": source_content},
        )
        require_cleanup_complete(source_write, "write_file source")
        source_sha256 = hashlib.sha256(source_content.encode("utf-8")).hexdigest()
        if source_write.get("sha256") != source_sha256 or not source.is_file():
            raise AcceptanceFailure("write_file did not create the registered source")

        read = self._success(session, "read_file", {"path": str(source)})
        if read.get("content") != source_content:
            raise AcceptanceFailure("read_file returned unexpected source content")
        external_read = self._success(
            session,
            "read_file",
            {"path": str(Path(__file__).resolve()), "limit": 256},
        )
        if "Launcher-based live acceptance runner" not in str(
            external_read.get("content") or ""
        ):
            raise AcceptanceFailure(
                "read_file could not read the registered runner outside its service cwd"
            )
        metadata = self._success(
            session,
            "get_file_metadata",
            {"path": str(source)},
        )
        if metadata.get("sha256") != source_sha256:
            raise AcceptanceFailure("get_file_metadata returned an unexpected digest")
        copied_result = self._success(
            session,
            "copy_file",
            {
                "source": str(source),
                "target": str(copied),
                "expected_source_sha256": source_sha256,
            },
        )
        require_cleanup_complete(copied_result, "copy_file")
        if not copied.is_file() or copied.read_text(encoding="utf-8") != source_content:
            raise AcceptanceFailure("copy_file did not create an exact registered copy")

        globbed = self._success(
            session,
            "file_glob_search",
            {"path": str(self.cleanup.root), "include": "*.txt"},
        )
        if not globbed.get("matches"):
            raise AcceptanceFailure("file_glob_search found no acceptance text files")
        grepped = self._success(
            session,
            "grep_search",
            {"path": str(self.cleanup.root), "pattern": "needle"},
        )
        if not grepped.get("matches"):
            raise AcceptanceFailure("grep_search did not find its deterministic marker")

        shell_result = session.call_tool(
            "exec_shell_command",
            {
                "command": [sys.executable, "-c", "print('qcopilots-live-shell')"],
                "timeout_seconds": 15,
            },
        )
        if self.security_mode == "formal_restricted" and not self.shell_enabled:
            require_business_error(
                shell_result,
                f"{session.service.service_id}.exec_shell_command",
                ("shell execution is disabled by the security policy",),
            )
        elif shell_result.get("isError") is True:
            require_business_error(
                shell_result,
                f"{session.service.service_id}.exec_shell_command",
                (
                    "shell execution is disabled by the security policy",
                    "allowed shell executable",
                    "shell executable allowlist",
                ),
            )
        else:
            shell = require_success(
                shell_result,
                f"{session.service.service_id}.exec_shell_command",
            )
            if shell.get("exit_code") != 0 or "qcopilots-live-shell" not in str(
                shell.get("stdout") or ""
            ):
                raise AcceptanceFailure(
                    "exec_shell_command succeeded without its deterministic output"
                )

        written_result = self._success(
            session,
            "write_file",
            {"path": str(written), "content": written_content},
        )
        require_cleanup_complete(written_result, "write_file edit target")
        edited = self._success(
            session,
            "edit_file",
            {
                "path": str(written),
                "search": "created",
                "replace": "updated",
                "expected_sha256": written_result["sha256"],
            },
        )
        require_cleanup_complete(edited, "edit_file")
        if not edited.get("changed") or written.read_text(encoding="utf-8") != (
            "updated by live acceptance\n"
        ):
            raise AcceptanceFailure("edit_file did not apply its exact replacement")

        raster_write = self._success(
            session,
            "write_file",
            {"path": str(raster), "content": raster_content},
        )
        require_cleanup_complete(raster_write, "write_file raster fixture")
        if not raster.is_file():
            raise AcceptanceFailure("write_file did not create the raster fixture")
        current_time = self._success(
            session,
            "get_datetime",
            {"timezone": "UTC"},
        )
        if current_time.get("timezone") != "UTC" or not current_time.get("iso"):
            raise AcceptanceFailure("get_datetime did not return a UTC timestamp")

    def _exercise_skills(self, session: McpSession) -> None:
        selector = {"slug": "qgis-skills-creator"}
        listed = self._success(session, "list_skills", {})
        slugs = {
            str(item.get("slug") or "")
            for item in listed.get("skills") or []
            if isinstance(item, dict)
        }
        if "qgis-skills-creator" not in slugs:
            raise AcceptanceFailure("list_skills omitted qgis-skills-creator")
        skill = self._success(session, "read_skill", dict(selector))
        if skill.get("slug") != "qgis-skills-creator" or not skill.get("content"):
            raise AcceptanceFailure("read_skill returned incomplete live skill content")
        resources = self._success(
            session,
            "list_skill_resources",
            dict(selector),
        )
        resource_name = "references/qgis-tools.catalog.schema.json"
        if resource_name not in (resources.get("resources") or []):
            raise AcceptanceFailure("list_skill_resources omitted the catalog schema")
        resource = self._success(
            session,
            "read_skill_resource",
            {**selector, "resource": resource_name},
        )
        if resource.get("resource") != resource_name or not resource.get("content"):
            raise AcceptanceFailure("read_skill_resource returned no real resource content")
        invoked = self._success(
            session,
            "qgis-skills-creator",
            {"task": "Validate a deterministic QGIS skill creation plan."},
        )
        if invoked.get("skill") != "qgis-skills-creator" or not invoked.get(
            "skill_content"
        ):
            raise AcceptanceFailure("qgis-skills-creator returned no live skill payload")

    def _exercise_interactive(self, session: McpSession) -> None:
        vector_path = self.cleanup.reserve_path("interactive/acceptance.geojson")
        raster_path = self.cleanup.reserve_path("inputs/acceptance.asc")
        image_path = self.cleanup.reserve_path("interactive/map.png")
        world_path = self.cleanup.reserve_path("interactive/map.pgw")
        style_path = TEST_DATA_ROOT / "points_single_symbol.qml"
        if not raster_path.is_file():
            raise AcceptanceFailure("The registered interactive raster fixture is missing")
        if not style_path.is_file():
            raise AcceptanceFailure(f"QGIS style fixture is missing: {style_path}")

        unique_suffix = uuid.uuid4().hex
        vector_layer_name = f"QCopilots live points {unique_suffix}"
        raster_layer_name = f"QCopilots live raster {unique_suffix}"
        self._success(session, "list_layers", {})
        self.cleanup.register_pending_layer_name(vector_layer_name)
        created = self._success(
            session,
            "create_vector_layer",
            {
                "name": vector_layer_name,
                "geometry_type": "Point",
                "crs": "EPSG:4326",
                "fields": [
                    {"name": "name", "type": "string"},
                    {"name": "value", "type": "integer"},
                ],
                "features": [
                    {
                        "geometry_wkt": "POINT (0 0)",
                        "attributes": {"name": "alpha", "value": 1},
                    },
                    {
                        "geometry_wkt": "POINT (1 1)",
                        "attributes": {"name": "beta", "value": 2},
                    },
                ],
                "path": str(vector_path),
                "driver_name": "GeoJSON",
                "add_to_project": True,
            },
        )
        vector_layer_id = self._layer_id(created, "create_vector_layer")
        self.cleanup.register_layer(vector_layer_id)
        require_cleanup_complete(created, "create_vector_layer")
        if not vector_path.is_file() or int(created.get("feature_count", -1)) != 2:
            raise AcceptanceFailure("create_vector_layer did not publish its live fixture")

        added = self._success(
            session,
            "add_vector_features",
            {
                "layer_id": vector_layer_id,
                "features": [
                    {
                        "geometry_wkt": "POINT (2 2)",
                        "attributes": {"name": "gamma", "value": 3},
                    }
                ],
            },
        )
        if int(added.get("added_count", -1)) != 1:
            raise AcceptanceFailure("add_vector_features did not add one feature")

        queried = self._success(
            session,
            "query_vector_features",
            {
                "layer_id": vector_layer_id,
                "include_geometry": True,
                "limit": 10,
            },
        )
        feature_ids = [
            feature.get("feature_id")
            for feature in queried.get("features") or []
            if isinstance(feature, dict) and feature.get("feature_id") is not None
        ]
        if len(feature_ids) != 3 or len(set(feature_ids)) != 3:
            raise AcceptanceFailure(
                "query_vector_features did not return the three live features"
            )

        updated = self._success(
            session,
            "update_vector_features",
            {
                "layer_id": vector_layer_id,
                "updates": [
                    {
                        "feature_id": feature_ids[0],
                        "attributes": {"value": 11},
                        "geometry_wkt": "POINT (0.25 0.25)",
                    }
                ],
            },
        )
        if int(updated.get("updated_count", -1)) != 1:
            raise AcceptanceFailure("update_vector_features did not update one feature")

        selection = self._success(
            session,
            "set_vector_selection",
            {
                "layer_id": vector_layer_id,
                "feature_ids": [feature_ids[1]],
                "mode": "replace",
            },
        )
        if int(selection.get("selected_count", -1)) != 1:
            raise AcceptanceFailure("set_vector_selection did not select one feature")

        deletion_preview = self._success(
            session,
            "delete_vector_features",
            {"layer_id": vector_layer_id, "feature_ids": [feature_ids[2]]},
        )
        deletion_token = str(deletion_preview.get("confirmation_token") or "")
        if len(deletion_token) < 16:
            raise AcceptanceFailure("delete_vector_features returned no confirmation token")
        deleted = self._success(
            session,
            "delete_vector_features",
            {"layer_id": vector_layer_id, "confirmation_token": deletion_token},
        )
        if int(deleted.get("deleted_count", -1)) != 1:
            raise AcceptanceFailure("delete_vector_features did not delete one feature")

        layers = self._success(session, "list_map_layers", {})
        if vector_layer_id not in self._layer_ids(layers):
            raise AcceptanceFailure("list_map_layers omitted the created vector layer")
        self._success(session, "describe_layer_sources", {})
        self._success(
            session,
            "get_layer_metadata",
            {"layer_id": vector_layer_id, "include_source": True},
        )
        self._success(
            session,
            "set_layer_visibility",
            {"layer_id": vector_layer_id, "visible": False},
        )
        visible = self._success(
            session,
            "set_layer_visibility",
            {"layer_id": vector_layer_id, "visible": True},
        )
        if visible.get("visible") is not True:
            raise AcceptanceFailure("set_layer_visibility did not restore visibility")

        self._success(
            session,
            "configure_vector_labels",
            {
                "layer_id": vector_layer_id,
                "enabled": True,
                "field_or_expression": "name",
                "font_size": 9,
                "color": "#204060",
            },
        )
        styled = self._success(
            session,
            "apply_layer_style",
            {"layer_id": vector_layer_id, "style_path": str(style_path)},
        )
        if styled.get("loaded") is not True:
            raise AcceptanceFailure("apply_layer_style did not load the QML fixture")

        self.cleanup.register_pending_layer_name(raster_layer_name)
        loaded = self._success(
            session,
            "load_map_layer",
            {
                "layer_type": "raster",
                "source": str(raster_path),
                "name": raster_layer_name,
                "provider": "gdal",
                "add_to_project": True,
            },
        )
        raster_layer_id = self._layer_id(loaded, "load_map_layer")
        self.cleanup.register_layer(raster_layer_id)
        statistics = self._success(
            session,
            "get_raster_statistics",
            {"layer_id": raster_layer_id, "band": 1, "sample_size": 100},
        )
        if int(statistics.get("element_count", 0)) != 4:
            raise AcceptanceFailure("get_raster_statistics did not inspect four cells")

        self._exercise_canvas_navigation(
            session,
            vector_layer_id=vector_layer_id,
            raster_layer_id=raster_layer_id,
        )

        crs = self._success(session, "get_project_crs", {})
        original_crs = crs.get("crs")
        if not isinstance(original_crs, dict):
            raise AcceptanceFailure("get_project_crs returned no CRS metadata")
        original_auth_id = str(original_crs.get("auth_id") or "")
        original_wkt = str(original_crs.get("wkt") or "")
        if original_auth_id:
            original_selector = original_auth_id
            original_identity = f"auth:{original_auth_id}"
        elif original_wkt and original_crs.get("wkt_truncated") is not True:
            original_selector = original_wkt
            original_identity = f"wkt:{original_wkt}"
        else:
            raise AcceptanceFailure(
                "The current project CRS cannot be safely restored after acceptance"
            )
        self.cleanup.register_crs_restore(original_selector, original_identity)
        test_crs = "EPSG:3857" if original_auth_id != "EPSG:3857" else "EPSG:4326"
        changed_crs = self._success(
            session,
            "set_project_crs",
            {"crs": test_crs},
        )
        if (changed_crs.get("crs") or {}).get("auth_id") != test_crs:
            raise AcceptanceFailure("set_project_crs did not apply the test CRS")
        self._restore_project_crs(session)

        exported = self._success(
            session,
            "export_map_image",
            {"path": str(image_path)},
        )
        require_cleanup_complete(exported, "export_map_image")
        if (
            exported.get("saved") is not True
            or not image_path.is_file()
            or image_path.stat().st_size <= 0
            or not world_path.is_file()
            or world_path.stat().st_size <= 0
        ):
            raise AcceptanceFailure("export_map_image did not publish its image family")
        self._success(session, "refresh_canvas", {})

        self._remove_layers(session, {vector_layer_id, raster_layer_id})
        self.cleanup.mark_layers_removed({vector_layer_id, raster_layer_id})
        self.cleanup.mark_layer_names_removed(
            {vector_layer_name, raster_layer_name}
        )
        self._exercise_disposable_project_outputs(session)

    def _exercise_disposable_project_outputs(self, session: McpSession) -> None:
        """Exercise persistent project state last in a disposable QGIS process."""

        layout_path = self.cleanup.reserve_path("interactive/layout.pdf")
        project_path = self.cleanup.reserve_path("interactive/project.qgz")
        layout_name = f"qcopilots-live-layout-{uuid.uuid4().hex}"

        self.cleanup.register_requires_process_exit("print_layout", layout_name)
        created = self._success(
            session,
            "create_print_layout",
            {
                "name": layout_name,
                "title": "QCopilots live acceptance",
                "page_size": "A4",
                "orientation": "landscape",
                "map_extent": [-1, -1, 3, 3],
                "include_legend": False,
                "include_scale_bar": False,
            },
        )
        layout = created.get("layout") or {}
        if (
            created.get("created") is not True
            or not isinstance(layout, dict)
            or layout.get("name") != layout_name
        ):
            raise AcceptanceFailure("create_print_layout did not create its layout")

        listed = self._success(session, "list_print_layouts", {})
        layout_names = {
            str(item.get("name") or "")
            for item in listed.get("layouts") or []
            if isinstance(item, dict)
        }
        if layout_name not in layout_names:
            raise AcceptanceFailure("list_print_layouts omitted the created layout")

        exported = self._success(
            session,
            "export_print_layout",
            {
                "layout_name": layout_name,
                "path": str(layout_path),
                "format": "pdf",
                "dpi": 96,
            },
        )
        require_cleanup_complete(exported, "export_print_layout")
        if (
            exported.get("saved") is not True
            or not layout_path.is_file()
            or layout_path.stat().st_size <= 0
        ):
            raise AcceptanceFailure(
                "export_print_layout did not publish its registered PDF"
            )

        self.cleanup.register_requires_process_exit(
            "project_filename",
            str(project_path),
        )
        saved = self._success(
            session,
            "save_project",
            {"path": str(project_path)},
        )
        require_cleanup_complete(saved, "save_project")
        if (
            saved.get("saved") is not True
            or not project_path.is_file()
            or project_path.stat().st_size <= 0
        ):
            raise AcceptanceFailure("save_project did not publish its registered QGZ")

    def _exercise_canvas_navigation(
        self,
        session: McpSession,
        *,
        vector_layer_id: str,
        raster_layer_id: str,
    ) -> None:
        self._success(session, "zoom_to_extent", {"extent": [-1, -1, 3, 3]})
        self._success(session, "zoom_to_last_extent", {})
        self._success(session, "zoom_to_next_extent", {})
        self._success(session, "zoom_to_last_extent", {})

        self._success(session, "zoom_in", {})
        self._success(session, "zoom_out", {})
        self._success(session, "zoom_to_layer", {"layer_id": vector_layer_id})
        self._success(session, "zoom_to_last_extent", {})
        self._success(session, "zoom_full", {})
        self._success(session, "zoom_to_last_extent", {})
        selection_zoom = self._success(
            session,
            "zoom_to_selection",
            {"layer_id": vector_layer_id},
        )
        if int(selection_zoom.get("selected_feature_count", -1)) != 1:
            raise AcceptanceFailure(
                "zoom_to_selection did not report the selected feature"
            )
        if selection_zoom.get("zoomed") is not True or not any(
            selection_zoom.get(field) is True
            for field in (
                "extent_changed",
                "scale_changed",
                "magnification_changed",
            )
        ):
            raise AcceptanceFailure(
                "zoom_to_selection did not produce an observable canvas change"
            )
        self._success(session, "zoom_to_last_extent", {})
        self._success(
            session,
            "zoom_to_native_resolution",
            {"layer_id": raster_layer_id},
        )
        self._success(session, "zoom_to_last_extent", {})

    def _exercise_processing(self, session: McpSession, category: str) -> None:
        if category not in {"vector", "raster", "general"}:
            raise AcceptanceFailure(f"Unsupported Processing category: {category}")
        list_tool = f"list_{category}_processing_algorithms"
        details_tool = f"get_{category}_processing_algorithm_details"
        start_tool = f"start_{category}_processing_algorithm"
        get_tool = f"get_{category}_processing_job"
        list_jobs_tool = f"list_{category}_processing_jobs"
        cancel_tool = f"cancel_{category}_processing_job"
        algorithm_id, parameters, output_path = self._processing_scenario(category)

        algorithm_ids: set[str] = set()
        page_arguments: dict[str, Any] = {"max_results": 2000}
        while True:
            page = self._success(session, list_tool, page_arguments)
            algorithm_ids.update(
                str(item.get("id") or "")
                for item in page.get("algorithms") or []
                if isinstance(item, dict)
            )
            cursor = str(page.get("next_cursor") or "")
            if not cursor:
                break
            page_arguments = {"cursor": cursor}
        if algorithm_id not in algorithm_ids:
            raise AcceptanceFailure(
                f"{list_tool} omitted required safe algorithm {algorithm_id}"
            )

        details = self._success(
            session,
            details_tool,
            {"algorithm_id": algorithm_id},
        )
        details_id = str(details.get("id") or (details.get("algorithm") or {}).get("id") or "")
        if details_id != algorithm_id:
            raise AcceptanceFailure(f"{details_tool} returned a different algorithm")

        client_request_id = f"live-{category}-{uuid.uuid4().hex}"
        registration = JobRegistration(
            service=session.service,
            get_tool=get_tool,
            cancel_tool=cancel_tool,
            list_tool=list_jobs_tool,
            client_request_id=client_request_id,
        )
        self.cleanup.register_job(registration)
        started = self._success(
            session,
            start_tool,
            {
                "algorithm_id": algorithm_id,
                "parameters": parameters,
                "add_outputs_to_project": False,
                "client_request_id": client_request_id,
            },
        )
        job_id = str(started.get("job_id") or "")
        if not job_id:
            raise AcceptanceFailure(f"{start_tool} returned no job_id")
        registration.job_id = job_id

        self._success(session, get_tool, {"job_id": job_id})
        listed_jobs = self._success(session, list_jobs_tool, {"limit": 200})
        if job_id not in {
            str(item.get("job_id") or "")
            for item in listed_jobs.get("jobs") or []
            if isinstance(item, dict)
        }:
            raise AcceptanceFailure(f"{list_jobs_tool} omitted its newly started job")

        terminal = self._wait_for_job(session, get_tool, job_id)
        if terminal.get("state") != "succeeded":
            raise AcceptanceFailure(
                f"{algorithm_id} ended in state {terminal.get('state')}: "
                f"{terminal.get('error')}"
            )
        if not output_path.is_file() or output_path.stat().st_size <= 0:
            raise AcceptanceFailure(
                f"{algorithm_id} did not publish its registered output"
            )
        cancelled = self._success(session, cancel_tool, {"job_id": job_id})
        if cancelled.get("state") not in TERMINAL_JOB_STATES:
            raise AcceptanceFailure(f"{cancel_tool} returned a non-terminal job")
        self.cleanup.unregister_job(registration)

    def _processing_scenario(
        self,
        category: str,
    ) -> tuple[str, dict[str, Any], Path]:
        vector_path = self.cleanup.reserve_path("interactive/acceptance.geojson")
        raster_path = self.cleanup.reserve_path("inputs/acceptance.asc")
        if category == "vector":
            output_path = self.cleanup.reserve_path("processing/vector-buffer.gpkg")
            return (
                "native:buffer",
                {
                    "INPUT": str(vector_path),
                    "DISTANCE": 1.0,
                    "SEGMENTS": 4,
                    "END_CAP_STYLE": 0,
                    "JOIN_STYLE": 0,
                    "MITER_LIMIT": 2.0,
                    "DISSOLVE": False,
                    "OUTPUT": str(output_path),
                },
                output_path,
            )
        if category == "raster":
            output_path = self.cleanup.reserve_path("processing/raster-warp.tif")
            return (
                "gdal:warpreproject",
                {
                    "INPUT": str(raster_path),
                    "SOURCE_CRS": "EPSG:4326",
                    "TARGET_CRS": "EPSG:4326",
                    "OUTPUT": str(output_path),
                },
                output_path,
            )
        output_path = self.cleanup.reserve_path("processing/general-package.gpkg")
        return (
            "native:package",
            {
                "LAYERS": [str(vector_path)],
                "OUTPUT": str(output_path),
                "OVERWRITE": False,
                "SAVE_STYLES": False,
                "SAVE_METADATA": False,
                "SELECTED_FEATURES_ONLY": False,
                "EXPORT_RELATED_LAYERS": False,
            },
            output_path,
        )

    def _exercise_binary(self, session: McpSession) -> None:
        safe_page = self._success(
            session,
            "list_qgis_binaries",
            {"query": "projinfo", "enabled": True, "risk": "R1", "limit": 50},
        )
        safe_binaries = [
            item
            for item in safe_page.get("binaries") or []
            if isinstance(item, dict)
        ]
        safe = next(
            (item for item in safe_binaries if item.get("id") == "bin.projinfo"),
            safe_binaries[0] if safe_binaries else None,
        )
        if safe is None:
            raise AcceptanceFailure("No enabled R1 projinfo binary is available")
        binary_id = str(safe.get("id") or "")
        details = self._success(
            session,
            "get_qgis_binary_details",
            {"binary_id": binary_id},
        )
        if details.get("id") != binary_id or details.get("risk") != "R1":
            raise AcceptanceFailure("get_qgis_binary_details returned a different policy")

        self._run_binary_job(
            session,
            binary_id=binary_id,
            arguments=["--searchpaths"],
            confirmed_risk=False,
        )

        r2_page = self._success(
            session,
            "list_qgis_binaries",
            {
                "query": "bin.gdal_translate",
                "enabled": True,
                "risk": "R2",
                "limit": 10,
            },
        )
        r2_entries = [
            item
            for item in r2_page.get("binaries") or []
            if isinstance(item, dict) and item.get("id")
        ]
        r2 = next(
            (
                item
                for item in r2_entries
                if item.get("id") == "bin.gdal_translate"
            ),
            None,
        )
        if r2 is None:
            raise AcceptanceFailure(
                "The QGIS binary catalog has no enabled gdal_translate R2 entry"
            )
        r2_id = str(r2["id"])
        r2_details = self._success(
            session,
            "get_qgis_binary_details",
            {"binary_id": r2_id},
        )
        if r2_details.get("risk") != "R2" or r2_details.get("enabled") is not True:
            raise AcceptanceFailure("gdal_translate returned an unexpected R2 policy")
        raster_input = self.cleanup.reserve_path("inputs/acceptance.asc")
        translated_output = self.cleanup.reserve_output_path(
            "binary/gdal-translate.tif"
        )
        if not raster_input.is_file():
            raise AcceptanceFailure("The binary raster fixture is missing")
        r2_arguments = [
            "-of",
            "GTiff",
            str(raster_input),
            str(translated_output),
        ]
        self._business_error(
            session,
            "start_qgis_binary",
            {
                "binary_id": r2_id,
                "arguments": r2_arguments,
                "working_directory": str(self.cleanup.root),
                "confirmed_risk": False,
            },
            ("risk confirmation required", "confirmed_risk must be true"),
        )
        self._run_binary_job(
            session,
            binary_id=r2_id,
            arguments=r2_arguments,
            confirmed_risk=True,
        )
        if not translated_output.is_file() or translated_output.stat().st_size <= 0:
            raise AcceptanceFailure(
                "The confirmed R2 gdal_translate run produced no registered output"
            )

        r3_page = self._success(
            session,
            "list_qgis_binaries",
            {"risk": "R3", "limit": 1},
        )
        r3_entries = [
            item
            for item in r3_page.get("binaries") or []
            if isinstance(item, dict) and item.get("id")
        ]
        if not r3_entries:
            raise AcceptanceFailure("The QGIS binary catalog has no R3 entry")
        self._business_error(
            session,
            "start_qgis_binary",
            {
                "binary_id": str(r3_entries[0]["id"]),
                "arguments": [],
                "working_directory": str(self.cleanup.root),
                "confirmed_risk": True,
            },
            ("risk blocked", "r3 binaries cannot run"),
        )

        disabled_page = self._success(
            session,
            "list_qgis_binaries",
            {"enabled": False, "risk": "R1", "limit": 1},
        )
        disabled = [
            item
            for item in disabled_page.get("binaries") or []
            if isinstance(item, dict) and item.get("id")
        ]
        if not disabled:
            raise AcceptanceFailure("The QGIS binary catalog has no disabled entry")
        self._business_error(
            session,
            "start_qgis_binary",
            {
                "binary_id": str(disabled[0]["id"]),
                "arguments": [],
                "working_directory": str(self.cleanup.root),
            },
            ("disabled", "risk blocked", "cannot run"),
        )

    def _run_binary_job(
        self,
        session: McpSession,
        *,
        binary_id: str,
        arguments: list[str],
        confirmed_risk: bool,
    ) -> dict[str, Any]:
        client_request_id = f"live-binary-{uuid.uuid4().hex}"
        registration = JobRegistration(
            service=session.service,
            get_tool="get_qgis_binary_job",
            cancel_tool="cancel_qgis_binary_job",
            list_tool="list_qgis_binary_jobs",
            client_request_id=client_request_id,
        )
        self.cleanup.register_job(registration)
        started = self._success(
            session,
            "start_qgis_binary",
            {
                "binary_id": binary_id,
                "arguments": arguments,
                "working_directory": str(self.cleanup.root),
                "timeout_seconds": 30,
                "client_request_id": client_request_id,
                "confirmed_risk": confirmed_risk,
            },
        )
        job_id = str(started.get("job_id") or "")
        if not job_id:
            raise AcceptanceFailure("start_qgis_binary returned no job_id")
        registration.job_id = job_id
        self._success(session, "get_qgis_binary_job", {"job_id": job_id})
        jobs = self._success(session, "list_qgis_binary_jobs", {"limit": 200})
        if job_id not in {
            str(item.get("job_id") or "")
            for item in jobs.get("jobs") or []
            if isinstance(item, dict)
        }:
            raise AcceptanceFailure("list_qgis_binary_jobs omitted its new job")
        terminal = self._wait_for_job(
            session,
            "get_qgis_binary_job",
            job_id,
        )
        if terminal.get("state") != "succeeded":
            raise AcceptanceFailure(
                f"{binary_id} ended in state {terminal.get('state')}: "
                f"{terminal.get('error')}"
            )
        cancelled = self._success(
            session,
            "cancel_qgis_binary_job",
            {"job_id": job_id},
        )
        if cancelled.get("state") not in TERMINAL_JOB_STATES:
            raise AcceptanceFailure(
                "cancel_qgis_binary_job returned a non-terminal job"
            )
        self.cleanup.unregister_job(registration)
        return terminal

    def _wait_for_job(
        self,
        session: McpSession,
        get_tool: str,
        job_id: str,
    ) -> dict[str, Any]:
        deadline = time.monotonic() + self.job_timeout_seconds
        while True:
            snapshot = self._success(session, get_tool, {"job_id": job_id})
            state = str(snapshot.get("state") or "")
            if state in TERMINAL_JOB_STATES:
                require_cleanup_complete(snapshot, get_tool)
                return snapshot
            if time.monotonic() >= deadline:
                raise AcceptanceFailure(
                    f"{get_tool} did not reach a terminal state before timeout"
                )
            time.sleep(0.1)

    @staticmethod
    def _layer_id(payload: dict[str, Any], label: str) -> str:
        layer = payload.get("layer")
        layer_id = str(layer.get("id") or "") if isinstance(layer, dict) else ""
        if not layer_id:
            raise AcceptanceFailure(f"{label} returned no layer id")
        return layer_id

    @staticmethod
    def _layer_ids(payload: dict[str, Any]) -> set[str]:
        return {
            str(item.get("id") or "")
            for item in payload.get("layers") or []
            if isinstance(item, dict) and item.get("id")
        }

    def _remove_layers(self, session: McpSession, layer_ids: set[str]) -> None:
        for layer_id in sorted(layer_ids):
            preview = self._success(
                session,
                "remove_map_layers",
                {
                    "layer_id": layer_id,
                    "editable_changes": "discard",
                },
            )
            token = str(preview.get("confirmation_token") or "")
            if len(token) < 16:
                raise AcceptanceFailure(
                    "remove_map_layers returned no confirmation token"
                )
            removed = self._success(
                session,
                "remove_map_layers",
                {"confirmation_token": token},
            )
            if int(removed.get("removed_count", -1)) != 1:
                raise AcceptanceFailure(
                    "remove_map_layers removed an unexpected layer count"
                )

    def _cleanup_registered_jobs(self) -> list[str]:
        errors: list[str] = []
        for registration in list(self.cleanup.jobs):
            try:
                with self._session(
                    registration.service,
                    with_coverage=False,
                ) as session:
                    self._assert_cleanup_tools(
                        session,
                        {
                            registration.list_tool,
                            registration.get_tool,
                            registration.cancel_tool,
                        },
                    )
                    if not self._resolve_pending_job(session, registration):
                        self.cleanup.unregister_job(registration)
                        continue
                    cancel_result = session.call_tool(
                        registration.cancel_tool,
                        {"job_id": registration.job_id},
                    )
                    if cancel_result.get("isError") is True:
                        message = require_business_error(
                            cancel_result,
                            f"cleanup.{registration.cancel_tool}",
                            (),
                        )
                        if "not found" not in message.casefold():
                            raise AcceptanceFailure(
                                f"cleanup.{registration.cancel_tool} failed"
                            )
                    else:
                        require_success(
                            cancel_result,
                            f"cleanup.{registration.cancel_tool}",
                        )
                        self._wait_for_job(
                            session,
                            registration.get_tool,
                            registration.job_id,
                        )
                self.cleanup.unregister_job(registration)
            except Exception as err:
                errors.append(
                    f"Job cleanup failed for {registration.service.service_id}: "
                    f"{_redacted_message(err, self._token)}"
                )
        if self.cleanup.jobs:
            errors.append(
                "Registered jobs remain after cleanup: "
                + ", ".join(
                    item.job_id or f"pending:{item.client_request_id}"
                    for item in self.cleanup.jobs
                )
            )
        return errors

    @staticmethod
    def _resolve_pending_job(
        session: McpSession,
        registration: JobRegistration,
    ) -> bool:
        if registration.job_id:
            return True
        listed = require_success(
            session.call_tool(registration.list_tool, {"limit": 200}),
            f"cleanup.{registration.list_tool}",
        )
        matches = [
            str(item.get("job_id") or "")
            for item in listed.get("jobs") or []
            if isinstance(item, dict)
            and item.get("job_id")
            and item.get("client_request_id") == registration.client_request_id
        ]
        if len(matches) > 1:
            raise AcceptanceFailure("Pending job request matched more than one job")
        if not matches:
            return False
        registration.job_id = matches[0]
        return True

    def _cleanup_qgis_state(self) -> list[str]:
        if (
            not self.cleanup.qgis_touched
            and not self.cleanup.layer_ids
            and not self.cleanup.layer_names
            and not self.cleanup.original_crs_selector
        ):
            return []
        errors: list[str] = []
        service = self._service_by_port[48213]
        try:
            with self._session(service, with_coverage=False) as session:
                required_tools = {
                    "list_map_layers",
                    "remove_map_layers",
                    "refresh_canvas",
                }
                if self.cleanup.original_crs_selector:
                    required_tools.add("set_project_crs")
                self._assert_cleanup_tools(session, required_tools)
                listed = require_success(
                    session.call_tool("list_map_layers", {}),
                    "cleanup.list_map_layers",
                )
                present = self.cleanup.layer_ids & self._layer_ids(listed)
                present.update(
                    str(item.get("id") or "")
                    for item in listed.get("layers") or []
                    if isinstance(item, dict)
                    and item.get("id")
                    and str(item.get("name") or "") in self.cleanup.layer_names
                )
                if present:
                    self._remove_layers(session, present)
                    self.cleanup.mark_layers_removed(present)
                verified = require_success(
                    session.call_tool("list_map_layers", {}),
                    "cleanup.verify_list_map_layers",
                )
                residual_layers = self.cleanup.layer_ids & self._layer_ids(verified)
                residual_names = {
                    str(item.get("name") or "")
                    for item in verified.get("layers") or []
                    if isinstance(item, dict)
                    and str(item.get("name") or "") in self.cleanup.layer_names
                }
                if residual_layers:
                    raise AcceptanceFailure(
                        "QGIS test layers remain: " + ", ".join(sorted(residual_layers))
                    )
                if residual_names:
                    raise AcceptanceFailure(
                        "QGIS test layer names remain: "
                        + ", ".join(sorted(residual_names))
                    )
                self._restore_project_crs(session, with_coverage=False)
                require_success(
                    session.call_tool("refresh_canvas", {}),
                    "cleanup.refresh_canvas",
                )
                self.cleanup.layer_ids.clear()
                self.cleanup.layer_names.clear()
                self.cleanup.qgis_touched = False
        except Exception as err:
            errors.append(
                "QGIS state cleanup failed: "
                + _redacted_message(err, self._token)
            )
        if self.cleanup.layer_ids:
            errors.append(
                "Registered QGIS layers remain after cleanup: "
                + ", ".join(sorted(self.cleanup.layer_ids))
            )
        if self.cleanup.layer_names:
            errors.append(
                "Registered QGIS layer names remain after cleanup: "
                + ", ".join(sorted(self.cleanup.layer_names))
            )
        if self.cleanup.original_crs_selector:
            errors.append("The original QGIS project CRS was not restored")
        return errors

    def _restore_project_crs(
        self,
        session: McpSession,
        *,
        with_coverage: bool = True,
    ) -> None:
        if not self.cleanup.original_crs_selector:
            return
        arguments = {"crs": self.cleanup.original_crs_selector}
        if with_coverage:
            restored = self._success(session, "set_project_crs", arguments)
        else:
            restored = require_success(
                session.call_tool("set_project_crs", arguments),
                "cleanup.set_project_crs",
            )
        metadata = restored.get("crs") or {}
        identity_kind, _separator, expected_identity = (
            self.cleanup.original_crs_identity.partition(":")
        )
        restored_identity = str(
            metadata.get("auth_id")
            if identity_kind == "auth"
            else metadata.get("wkt")
            if identity_kind == "wkt"
            else ""
        )
        if restored_identity != expected_identity:
            raise AcceptanceFailure("The original project CRS identity was not restored")
        self.cleanup.clear_crs_restore()

    @staticmethod
    def _assert_cleanup_tools(
        session: McpSession,
        required_tools: set[str],
    ) -> None:
        names = {
            str(item.get("name") or "") for item in session.list_tools()
        }
        missing = required_tools - names
        if missing:
            raise AcceptanceFailure(
                f"{session.service.service_id} cleanup tools are missing: "
                + ", ".join(sorted(missing))
            )


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Run live acceptance against the seven QCopilots MCP services after "
            "QGIS has been started with Launcher.ps1."
        )
    )
    parser.add_argument(
        "--config-path",
        help=(
            "Optional manager user configuration path. The Bearer token is read "
            "from the file and is never printed."
        ),
    )
    parser.add_argument(
        "--request-timeout-seconds",
        type=float,
        default=60.0,
    )
    parser.add_argument(
        "--job-timeout-seconds",
        type=float,
        default=120.0,
    )
    parser.add_argument(
        "--workspace-base",
        help=(
            "Optional existing absolute directory under which a unique live "
            "acceptance run directory is created. The base directory itself is "
            "never removed."
        ),
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    token = ""
    try:
        arguments = build_argument_parser().parse_args(argv)
        if arguments.request_timeout_seconds <= 0:
            raise AcceptanceFailure("Request timeout must be greater than zero")
        if arguments.job_timeout_seconds <= 0:
            raise AcceptanceFailure("Job timeout must be greater than zero")
        configuration = load_manager_acceptance_config(arguments.config_path)
        token = configuration.auth_token
        AcceptanceRunner(
            token,
            request_timeout_seconds=arguments.request_timeout_seconds,
            job_timeout_seconds=arguments.job_timeout_seconds,
            security_mode=configuration.security_mode,
            shell_enabled=configuration.shell_enabled,
            workspace_base=arguments.workspace_base,
        ).run()
        return 0
    except Exception as err:
        print(f"FAIL: {_redacted_message(err, token)}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
