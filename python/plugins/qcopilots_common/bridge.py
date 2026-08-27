"""HTTP bridge between out-of-process MCP services and the QGIS process.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import base64
import binascii
import errno
import hashlib
import hmac
import json
import math
import os
import re
import secrets
import shutil
import tempfile
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, unquote, urlencode, urlsplit
from urllib.request import Request, urlopen

from qcopilots_common.constants import (
    DEFAULT_PROCESSING_ALGORITHM_RESULTS,
    DEFAULT_BRIDGE_PORT,
    DEFAULT_HOST,
    HTTP_ERROR_BODY_DRAIN_TIMEOUT_SECONDS,
    MAX_HTTP_ERROR_BODY_DRAIN_BYTES,
    MAX_HTTP_REQUEST_BODY_BYTES,
    MAX_PROCESSING_ALGORITHM_CURSOR_LENGTH,
    MAX_PROCESSING_ALGORITHM_RESULTS,
    PROCESSING_ALGORITHM_CURSOR_ERROR_PREFIX,
    PROCESSING_ALGORITHM_PAGE_SIZE,
    QGIS_BRIDGE_AUTH_TOKEN_ENV,
)
from qcopilots_common.processing_metadata import (
    processing_algorithm_matches_domain,
    processing_algorithm_metadata,
    processing_algorithm_start_policy,
)
from qcopilots_common.mcp_http import ToolError
from qcopilots_common.security_policy import FilesystemPolicy


MAX_VECTOR_QUERY_OFFSET = 10000
MAX_VECTOR_QUERY_GEOMETRY_BYTES = 1024 * 1024
MAX_VECTOR_QUERY_RESPONSE_BYTES = 2 * 1024 * 1024
MAX_SELECTION_MATCHES = 10000
MAX_SELECTION_SUMMARY_IDS = 100
MAX_RASTER_STATISTICS_SAMPLES = 250000
MAX_LAYOUT_EXPORT_PIXELS = 25000000
MAX_INTERACTIVE_OVERWRITE_HASH_BYTES = 64 * 1024 * 1024
MAX_LAYER_REMOVAL_LAYERS = 64
MAX_LAYER_REMOVAL_EDIT_ENTRIES = 10000
MAX_LAYER_REMOVAL_FINGERPRINT_BYTES = 4 * 1024 * 1024
LAYER_REMOVAL_EDITABLE_CHANGES = frozenset({"reject", "save", "discard"})


class ProcessingAlgorithmCursorError(RuntimeError):
    """Raised when an algorithm-list cursor cannot be safely continued."""


class BridgeClient:
    def __init__(self, base_url: str | None, auth_token: str | None = None):
        self.base_url = (base_url or "").rstrip("/")
        self._auth_token = (
            os.environ.get(QGIS_BRIDGE_AUTH_TOKEN_ENV, "")
            if auth_token is None
            else auth_token
        )

    def call(self, tool: str, arguments: dict[str, Any] | None = None) -> dict[str, Any]:
        if not self.base_url:
            raise RuntimeError("QCopilots QGIS bridge is not configured")
        payload = json.dumps({"tool": tool, "arguments": arguments or {}}).encode("utf-8")
        headers = {"Content-Type": "application/json"}
        if self._auth_token:
            headers["Authorization"] = f"Bearer {self._auth_token}"
        request = Request(
            f"{self.base_url}/call",
            data=payload,
            headers=headers,
            method="POST",
        )
        with urlopen(request, timeout=60) as response:
            data = json.loads(response.read().decode("utf-8"))
        if not data.get("ok"):
            raise ToolError(data.get("error", "QGIS bridge call failed"))
        return data.get("result", {})


def _is_address_in_use(error: OSError) -> bool:
    return error.errno == errno.EADDRINUSE or getattr(error, "winerror", None) == 10048


def _bridge_bind_host(value: str) -> str:
    host = value.strip()
    if host == DEFAULT_HOST:
        return host
    raise ValueError(
        "QCopilots QGIS bridge requires the literal IPv4 loopback host "
        f"{DEFAULT_HOST}: {host}"
    )


class QgisBridgeHttpServer(ThreadingHTTPServer):
    allow_reuse_address = False


class FeatureDeleteConfirmationStore:
    def __init__(
        self,
        ttl_seconds: float = 300,
        max_entries: int = 256,
        clock: Any = time.monotonic,
        token_factory: Any = None,
    ):
        self.ttl_seconds = float(ttl_seconds)
        self.max_entries = int(max_entries)
        self._clock = clock
        self._token_factory = token_factory or (lambda: secrets.token_urlsafe(32))
        self._entries: dict[str, dict[str, Any]] = {}
        self._lock = threading.RLock()

    def issue(
        self,
        layer_id: str,
        feature_ids: list[int],
        fingerprint: str,
    ) -> str:
        with self._lock:
            now = self._clock()
            self._purge_expired(now)
            while len(self._entries) >= self.max_entries:
                oldest = min(
                    self._entries,
                    key=lambda token: self._entries[token]["expires_at"],
                )
                self._entries.pop(oldest, None)
            token = self._token_factory()
            self._entries[token] = {
                "layer_id": layer_id,
                "feature_ids": list(feature_ids),
                "fingerprint": fingerprint,
                "expires_at": now + self.ttl_seconds,
            }
            return token

    def consume(self, token: str, layer_id: str) -> dict[str, Any]:
        with self._lock:
            now = self._clock()
            entry = self._entries.pop(str(token), None)
            if entry is None:
                raise RuntimeError("Delete confirmation token is invalid or already used")
            if entry["expires_at"] <= now:
                raise RuntimeError("Delete confirmation token has expired")
            if entry["layer_id"] != layer_id:
                raise RuntimeError("Delete confirmation token does not match the layer")
            return entry

    def clear(self) -> None:
        with self._lock:
            self._entries.clear()

    def _purge_expired(self, now: float) -> None:
        for token in [
            token
            for token, entry in self._entries.items()
            if entry["expires_at"] <= now
        ]:
            self._entries.pop(token, None)


class DestructiveActionConfirmationStore:
    def __init__(
        self,
        ttl_seconds: float = 300,
        max_entries: int = 256,
        clock: Any = time.monotonic,
        token_factory: Any = None,
    ):
        self.ttl_seconds = float(ttl_seconds)
        self.max_entries = int(max_entries)
        self._clock = clock
        self._token_factory = token_factory or (lambda: secrets.token_urlsafe(32))
        self._entries: dict[str, dict[str, Any]] = {}
        self._lock = threading.RLock()

    def issue(self, action: str, fingerprint: str, payload: Any) -> str:
        with self._lock:
            now = self._clock()
            self._purge_expired(now)
            while len(self._entries) >= self.max_entries:
                oldest = min(
                    self._entries,
                    key=lambda token: self._entries[token]["expires_at"],
                )
                self._entries.pop(oldest, None)
            token = self._token_factory()
            self._entries[token] = {
                "action": str(action),
                "fingerprint": str(fingerprint),
                "payload": payload,
                "expires_at": now + self.ttl_seconds,
            }
            return token

    def consume(self, token: str, action: str, fingerprint: str) -> Any:
        entry = self.consume_payload(token, action)
        if entry["fingerprint"] != fingerprint:
            raise RuntimeError(
                "Destructive action target or parameters changed after preview. "
                "Request a new confirmation token."
            )
        return entry["payload"]

    def consume_payload(self, token: str, action: str) -> dict[str, Any]:
        with self._lock:
            now = self._clock()
            entry = self._entries.pop(str(token), None)
            if entry is None:
                raise RuntimeError(
                    "Destructive action confirmation token is invalid or already used"
                )
            if entry["expires_at"] <= now:
                raise RuntimeError("Destructive action confirmation token has expired")
            if entry["action"] != action:
                raise RuntimeError(
                    "Destructive action confirmation token does not match this action"
                )
            return entry

    def clear(self) -> None:
        with self._lock:
            self._entries.clear()

    def _purge_expired(self, now: float) -> None:
        for token in [
            token
            for token, entry in self._entries.items()
            if entry["expires_at"] <= now
        ]:
            self._entries.pop(token, None)


class QgisBridgeController:
    def __init__(
        self,
        iface: Any,
        host: str = DEFAULT_HOST,
        port: int = DEFAULT_BRIDGE_PORT,
        auth_token: str | None = None,
        filesystem_policy: FilesystemPolicy | None = None,
    ):
        self.iface = iface
        self.host = _bridge_bind_host(host)
        self.port = port
        self._auth_token = auth_token or ""
        self._filesystem_policy = filesystem_policy or FilesystemPolicy()
        self._httpd: ThreadingHTTPServer | None = None
        self._thread: threading.Thread | None = None
        self._shutdown_requested = False
        self._stopping = threading.Event()
        self._dispatcher = _make_main_thread_dispatcher()
        self._processing_job_manager = None
        self._processing_job_manager_lock = threading.RLock()
        self._qgis_binary_job_manager = None
        self._qgis_binary_job_manager_lock = threading.RLock()
        self._processing_algorithm_cursor_secret = secrets.token_bytes(32)
        self._feature_delete_confirmations = FeatureDeleteConfirmationStore()
        self._destructive_action_confirmations = (
            DestructiveActionConfirmationStore()
        )

    @property
    def url(self) -> str:
        return f"http://{self.host}:{self.port}"

    def set_auth_token(self, auth_token: str) -> None:
        if self._httpd:
            raise RuntimeError("QCopilots QGIS bridge token cannot change while running")
        if not auth_token or not auth_token.strip():
            raise ValueError("QCopilots QGIS bridge requires a non-empty auth token")
        self._auth_token = auth_token

    def clear_auth_token(self) -> None:
        self._auth_token = ""

    def set_filesystem_policy(self, policy: FilesystemPolicy) -> None:
        if self._httpd:
            raise RuntimeError(
                "QCopilots QGIS bridge filesystem policy cannot change while running"
            )
        if not isinstance(policy, FilesystemPolicy):
            raise TypeError("policy must be a FilesystemPolicy")
        self._filesystem_policy = policy

    def start(self) -> None:
        if not self._auth_token or not self._auth_token.strip():
            raise ValueError("QCopilots QGIS bridge requires a non-empty auth token")
        if self._httpd:
            return
        self._stopping.clear()
        if self._dispatcher is None or self._dispatcher.closed:
            self._dispatcher = _make_main_thread_dispatcher()

        controller = self

        class Handler(BaseHTTPRequestHandler):
            server_version = "QCopilotsQgisBridge/0.1"

            def log_message(self, fmt: str, *args: Any) -> None:
                return

            def do_OPTIONS(self) -> None:
                if not self._request_allowed():
                    return
                self._send_json(
                    HTTPStatus.METHOD_NOT_ALLOWED,
                    {"ok": False, "error": "method_not_allowed"},
                    {"Allow": "GET, POST"},
                )

            def do_GET(self) -> None:
                if not self._request_allowed():
                    return
                if self.path.rstrip("/") != "/health":
                    self._send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not_found"})
                    return
                self._send_json(HTTPStatus.OK, {"ok": True, "status": "ok"})

            def do_POST(self) -> None:
                if not self._request_allowed():
                    return
                if self.path.rstrip("/") != "/call":
                    self._discard_request_body()
                    self._send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not_found"})
                    return
                if self.headers.get_content_type().lower() != "application/json":
                    self._discard_request_body()
                    self._send_json(HTTPStatus.UNSUPPORTED_MEDIA_TYPE, {"ok": False, "error": "unsupported_media_type"})
                    return
                try:
                    length = self._request_body_length()
                    if length < 0:
                        self._send_json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": "bad_content_length"})
                        return
                    if length > MAX_HTTP_REQUEST_BODY_BYTES:
                        self.close_connection = True
                        self._send_json(
                            HTTPStatus.REQUEST_ENTITY_TOO_LARGE,
                            {"ok": False, "error": "request_too_large"},
                        )
                        return
                    payload = json.loads(self.rfile.read(length).decode("utf-8"))
                    result = controller.dispatch(payload.get("tool"), payload.get("arguments") or {})
                    self._send_json(HTTPStatus.OK, {"ok": True, "result": result})
                except Exception as err:
                    self._send_json(HTTPStatus.OK, {"ok": False, "error": str(err)})

            def _request_allowed(self) -> bool:
                host_headers = self.headers.get_all("Host") or []
                expected_host = (
                    f"{controller.host}:{int(self.server.server_address[1])}"
                )
                if len(host_headers) != 1 or not hmac.compare_digest(
                    host_headers[0], expected_host
                ):
                    self.close_connection = True
                    self._discard_request_body()
                    self._send_json(
                        HTTPStatus.FORBIDDEN,
                        {"ok": False, "error": "host_not_allowed"},
                    )
                    return False

                if self.headers.get_all("Origin"):
                    self.close_connection = True
                    self._discard_request_body()
                    self._send_json(
                        HTTPStatus.FORBIDDEN,
                        {"ok": False, "error": "origin_not_allowed"},
                    )
                    return False

                if not self._is_protected_path():
                    return True

                authorization_headers = self.headers.get_all("Authorization") or []
                expected_authorization = f"Bearer {controller._auth_token}"
                if controller._auth_token and len(authorization_headers) == 1 and hmac.compare_digest(
                    authorization_headers[0], expected_authorization
                ):
                    return True

                self.close_connection = True
                self._discard_request_body()
                self._send_json(
                    HTTPStatus.UNAUTHORIZED,
                    {"ok": False, "error": "unauthorized"},
                    {"WWW-Authenticate": "Bearer"},
                )
                return False

            def _is_protected_path(self) -> bool:
                normalized_path = self.path.rstrip("/")
                return normalized_path in ("/call", "/health")

            def _send_json(
                self,
                status: HTTPStatus,
                body: dict[str, Any],
                extra_headers: dict[str, str] | None = None,
            ) -> None:
                data = json.dumps(body, ensure_ascii=False).encode("utf-8")
                self.send_response(status)
                for name, value in (extra_headers or {}).items():
                    self.send_header(name, value)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(data)))
                if self.close_connection:
                    self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(data)

            def _discard_request_body(self) -> None:
                length = self._request_body_length()
                if length <= 0:
                    return
                if length > MAX_HTTP_ERROR_BODY_DRAIN_BYTES:
                    self.close_connection = True
                    return
                old_timeout = self.connection.gettimeout()
                try:
                    self.connection.settimeout(HTTP_ERROR_BODY_DRAIN_TIMEOUT_SECONDS)
                    remaining = length
                    while remaining > 0:
                        chunk = self.rfile.read(min(remaining, 8192))
                        if not chunk:
                            self.close_connection = True
                            break
                        remaining -= len(chunk)
                except OSError:
                    self.close_connection = True
                finally:
                    try:
                        self.connection.settimeout(old_timeout)
                    except OSError:
                        self.close_connection = True

            def _request_body_length(self) -> int:
                transfer_encodings = self.headers.get_all("Transfer-Encoding") or []
                content_lengths = self.headers.get_all("Content-Length") or []
                if transfer_encodings or len(content_lengths) > 1:
                    self.close_connection = True
                    return -1
                if not content_lengths:
                    return 0
                value = content_lengths[0]
                if not re.fullmatch(r"0|[1-9][0-9]*", value):
                    self.close_connection = True
                    return -1
                return int(value)

        try:
            self._httpd = QgisBridgeHttpServer((self.host, self.port), Handler)
        except OSError as err:
            if self.port == 0 or not _is_address_in_use(err):
                raise
            self._httpd = QgisBridgeHttpServer((self.host, 0), Handler)
        self.port = int(self._httpd.server_address[1])
        self._shutdown_requested = False
        self._thread = threading.Thread(target=self._httpd.serve_forever, daemon=True)
        self._thread.start()

    def stop(self, timeout_seconds: float = 5) -> None:
        timeout = float(timeout_seconds)
        if not math.isfinite(timeout) or timeout < 0:
            raise ValueError(
                "QCopilots QGIS bridge stop timeout must be finite and non-negative"
            )
        deadline = time.monotonic() + timeout
        self._stopping.set()
        self._feature_delete_confirmations.clear()
        self._destructive_action_confirmations.clear()
        httpd = self._httpd
        thread = self._thread
        dispatcher = self._dispatcher
        manager_shutdown_errors = []
        processing_job_manager = self._processing_job_manager
        if processing_job_manager is not None:
            try:
                processing_job_manager.shutdown(
                    timeout_seconds=max(0.0, deadline - time.monotonic())
                )
            except Exception as err:
                manager_shutdown_errors.append(err)
            finally:
                with self._processing_job_manager_lock:
                    if self._processing_job_manager is processing_job_manager:
                        self._processing_job_manager = None
        qgis_binary_job_manager = self._qgis_binary_job_manager
        if qgis_binary_job_manager is not None:
            try:
                qgis_binary_job_manager.shutdown(
                    timeout_seconds=max(0.0, deadline - time.monotonic())
                )
            except Exception as err:
                manager_shutdown_errors.append(err)
            finally:
                with self._qgis_binary_job_manager_lock:
                    if self._qgis_binary_job_manager is qgis_binary_job_manager:
                        self._qgis_binary_job_manager = None
        if dispatcher:
            dispatcher.close()
        if (
            httpd
            and thread
            and thread.is_alive()
            and not self._shutdown_requested
        ):
            self._shutdown_requested = True
            try:
                httpd.shutdown()
            except Exception:
                self._shutdown_requested = False
                raise
        if httpd:
            httpd.server_close()
        if thread and thread.is_alive():
            thread.join(timeout=max(0.0, deadline - time.monotonic()))
        if thread and thread.is_alive():
            raise RuntimeError(
                "QCopilots QGIS bridge HTTP thread did not stop within "
                f"{timeout:g} seconds"
            )
        self._httpd = None
        self._thread = None
        self._shutdown_requested = False
        if manager_shutdown_errors:
            raise RuntimeError(
                "QCopilots QGIS bridge job manager shutdown failed: "
                + " | ".join(str(error) for error in manager_shutdown_errors)
            ) from manager_shutdown_errors[0]

    def dispatch(self, tool: str, arguments: dict[str, Any]) -> dict[str, Any]:
        if self._stopping.is_set():
            raise RuntimeError("QCopilots QGIS bridge is stopping")

        def invoke() -> dict[str, Any]:
            if self._stopping.is_set():
                raise RuntimeError("QCopilots QGIS bridge is stopping")
            return QgisBridgeTools(
                self.iface,
                processing_job_manager_factory=self._get_processing_job_manager,
                qgis_binary_job_manager_factory=self._get_qgis_binary_job_manager,
                feature_delete_confirmations=self._feature_delete_confirmations,
                destructive_action_confirmations=(
                    self._destructive_action_confirmations
                ),
                filesystem_policy=self._filesystem_policy,
                processing_algorithm_cursor_secret=(
                    self._processing_algorithm_cursor_secret
                ),
            ).dispatch(tool, arguments)

        if self._dispatcher:
            return self._dispatcher.call(invoke)
        return invoke()

    def _get_processing_job_manager(self):
        with self._processing_job_manager_lock:
            if self._stopping.is_set():
                raise RuntimeError("QCopilots QGIS bridge is stopping")
            if self._processing_job_manager is None:
                from qcopilots_common.processing_jobs import ProcessingJobManager

                self._processing_job_manager = ProcessingJobManager(
                    self.iface,
                    dependencies={
                        "parameter_sanitizer": _sanitize_processing_parameters,
                        "category_matcher": _algorithm_matches_category,
                        "filesystem_policy": self._filesystem_policy,
                    },
                )
            return self._processing_job_manager

    def _get_qgis_binary_job_manager(self):
        with self._qgis_binary_job_manager_lock:
            if self._stopping.is_set():
                raise RuntimeError("QCopilots QGIS bridge is stopping")
            if self._qgis_binary_job_manager is None:
                from qcopilots_common.qgis_binary_jobs import QGISBinaryJobManager

                self._qgis_binary_job_manager = QGISBinaryJobManager(
                    self.iface,
                    dependencies={"filesystem_policy": self._filesystem_policy},
                )
            return self._qgis_binary_job_manager


class _QtMainThreadDispatcher:
    def __init__(self):
        from qgis.PyQt.QtCore import QCoreApplication, QObject, QThread, Qt, pyqtSignal

        class DispatcherObject(QObject):
            request = pyqtSignal(object)

        self._app = QCoreApplication.instance()
        self._qthread = QThread
        self._object = DispatcherObject()
        self._object.request.connect(self._dispatch, Qt.ConnectionType.QueuedConnection)
        self._closed = threading.Event()
        self._pending_lock = threading.Lock()
        self._pending: dict[int, dict[str, Any]] = {}

    @property
    def closed(self) -> bool:
        return self._closed.is_set()

    def call(self, function, timeout_seconds: float = 60):
        if self._closed.is_set():
            raise RuntimeError("QCopilots QGIS bridge is stopping")
        if not self._app or self._qthread.currentThread() == self._app.thread():
            return function()

        done = threading.Event()
        payload = {
            "function": function,
            "done": done,
            "result": None,
            "error": None,
            "cancelled": False,
            "started": False,
            "audit_identifier": secrets.token_hex(16),
        }
        payload_id = id(payload)
        with self._pending_lock:
            if self._closed.is_set():
                raise RuntimeError("QCopilots QGIS bridge is stopping")
            self._pending[payload_id] = payload
        self._object.request.emit(payload)
        if not done.wait(timeout_seconds):
            with self._pending_lock:
                self._pending.pop(payload_id, None)
                payload["cancelled"] = True
                started = bool(payload["started"])
            if started:
                raise TimeoutError(
                    "QCopilots QGIS bridge call timed out after execution started, "
                    "indeterminate_outcome=true, audit_identifier="
                    + payload["audit_identifier"]
                )
            raise TimeoutError(
                "QCopilots QGIS bridge call timed out before execution started, "
                "indeterminate_outcome=false, audit_identifier="
                + payload["audit_identifier"]
            )
        if payload["error"]:
            raise payload["error"]
        return payload["result"]

    def _dispatch(self, payload: dict[str, Any]) -> None:
        payload_id = id(payload)
        with self._pending_lock:
            if payload["cancelled"] or self._closed.is_set():
                self._pending.pop(payload_id, None)
                payload["cancelled"] = True
                if payload["error"] is None:
                    payload["error"] = RuntimeError(
                        "QCopilots QGIS bridge request was cancelled"
                    )
                payload["done"].set()
                return
            payload["started"] = True

        result = None
        error = None
        try:
            result = payload["function"]()
        except Exception as err:
            error = err
        finally:
            with self._pending_lock:
                self._pending.pop(payload_id, None)
                if not payload["cancelled"]:
                    payload["result"] = result
                    payload["error"] = error
            payload["done"].set()

    def close(self) -> None:
        self._closed.set()
        cancellation_error = RuntimeError("QCopilots QGIS bridge is stopping")
        with self._pending_lock:
            pending = list(self._pending.values())
            self._pending.clear()
            for payload in pending:
                payload["cancelled"] = True
                payload["error"] = cancellation_error
                payload["done"].set()


def _make_main_thread_dispatcher():
    try:
        return _QtMainThreadDispatcher()
    except Exception:
        return None


class QgisBridgeTools:
    def __init__(
        self,
        iface: Any,
        processing_job_manager: Any = None,
        processing_job_manager_factory: Any = None,
        qgis_binary_job_manager: Any = None,
        qgis_binary_job_manager_factory: Any = None,
        feature_delete_confirmations: FeatureDeleteConfirmationStore | None = None,
        destructive_action_confirmations: (
            DestructiveActionConfirmationStore | None
        ) = None,
        filesystem_policy: FilesystemPolicy | None = None,
        processing_algorithm_cursor_secret: bytes | None = None,
    ):
        self.iface = iface
        self._processing_job_manager_instance = processing_job_manager
        self._processing_job_manager_factory = processing_job_manager_factory
        self._qgis_binary_job_manager_instance = qgis_binary_job_manager
        self._qgis_binary_job_manager_factory = qgis_binary_job_manager_factory
        self._feature_delete_confirmations = (
            feature_delete_confirmations or FeatureDeleteConfirmationStore()
        )
        self._destructive_action_confirmations = (
            destructive_action_confirmations
            or DestructiveActionConfirmationStore()
        )
        self._filesystem_policy = filesystem_policy or FilesystemPolicy()
        if processing_algorithm_cursor_secret is None:
            processing_algorithm_cursor_secret = secrets.token_bytes(32)
        if not isinstance(processing_algorithm_cursor_secret, bytes):
            raise TypeError("processing_algorithm_cursor_secret must be bytes")
        if not processing_algorithm_cursor_secret:
            raise ValueError("processing_algorithm_cursor_secret must not be empty")
        self._processing_algorithm_cursor_secret = (
            processing_algorithm_cursor_secret
        )

    def dispatch(self, tool: str, arguments: dict[str, Any]) -> dict[str, Any]:
        handlers = {
            "list_layers": self.list_layers,
            "add_vector_layer": self.add_vector_layer,
            "add_raster_layer": self.add_raster_layer,
            "remove_layer": self.remove_layer,
            "set_layer_visibility": self.set_layer_visibility,
            "zoom_to_layer": self.zoom_to_layer,
            "zoom_to_extent": self.zoom_to_extent,
            "zoom_in": self.zoom_in,
            "zoom_out": self.zoom_out,
            "zoom_full": self.zoom_full,
            "zoom_to_selection": self.zoom_to_selection,
            "zoom_to_native_resolution": self.zoom_to_native_resolution,
            "zoom_to_last_extent": self.zoom_to_last_extent,
            "zoom_to_next_extent": self.zoom_to_next_extent,
            "save_project": self.save_project,
            "refresh_canvas": self.refresh_canvas,
            "export_map_image": self.export_map_image,
            "interactive_layer_describe_sources": self.interactive_layer_describe_sources,
            "interactive_layer_list_layers": self.interactive_layer_list_layers,
            "interactive_layer_load_layer": self.interactive_layer_load_layer,
            "interactive_layer_remove_layers": self.interactive_layer_remove_layers,
            "interactive_layer_get_metadata": self.interactive_layer_get_metadata,
            "interactive_layer_query_features": self.interactive_layer_query_features,
            "interactive_layer_set_selection": self.interactive_layer_set_selection,
            "interactive_layer_delete_features": self.interactive_layer_delete_features,
            "interactive_project_get_crs": self.interactive_project_get_crs,
            "interactive_project_set_crs": self.interactive_project_set_crs,
            "interactive_raster_statistics": self.interactive_raster_statistics,
            "interactive_layer_apply_style": self.interactive_layer_apply_style,
            "interactive_layer_configure_labels": self.interactive_layer_configure_labels,
            "interactive_layout_create": self.interactive_layout_create,
            "interactive_layout_list": self.interactive_layout_list,
            "interactive_layout_export": self.interactive_layout_export,
            "create_vector_layer": self.create_vector_layer,
            "add_vector_features": self.add_vector_features,
            "update_vector_features": self.update_vector_features,
            "processing_list_algorithms": self.processing_list_algorithms,
            "processing_algorithm_details": self.processing_algorithm_details,
            "processing_start_algorithm": self.processing_start_algorithm,
            "processing_get_job": self.processing_get_job,
            "processing_list_jobs": self.processing_list_jobs,
            "processing_cancel_job": self.processing_cancel_job,
            "qgis_binary_list_binaries": self.qgis_binary_list_binaries,
            "qgis_binary_get_binary_details": self.qgis_binary_get_binary_details,
            "qgis_binary_start": self.qgis_binary_start,
            "qgis_binary_get_job": self.qgis_binary_get_job,
            "qgis_binary_list_jobs": self.qgis_binary_list_jobs,
            "qgis_binary_cancel_job": self.qgis_binary_cancel_job,
        }
        if tool not in handlers:
            raise ValueError(f"Unknown QGIS bridge tool: {tool}")
        return handlers[tool](arguments)

    def list_layers(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        layers = []
        for layer in QgsProject.instance().mapLayers().values():
            layers.append(_layer_summary(layer))
        return {"layers": layers}

    def add_vector_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject, QgsVectorLayer

        path = self._safe_workspace_path(arguments["path"], access="read")
        name = arguments.get("name") or Path(path).stem
        layer = QgsVectorLayer(path, name, arguments.get("provider", "ogr"))
        if not layer.isValid():
            raise RuntimeError(f"Invalid vector layer: {path}")
        QgsProject.instance().addMapLayer(layer)
        return {"layer_id": layer.id(), "name": layer.name()}

    def add_raster_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject, QgsRasterLayer

        path = self._safe_workspace_path(arguments["path"], access="read")
        name = arguments.get("name") or Path(path).stem
        layer = QgsRasterLayer(path, name)
        if not layer.isValid():
            raise RuntimeError(f"Invalid raster layer: {path}")
        QgsProject.instance().addMapLayer(layer)
        return {"layer_id": layer.id(), "name": layer.name()}

    def remove_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self.interactive_layer_remove_layers(
            {
                "layer_id": arguments["layer_id"],
                **(
                    {"confirmation_token": arguments["confirmation_token"]}
                    if arguments.get("confirmation_token")
                    else {}
                ),
            }
        )

    def set_layer_visibility(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        node = QgsProject.instance().layerTreeRoot().findLayer(arguments["layer_id"])
        if not node:
            raise RuntimeError("Layer tree node not found")
        node.setItemVisibilityChecked(bool(arguments.get("visible", True)))
        return {"layer_id": arguments["layer_id"], "visible": node.itemVisibilityChecked()}

    def zoom_to_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        layer = QgsProject.instance().mapLayer(arguments["layer_id"])
        if not layer:
            raise RuntimeError("Layer not found")
        self.iface.mapCanvas().setExtent(layer.extent())
        self.iface.mapCanvas().refresh()
        return {"layer_id": layer.id()}

    def zoom_to_extent(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsRectangle

        extent = arguments["extent"]
        if not isinstance(extent, list) or len(extent) != 4:
            raise RuntimeError(
                "extent must contain exactly four numbers: xmin, ymin, xmax, ymax"
            )
        rectangle = QgsRectangle(extent[0], extent[1], extent[2], extent[3])
        self.iface.mapCanvas().setExtent(rectangle)
        self.iface.mapCanvas().refresh()
        return {"extent": extent}

    def zoom_in(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        self.iface.mapCanvas().zoomIn()
        self.iface.mapCanvas().refresh()
        return {"zoomed": "in"}

    def zoom_out(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        self.iface.mapCanvas().zoomOut()
        self.iface.mapCanvas().refresh()
        return {"zoomed": "out"}

    def zoom_full(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        if hasattr(self.iface, "zoomFull"):
            self.iface.zoomFull()
        else:
            self.iface.mapCanvas().zoomToFullExtent()
        self.iface.mapCanvas().refresh()
        return {"zoomed": "full"}

    def zoom_to_selection(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        layer = None
        if arguments.get("layer_id"):
            layer = QgsProject.instance().mapLayer(arguments["layer_id"])
            if not layer:
                raise RuntimeError("Layer not found")
        self.iface.mapCanvas().zoomToSelected(layer)
        self.iface.mapCanvas().refresh()
        return {"layer_id": layer.id() if layer else None}

    def zoom_to_last_extent(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        if hasattr(self.iface, "zoomToPrevious"):
            self.iface.zoomToPrevious()
        else:
            self.iface.mapCanvas().zoomToPreviousExtent()
        self.iface.mapCanvas().refresh()
        return {"zoomed": "last"}

    def zoom_to_next_extent(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        if hasattr(self.iface, "zoomToNext"):
            self.iface.zoomToNext()
        else:
            self.iface.mapCanvas().zoomToNextExtent()
        self.iface.mapCanvas().refresh()
        return {"zoomed": "next"}

    def zoom_to_native_resolution(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject, QgsRasterLayer

        layer = QgsProject.instance().mapLayer(arguments.get("layer_id", ""))
        if layer is None and hasattr(self.iface, "activeLayer"):
            layer = self.iface.activeLayer()
        if not isinstance(layer, QgsRasterLayer):
            raise RuntimeError("A raster layer is required for native resolution zoom")

        canvas = self.iface.mapCanvas()
        factor = _native_resolution_zoom_factor(layer, canvas)
        canvas.zoomByFactor(factor)
        canvas.refresh()
        return {"layer_id": layer.id(), "zoom_factor": factor}

    def save_project(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        project = QgsProject.instance()
        path = arguments.get("path")
        if path:
            path = self._safe_workspace_path(path, access="write")
        elif project.fileName():
            path = self._safe_workspace_path(
                project.fileName(), access="write"
            )
        if not path:
            raise RuntimeError("A project output path is required for an unsaved project")
        target = Path(path)
        if target.suffix.lower() != ".qgz":
            raise RuntimeError(
                "Project output path must end in .qgz so it can be published as "
                "one atomic file"
            )
        preview, expected_versions = _authorize_interactive_overwrite(
            self._destructive_action_confirmations,
            arguments,
            target,
            "save_project",
            "Project output",
            binding={"path": str(target)},
        )
        if preview is not None:
            return preview
        target.parent.mkdir(parents=True, exist_ok=True)
        staged_path = _new_staged_output_path(target)
        previous_file_name = project.fileName()
        previous_dirty = bool(getattr(project, "isDirty", lambda: False)())
        published = False
        cleanup_residuals = []
        primary_error = None
        try:
            if not project.write(str(staged_path)):
                raise RuntimeError("Project write failed")
            cleanup_residuals.extend(
                _publish_staged_output(
                    staged_path,
                    target,
                    overwrite=bool(arguments.get("overwrite", False)),
                    expected_versions=expected_versions,
                )
            )
            published = True
            set_file_name = getattr(project, "setFileName", None)
            if callable(set_file_name):
                set_file_name(str(target))
            set_dirty = getattr(project, "setDirty", None)
            if callable(set_dirty):
                set_dirty(False)
        except Exception as err:
            primary_error = err
            raise
        finally:
            stage_residuals = _cleanup_interactive_paths([staged_path])
            if not published:
                set_file_name = getattr(project, "setFileName", None)
                if callable(set_file_name):
                    set_file_name(previous_file_name)
                set_dirty = getattr(project, "setDirty", None)
                if callable(set_dirty):
                    set_dirty(previous_dirty)
            if stage_residuals:
                if primary_error is not None:
                    raise RuntimeError(
                        f"{primary_error}. Project staging cleanup was incomplete: "
                        + ", ".join(stage_residuals)
                    ) from primary_error
                cleanup_residuals.extend(stage_residuals)
        return {
            "saved": True,
            "path": str(target),
            "cleanup": {
                "complete": not cleanup_residuals,
                "residual_paths": sorted(set(cleanup_residuals)),
                "retry_recommended": bool(cleanup_residuals),
            },
        }

    def refresh_canvas(self, arguments: dict[str, Any]) -> dict[str, Any]:
        self.iface.mapCanvas().refresh()
        return {"refreshed": True}

    def export_map_image(self, arguments: dict[str, Any]) -> dict[str, Any]:
        path = Path(
            self._safe_workspace_path(arguments["path"], access="write")
        )
        encoders = {
            ".png": "PNG",
            ".jpg": "JPEG",
            ".jpeg": "JPEG",
            ".bmp": "BMP",
            ".tif": "TIFF",
            ".tiff": "TIFF",
        }
        encoder = encoders.get(path.suffix.lower())
        if encoder is None:
            raise RuntimeError(
                "Map image output must use a .png, .jpg, .jpeg, .bmp, .tif or .tiff suffix"
            )
        world_path = _map_image_world_file_path(path)
        family_paths = [path, world_path]
        preview, expected_versions = _authorize_interactive_overwrite(
            self._destructive_action_confirmations,
            arguments,
            path,
            "export_map_image",
            "Map image output",
            binding={"path": str(path), "encoder": encoder},
            family_paths=family_paths,
        )
        if preview is not None:
            return preview
        path.parent.mkdir(parents=True, exist_ok=True)
        staging_root = Path(
            tempfile.mkdtemp(
                prefix=f".{path.stem}.qcopilots-stage-",
                dir=str(path.parent),
            )
        )
        staged_path = staging_root / path.name
        staged_world_path = _map_image_world_file_path(staged_path)
        cleanup_residuals = []
        primary_error = None
        try:
            self.iface.mapCanvas().saveAsImage(str(staged_path), None, encoder)
            if not staged_path.is_file() or staged_path.stat().st_size <= 0:
                raise RuntimeError("Map image export produced no output")
            if (
                not staged_world_path.is_file()
                or staged_world_path.stat().st_size <= 0
            ):
                raise RuntimeError("Map image export produced no world file")
            cleanup_residuals = _publish_staged_output_family(
                staging_root,
                path,
                family_paths,
                overwrite=bool(arguments.get("overwrite", False)),
                expected_versions=expected_versions,
            )
        except Exception as err:
            primary_error = err
            raise
        finally:
            stage_residuals = _cleanup_interactive_paths([staging_root])
            if stage_residuals:
                if primary_error is not None:
                    raise RuntimeError(
                        f"{primary_error}. Map image staging cleanup was incomplete: "
                        + ", ".join(stage_residuals)
                    ) from primary_error
                cleanup_residuals.extend(stage_residuals)
        return {
            "path": str(path),
            "world_file_path": str(world_path),
            "encoder": encoder,
            "saved": path.is_file() and path.stat().st_size > 0,
            "bytes_written": path.stat().st_size if path.is_file() else 0,
            "cleanup": {
                "complete": not cleanup_residuals,
                "residual_paths": sorted(set(cleanup_residuals)),
                "retry_recommended": bool(cleanup_residuals),
            },
        }

    def interactive_layer_describe_sources(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        provider_keys = []
        try:
            from qgis.core import QgsProviderRegistry

            provider_keys = sorted(QgsProviderRegistry.instance().providerList())
        except Exception:
            provider_keys = []
        return {
            "supported_layer_types": _supported_layer_type_descriptions(),
            "available_providers": provider_keys,
        }

    def interactive_layer_list_layers(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self.list_layers(arguments)

    def interactive_layer_load_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        layer_type = _normalize_layer_type(arguments["layer_type"])
        source = str(arguments["source"])
        provider = str(arguments.get("provider") or _default_layer_provider(layer_type))
        name = str(arguments.get("name") or _layer_name_from_source(source, layer_type))
        uri_options = arguments.get("uri_options") or {}
        if not isinstance(uri_options, dict):
            raise RuntimeError("uri_options must be an object")
        if self._filesystem_policy.restricted:
            _validate_formal_layer_uri_options(
                source,
                uri_options,
                layer_type=layer_type,
                provider=provider,
                policy=self._filesystem_policy,
            )
        source = self._validate_layer_source_path(
            source,
            layer_type=layer_type,
            provider=provider,
        )
        uri_options = self._validate_layer_uri_options(
            uri_options,
            layer_type=layer_type,
            provider=provider,
        )

        layer, resolved_layer_type, resolved_provider, resolved_source = _create_qgis_layer(
            layer_type,
            source,
            name,
            provider,
            uri_options,
        )
        if not layer.isValid():
            raise RuntimeError(
                f"Invalid {resolved_layer_type} layer from provider {resolved_provider}: {resolved_source}"
            )

        added = bool(arguments.get("add_to_project", True))
        if added:
            QgsProject.instance().addMapLayer(layer)
        if bool(arguments.get("refresh_canvas", False)) and self.iface:
            self.iface.mapCanvas().refresh()
        return {
            "added": added,
            "layer": _layer_summary(layer),
            "layer_type": resolved_layer_type,
            "provider": resolved_provider,
            "source": resolved_source,
        }

    def interactive_layer_remove_layers(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        project = QgsProject.instance()
        confirmation_token = str(
            arguments.get("confirmation_token") or ""
        ).strip()
        if confirmation_token:
            if set(arguments) != {"confirmation_token"}:
                raise RuntimeError(
                    "A confirmed layer removal must contain only confirmation_token"
                )
            entry = self._destructive_action_confirmations.consume_payload(
                confirmation_token,
                "remove_map_layers",
            )
            payload = entry["payload"]
            layer_ids = payload["layer_ids"]
            editable_changes = payload["editable_changes"]
            layers = [
                project.mapLayer(layer_id)
                for layer_id in layer_ids
            ]
            if any(layer is None for layer in layers) or (
                _layer_removal_fingerprint(layers) != entry["fingerprint"]
            ):
                raise RuntimeError(
                    "A layer changed after removal preview. Request a new confirmation token."
                )
        else:
            editable_changes = str(
                arguments.get("editable_changes") or "reject"
            ).strip()
            if editable_changes not in LAYER_REMOVAL_EDITABLE_CHANGES:
                raise RuntimeError(
                    "editable_changes must be reject, save or discard"
                )
            selectors = _layer_remove_selectors(arguments)
            if not any(selectors.values()):
                raise RuntimeError(
                    "Specify at least one layer_id, layer_ids, name, names, source or sources value"
                )
            layers = _layers_matching_selectors(
                list(project.mapLayers().values()),
                selectors,
                bool(arguments.get("allow_multiple", False)),
            )
            layers = sorted(layers, key=lambda layer: str(layer.id()))
            _validate_layer_removal_edit_policy(layers, editable_changes)
            layer_ids = [str(layer.id()) for layer in layers]
            token = self._destructive_action_confirmations.issue(
                "remove_map_layers",
                _layer_removal_fingerprint(layers),
                {
                    "layer_ids": layer_ids,
                    "editable_changes": editable_changes,
                },
            )
            return {
                "preview": True,
                "confirmation_required": True,
                "editable_changes": editable_changes,
                "layers": [
                    {
                        **_layer_summary(layer),
                        "edit_state": _layer_edit_state(layer),
                    }
                    for layer in layers
                ],
                "confirmation_token": token,
                "confirmation_expires_in_seconds": (
                    self._destructive_action_confirmations.ttl_seconds
                ),
            }

        editable_layers_processed = _apply_layer_removal_edit_policy(
            layers,
            editable_changes,
        )
        removed = []
        for layer in layers:
            removed.append(_layer_summary(layer))
            project.removeMapLayer(layer.id())
        return {
            "removed": removed,
            "removed_count": len(removed),
            "editable_changes": editable_changes,
            "editable_layers_processed": editable_layers_processed,
            "confirmation_consumed": bool(confirmation_token),
        }

    def interactive_layer_get_metadata(self, arguments: dict[str, Any]) -> dict[str, Any]:
        layer = _map_layer_from_id(arguments["layer_id"])
        return {
            "layer": _detailed_layer_metadata(
                layer,
                include_source=bool(arguments.get("include_source", False)),
            )
        }

    def interactive_layer_query_features(self, arguments: dict[str, Any]) -> dict[str, Any]:
        layer = _vector_layer_from_id(arguments["layer_id"])
        return _query_vector_features(layer, arguments)

    def interactive_layer_set_selection(self, arguments: dict[str, Any]) -> dict[str, Any]:
        layer = _vector_layer_from_id(arguments["layer_id"])
        mode = str(arguments.get("mode") or "replace")
        if mode == "clear":
            matched_ids: list[int] = []
        else:
            matched_ids, truncated = _matching_vector_feature_ids(
                layer,
                arguments,
                maximum=MAX_SELECTION_MATCHES,
            )
            if truncated:
                raise RuntimeError(
                    f"Selection matches more than {MAX_SELECTION_MATCHES} features"
                )
            if not arguments.get("feature_ids") and not arguments.get("filter_expression"):
                raise RuntimeError(
                    "feature_ids or filter_expression is required unless mode is clear"
                )
        current = set()
        if mode in {"add", "remove"}:
            if int(layer.selectedFeatureCount()) > MAX_SELECTION_MATCHES:
                raise RuntimeError(
                    "The existing selection is too large for add/remove. Use replace "
                    "or clear."
                )
            current = set(int(value) for value in layer.selectedFeatureIds())
        matches = set(matched_ids)
        if mode == "clear":
            selected = set()
        elif mode == "replace":
            selected = matches
        elif mode == "add":
            selected = current | matches
        elif mode == "remove":
            selected = current - matches
        else:
            raise RuntimeError(f"Unsupported selection mode: {mode}")
        layer.selectByIds(sorted(selected))
        if self.iface:
            self.iface.mapCanvas().refresh()
        selected_ids = sorted(selected)
        return {
            "layer_id": layer.id(),
            "mode": mode,
            "matched_count": len(matched_ids),
            "selected_count": len(selected),
            "selected_feature_ids": selected_ids[:MAX_SELECTION_SUMMARY_IDS],
            "selected_feature_ids_truncated": (
                len(selected_ids) > MAX_SELECTION_SUMMARY_IDS
            ),
        }

    def interactive_layer_delete_features(self, arguments: dict[str, Any]) -> dict[str, Any]:
        layer = _vector_layer_from_id(arguments["layer_id"])
        confirmation_token = str(arguments.get("confirmation_token") or "").strip()
        if confirmation_token:
            if arguments.get("feature_ids") or arguments.get("filter_expression"):
                raise RuntimeError(
                    "confirmation_token cannot be combined with feature_ids or "
                    "filter_expression"
                )
            confirmed = self._feature_delete_confirmations.consume(
                confirmation_token,
                layer.id(),
            )
            feature_ids = confirmed["feature_ids"]
            current_fingerprint = _vector_feature_fingerprint(layer, feature_ids)
            if current_fingerprint != confirmed["fingerprint"]:
                raise RuntimeError(
                    "Features changed after preview. Request a new delete preview."
                )

            def delete_features() -> None:
                if not layer.deleteFeatures(feature_ids):
                    raise RuntimeError("Could not delete vector features")

            started_edit_session = _atomic_vector_layer_edit(
                layer,
                "QCopilots delete vector features",
                delete_features,
            )
            layer.updateExtents()
            if self.iface:
                self.iface.mapCanvas().refresh()
            return {
                "layer": _layer_summary(layer),
                "deleted_feature_ids": feature_ids,
                "deleted_count": len(feature_ids),
                "confirmation_consumed": True,
                **_vector_edit_result_state(started_edit_session),
            }

        if not arguments.get("feature_ids") and not arguments.get("filter_expression"):
            raise RuntimeError(
                "feature_ids or filter_expression is required for a delete preview"
            )
        feature_ids, truncated = _matching_vector_feature_ids(
            layer,
            arguments,
            maximum=10000,
        )
        if truncated:
            raise RuntimeError(
                "Delete preview matches more than 10000 features. Use an exact smaller selector."
            )
        if not feature_ids:
            return {
                "preview": True,
                "layer": _layer_summary(layer),
                "matched_count": 0,
                "feature_ids": [],
                "confirmation_token": None,
            }
        fingerprint = _vector_feature_fingerprint(layer, feature_ids)
        token = self._feature_delete_confirmations.issue(
            layer.id(),
            feature_ids,
            fingerprint,
        )
        return {
            "preview": True,
            "layer": _layer_summary(layer),
            "matched_count": len(feature_ids),
            "feature_ids": feature_ids[:100],
            "feature_ids_truncated": len(feature_ids) > 100,
            "confirmation_token": token,
            "confirmation_expires_in_seconds": self._feature_delete_confirmations.ttl_seconds,
        }

    def interactive_project_get_crs(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        from qgis.core import QgsProject

        return {"crs": _crs_metadata(QgsProject.instance().crs())}

    def interactive_project_set_crs(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsCoordinateReferenceSystem, QgsProject

        project = QgsProject.instance()
        previous = _crs_metadata(project.crs())
        crs = QgsCoordinateReferenceSystem(str(arguments["crs"]))
        if not crs.isValid():
            raise RuntimeError(f"Invalid project CRS: {arguments['crs']}")
        project.setCrs(crs)
        if self.iface:
            self.iface.mapCanvas().refresh()
        return {"previous_crs": previous, "crs": _crs_metadata(project.crs())}

    def interactive_raster_statistics(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import Qgis

        layer = _raster_layer_from_id(arguments["layer_id"])
        band = int(arguments.get("band", 1))
        if band < 1 or band > layer.bandCount():
            raise RuntimeError(
                f"Raster band must be between 1 and {layer.bandCount()}: {band}"
            )
        sample_size = int(arguments.get("sample_size", 100000))
        if not 1 <= sample_size <= MAX_RASTER_STATISTICS_SAMPLES:
            raise RuntimeError(
                "sample_size must be between 1 and "
                f"{MAX_RASTER_STATISTICS_SAMPLES}"
            )
        stats = layer.dataProvider().bandStatistics(
            band,
            Qgis.RasterBandStatistic.All,
            layer.extent(),
            sample_size,
        )
        return {
            "layer": _layer_summary(layer),
            "band": band,
            "sample_size": sample_size,
            "element_count": int(stats.elementCount),
            "minimum": _finite_number(stats.minimumValue),
            "maximum": _finite_number(stats.maximumValue),
            "range": _finite_number(stats.range),
            "sum": _finite_number(stats.sum),
            "mean": _finite_number(stats.mean),
            "standard_deviation": _finite_number(stats.stdDev),
            "sum_of_squares": _finite_number(stats.sumOfSquares),
            "sample_width": int(stats.width),
            "sample_height": int(stats.height),
            "extent": _extent_values(stats.extent),
        }

    def interactive_layer_apply_style(self, arguments: dict[str, Any]) -> dict[str, Any]:
        layer = _map_layer_from_id(arguments["layer_id"])
        style_path = Path(
            self._safe_workspace_path(arguments["style_path"], access="read")
        )
        if not style_path.is_file():
            raise FileNotFoundError(f"Layer style file not found: {style_path}")
        if style_path.suffix.lower() not in {".qml", ".sld"}:
            raise RuntimeError("Layer style_path must use a .qml or .sld suffix")
        message, loaded = layer.loadNamedStyle(str(style_path))
        if not loaded:
            raise RuntimeError(f"Could not load layer style: {message}")
        layer.triggerRepaint()
        if self.iface:
            self.iface.mapCanvas().refresh()
        return {
            "layer": _layer_summary(layer),
            "style_path": str(style_path),
            "loaded": True,
            "message": str(message),
        }

    def interactive_layer_configure_labels(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.PyQt.QtGui import QColor
        from qgis.core import (
            QgsExpression,
            QgsPalLayerSettings,
            QgsTextFormat,
            QgsVectorLayerSimpleLabeling,
        )

        layer = _vector_layer_from_id(arguments["layer_id"])
        enabled = bool(arguments.get("enabled", True))
        if not enabled:
            layer.setLabelsEnabled(False)
        else:
            value = str(arguments.get("field_or_expression") or "").strip()
            if not value:
                raise RuntimeError("field_or_expression is required when labels are enabled")
            is_expression = bool(arguments.get("is_expression", False))
            if is_expression:
                expression = QgsExpression(value)
                if expression.hasParserError():
                    raise RuntimeError(
                        "Invalid label expression: " + expression.parserErrorString()
                    )
                _validate_expression_fields(
                    expression,
                    layer.fields(),
                    "field_or_expression",
                )
            else:
                if value not in _field_index_by_name(layer.fields()):
                    raise RuntimeError(f"Unknown vector field: {value}")
            settings = QgsPalLayerSettings()
            settings.fieldName = value
            settings.isExpression = is_expression
            text_format = QgsTextFormat()
            text_format.setSize(float(arguments.get("font_size", 10)))
            color = QColor(str(arguments.get("color") or "#202020"))
            if not color.isValid():
                raise RuntimeError(f"Invalid label color: {arguments.get('color')}")
            text_format.setColor(color)
            settings.setFormat(text_format)
            layer.setLabeling(QgsVectorLayerSimpleLabeling(settings))
            layer.setLabelsEnabled(True)
        layer.triggerRepaint()
        if self.iface:
            self.iface.mapCanvas().refresh()
        return {
            "layer": _layer_summary(layer),
            "labels_enabled": layer.labelsEnabled(),
            "field_or_expression": arguments.get("field_or_expression"),
            "is_expression": bool(arguments.get("is_expression", False)),
        }

    def interactive_layout_list(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        from qgis.core import QgsProject

        layouts = []
        for layout in QgsProject.instance().layoutManager().printLayouts():
            layouts.append(
                {
                    "name": layout.name(),
                    "page_count": layout.pageCollection().pageCount(),
                }
            )
        return {"layouts": layouts}

    def interactive_layout_create(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import (
            QgsLayoutItemLabel,
            QgsLayoutItemLegend,
            QgsLayoutItemMap,
            QgsLayoutItemScaleBar,
            QgsLayoutPoint,
            QgsLayoutSize,
            QgsPrintLayout,
            QgsProject,
            QgsRectangle,
            QgsUnitTypes,
        )

        project = QgsProject.instance()
        manager = project.layoutManager()
        name = str(arguments["name"]).strip()
        if manager.layoutByName(name) is not None:
            raise FileExistsError(f"Print layout already exists: {name}")
        page_width, page_height = _layout_page_dimensions(
            str(arguments.get("page_size") or "A4"),
            str(arguments.get("orientation") or "landscape"),
        )
        layout = QgsPrintLayout(project)
        layout.initializeDefaults()
        layout.setName(name)
        page = layout.pageCollection().page(0)
        page.setPageSize(
            QgsLayoutSize(
                page_width,
                page_height,
                QgsUnitTypes.LayoutUnit.LayoutMillimeters,
            )
        )

        margin = 10.0
        title = str(arguments.get("title") or "").strip()
        content_top = margin
        if title:
            title_item = QgsLayoutItemLabel(layout)
            title_item.setText(title)
            title_item.attemptMove(QgsLayoutPoint(margin, margin))
            title_item.attemptResize(QgsLayoutSize(page_width - 2 * margin, 10))
            layout.addLayoutItem(title_item)
            content_top += 12.0

        include_legend = bool(arguments.get("include_legend", True))
        include_scale_bar = bool(arguments.get("include_scale_bar", True))
        legend_width = 45.0 if include_legend else 0.0
        scale_height = 12.0 if include_scale_bar else 0.0
        map_width = page_width - 2 * margin - legend_width
        map_height = page_height - content_top - margin - scale_height
        if map_width < 40 or map_height < 40:
            raise RuntimeError("Page is too small for the constrained layout items")

        map_item = QgsLayoutItemMap(layout)
        map_item.attemptMove(QgsLayoutPoint(margin, content_top))
        map_item.attemptResize(QgsLayoutSize(map_width, map_height))
        extent_values = arguments.get("map_extent")
        if extent_values is not None:
            extent = QgsRectangle(*[float(value) for value in extent_values])
            if extent.isEmpty() or not all(
                math.isfinite(float(value)) for value in extent_values
            ):
                raise RuntimeError("map_extent must describe a finite non-empty extent")
            map_item.setExtent(extent)
        elif self.iface is not None and self.iface.mapCanvas() is not None:
            canvas_extent = self.iface.mapCanvas().extent()
            if not canvas_extent.isEmpty():
                map_item.setExtent(canvas_extent)
        layout.addLayoutItem(map_item)

        if include_legend:
            legend = QgsLayoutItemLegend(layout)
            legend.setTitle("Legend")
            legend.setLinkedMap(map_item)
            legend.attemptMove(
                QgsLayoutPoint(margin + map_width + 3.0, content_top)
            )
            legend.attemptResize(QgsLayoutSize(legend_width - 3.0, map_height))
            layout.addLayoutItem(legend)

        if include_scale_bar:
            scale_bar = QgsLayoutItemScaleBar(layout)
            scale_bar.setStyle("Numeric")
            scale_bar.setLinkedMap(map_item)
            scale_bar.attemptMove(
                QgsLayoutPoint(margin, content_top + map_height + 3.0)
            )
            scale_bar.attemptResize(QgsLayoutSize(min(map_width, 80.0), 8.0))
            layout.addLayoutItem(scale_bar)

        if not manager.addLayout(layout):
            raise RuntimeError(f"Could not add print layout: {name}")
        return {
            "created": True,
            "layout": {
                "name": name,
                "page_count": 1,
                "page_size": str(arguments.get("page_size") or "A4"),
                "orientation": str(arguments.get("orientation") or "landscape"),
                "title": title,
                "has_map_frame": True,
                "has_legend": include_legend,
                "has_scale_bar": include_scale_bar,
            },
        }

    def interactive_layout_export(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsLayoutExporter, QgsProject

        layout = QgsProject.instance().layoutManager().layoutByName(
            str(arguments["layout_name"])
        )
        if layout is None:
            raise RuntimeError(f"Print layout not found: {arguments['layout_name']}")
        path, export_format = _layout_export_target(
            self._safe_workspace_path(arguments["path"], access="write"),
            arguments.get("format"),
        )
        if layout.pageCollection().pageCount() != 1 and export_format != "pdf":
            raise RuntimeError("Multi-page layouts must be exported as PDF")
        preview, expected_versions = _authorize_interactive_overwrite(
            self._destructive_action_confirmations,
            arguments,
            path,
            "export_print_layout",
            "Print layout output",
            binding={
                "layout_name": layout.name(),
                "path": str(path),
                "format": export_format,
                "dpi": float(arguments.get("dpi", 300)),
            },
        )
        if preview is not None:
            return preview
        path.parent.mkdir(parents=True, exist_ok=True)
        staged_path = _new_staged_output_path(path)
        exporter = QgsLayoutExporter(layout)
        dpi = float(arguments.get("dpi", 300))
        if not math.isfinite(dpi) or not 72 <= dpi <= 1200:
            raise RuntimeError("Layout export dpi must be between 72 and 1200")
        _validate_layout_export_pixels(layout, dpi)
        cleanup_residuals = []
        primary_error = None
        try:
            if export_format == "pdf":
                settings = QgsLayoutExporter.PdfExportSettings()
                settings.dpi = dpi
                result = exporter.exportToPdf(str(staged_path), settings)
            elif export_format == "svg":
                settings = QgsLayoutExporter.SvgExportSettings()
                settings.dpi = dpi
                result = exporter.exportToSvg(str(staged_path), settings)
            else:
                settings = QgsLayoutExporter.ImageExportSettings()
                settings.dpi = dpi
                result = exporter.exportToImage(str(staged_path), settings)
            if result != QgsLayoutExporter.ExportResult.Success:
                raise RuntimeError(f"Print layout export failed with result: {result}")
            if not staged_path.is_file() or staged_path.stat().st_size <= 0:
                raise RuntimeError("Print layout export produced no output")
            cleanup_residuals.extend(
                _publish_staged_output(
                    staged_path,
                    path,
                    overwrite=bool(arguments.get("overwrite", False)),
                    expected_versions=expected_versions,
                )
            )
        except Exception as err:
            primary_error = err
            raise
        finally:
            stage_residuals = _cleanup_interactive_paths([staged_path])
            if stage_residuals:
                if primary_error is not None:
                    raise RuntimeError(
                        f"{primary_error}. Layout staging cleanup was incomplete: "
                        + ", ".join(stage_residuals)
                    ) from primary_error
                cleanup_residuals.extend(stage_residuals)
        return {
            "layout_name": layout.name(),
            "path": str(path),
            "format": export_format,
            "dpi": dpi,
            "saved": path.is_file() and path.stat().st_size > 0,
            "bytes_written": path.stat().st_size if path.is_file() else 0,
            "cleanup": {
                "complete": not cleanup_residuals,
                "residual_paths": sorted(set(cleanup_residuals)),
                "retry_recommended": bool(cleanup_residuals),
            },
        }

    def create_vector_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject, QgsVectorLayer

        name = str(arguments["name"])
        output_path = ""
        target = None
        expected_versions = []
        requested_path = str(arguments.get("path") or "").strip()
        if requested_path.lower() == "memory:":
            output_path = "memory:"
        elif requested_path:
            output_path = self._safe_workspace_path(
                requested_path, access="write"
            )
            target = Path(output_path)
            from qcopilots_common.processing_jobs import (
                _processing_output_family_paths,
            )

            preview, expected_versions = _authorize_interactive_overwrite(
                self._destructive_action_confirmations,
                arguments,
                target,
                "create_vector_layer",
                "Vector layer output",
                binding={
                    key: value
                    for key, value in arguments.items()
                    if key != "overwrite_confirmation_token"
                },
                family_paths=_processing_output_family_paths(target),
            )
            if preview is not None:
                return preview

        geometry_type = _vector_geometry_type(arguments.get("geometry_type", "Point"))
        crs = str(arguments.get("crs") or "EPSG:4326")
        layer = QgsVectorLayer(f"{geometry_type}?crs={crs}", name, "memory")
        if not layer.isValid():
            raise RuntimeError(f"Could not create vector layer: {name}")

        provider = layer.dataProvider()
        fields = [_qgs_field(field) for field in arguments.get("fields") or []]
        if fields:
            if not provider.addAttributes(fields):
                raise RuntimeError("Could not add vector layer fields")
            layer.updateFields()

        features = [
            _feature_from_payload(layer.fields(), feature)
            for feature in arguments.get("features") or []
        ]
        if features:
            ok, added = provider.addFeatures(features)
            if not ok:
                raise RuntimeError("Could not add initial vector features")
            layer.updateExtents()

        cleanup_residuals = []
        cleanup_errors = []
        if target is not None:
            from qcopilots_common.processing_jobs import (
                _cleanup_processing_stages,
                _commit_processing_output_promotion,
                _processing_file_version,
                _processing_output_family_paths,
                _promote_processing_outputs,
                _rollback_processing_output_promotion,
                _validate_processing_stage_targets,
            )

            target.parent.mkdir(parents=True, exist_ok=True)
            staging_root = Path(
                tempfile.mkdtemp(
                    prefix=f".{target.stem}.qcopilots-stage-",
                    dir=str(target.parent),
                )
            )
            staged_target = staging_root / target.name
            _validate_interactive_versions(expected_versions)
            stage = {
                "parameter": "path",
                "target": target,
                "family_paths": _processing_output_family_paths(target),
                "staging_root": staging_root,
                "staged_target": staged_target,
                "authorized_versions": [
                    _processing_file_version(path)
                    for path in _processing_output_family_paths(target)
                ],
            }
            receipt = None
            promotion_validated = False
            try:
                driver_name, written_layer_name = _write_vector_layer_file(
                    layer,
                    str(staged_target),
                    arguments.get("driver_name"),
                    name,
                )
                _validate_interactive_versions(expected_versions)
                _validate_processing_stage_targets([stage])
                receipt = _promote_processing_outputs(
                    [stage],
                    overwrite=bool(arguments.get("overwrite", False)),
                )
                source = str(target)
                if driver_name == "GPKG" and written_layer_name:
                    source = f"{target}|layername={written_layer_name}"
                written_layer = QgsVectorLayer(source, layer.name(), "ogr")
                if not written_layer.isValid():
                    raise RuntimeError(
                        f"Could not reopen written vector layer: {source}"
                    )
                layer = written_layer
                promotion_validated = True
                try:
                    cleanup_residuals.extend(
                        _commit_processing_output_promotion(receipt)
                    )
                except Exception as err:
                    cleanup_errors.append(
                        f"Vector output backup cleanup failed: {err}"
                    )
                    cleanup_residuals.extend(
                        str(path)
                        for path in receipt.get("backup_roots", [])
                        if Path(path).exists()
                    )
                receipt = None
            except Exception:
                if receipt is not None and not promotion_validated:
                    rollback = _rollback_processing_output_promotion(receipt)
                    if not rollback["complete"]:
                        raise RuntimeError(
                            "Vector output rollback was incomplete: "
                            + ", ".join(rollback["residual_paths"])
                        )
                raise
            finally:
                cleanup_residuals.extend(_cleanup_processing_stages([stage]))

        added_to_project = bool(arguments.get("add_to_project", True))
        if added_to_project:
            QgsProject.instance().addMapLayer(layer)
        return {
            "added": added_to_project,
            "layer": _layer_summary(layer),
            "path": output_path,
            "feature_count": layer.featureCount(),
            "cleanup": {
                "complete": not cleanup_residuals and not cleanup_errors,
                "residual_paths": sorted(set(cleanup_residuals)),
                "errors": cleanup_errors,
                "retry_recommended": bool(cleanup_residuals or cleanup_errors),
            },
        }

    def add_vector_features(self, arguments: dict[str, Any]) -> dict[str, Any]:
        layer = _vector_layer_from_id(arguments["layer_id"])
        features = [
            _feature_from_payload(layer.fields(), feature)
            for feature in arguments.get("features") or []
        ]
        if not features:
            raise RuntimeError("features must contain at least one feature")
        returned_features: list[Any] = []
        buffer = getattr(layer, "editBuffer", lambda: None)()
        before_added_ids = set()
        if buffer is not None:
            before_added_ids = {
                int(feature_id) for feature_id in buffer.addedFeatures()
            }

        def add_features() -> None:
            result = layer.addFeatures(features)
            ok = result[0] if isinstance(result, tuple) else result
            if isinstance(result, tuple) and len(result) > 1:
                returned_features.extend(result[1])
            current_buffer = getattr(layer, "editBuffer", lambda: None)()
            added_by_command = []
            if current_buffer is not None:
                added_by_command = [
                    feature
                    for feature_id, feature in current_buffer.addedFeatures().items()
                    if int(feature_id) not in before_added_ids
                ]
            if not ok or len(added_by_command) != len(features) or (
                isinstance(result, tuple)
                and len(result) > 1
                and len(result[1]) != len(features)
            ):
                raise RuntimeError("Could not add vector features")
            if not returned_features:
                returned_features.extend(added_by_command)

        started_edit_session = _atomic_vector_layer_edit(
            layer,
            "QCopilots add vector features",
            add_features,
        )
        layer.updateExtents()
        assigned_features = returned_features or features
        added_feature_ids = [int(feature.id()) for feature in assigned_features]
        feature_ids_verified = (
            len(added_feature_ids) == len(features)
            and len(set(added_feature_ids)) == len(features)
            and all(layer.getFeature(feature_id).isValid() for feature_id in added_feature_ids)
        )
        if not feature_ids_verified:
            added_feature_ids = []
        if self.iface:
            self.iface.mapCanvas().refresh()
        return {
            "layer": _layer_summary(layer),
            "added_feature_ids": added_feature_ids,
            "added_count": len(features),
            "feature_ids_verified": feature_ids_verified,
            **_vector_edit_result_state(started_edit_session),
        }

    def update_vector_features(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsGeometry

        layer = _vector_layer_from_id(arguments["layer_id"])
        field_indexes = _field_index_by_name(layer.fields())
        attribute_updates = {}
        geometry_updates = {}
        updated_ids = []
        seen_feature_ids = set()
        updates = arguments.get("updates") or []
        if not updates:
            raise RuntimeError("updates must contain at least one feature update")
        for update in updates:
            feature_id = _feature_id(update.get("feature_id"))
            if feature_id in seen_feature_ids:
                raise RuntimeError(f"Duplicate feature_id in updates: {feature_id}")
            seen_feature_ids.add(feature_id)
            feature = layer.getFeature(feature_id)
            if not feature.isValid():
                raise RuntimeError(f"Vector feature not found: {feature_id}")
            has_attributes = bool(update.get("attributes"))
            has_geometry = bool(update.get("geometry_wkt"))
            if not has_attributes and not has_geometry:
                raise RuntimeError("Each update must include attributes or geometry_wkt")
            updated_ids.append(feature_id)
            if has_attributes:
                attribute_values = {}
                for name, value in (update.get("attributes") or {}).items():
                    if name not in field_indexes:
                        raise RuntimeError(f"Unknown vector field: {name}")
                    attribute_values[field_indexes[name]] = value
                attribute_updates[feature_id] = attribute_values
            if has_geometry:
                geometry = QgsGeometry.fromWkt(update["geometry_wkt"])
                if geometry.isNull():
                    raise RuntimeError(f"Invalid feature geometry WKT: {update['geometry_wkt']}")
                geometry_updates[feature_id] = geometry

        def update_features() -> None:
            for feature_id, values in attribute_updates.items():
                for field_index, value in values.items():
                    if not layer.changeAttributeValue(feature_id, field_index, value):
                        raise RuntimeError(
                            f"Could not update vector feature attribute: {feature_id}"
                        )
            for feature_id, geometry in geometry_updates.items():
                if not layer.changeGeometry(feature_id, geometry):
                    raise RuntimeError(
                        f"Could not update vector feature geometry: {feature_id}"
                    )

        started_edit_session = _atomic_vector_layer_edit(
            layer,
            "QCopilots update vector features",
            update_features,
        )
        layer.updateExtents()
        if self.iface:
            self.iface.mapCanvas().refresh()
        return {
            "layer": _layer_summary(layer),
            "updated_feature_ids": updated_ids,
            "updated_count": len(updated_ids),
            **_vector_edit_result_state(started_edit_session),
        }

    def processing_list_algorithms(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsApplication

        _ensure_processing_initialized()
        category = arguments.get("category")
        if category not in {"vector", "raster", "general"}:
            raise ValueError("category must be vector, raster or general")
        if "cursor" in arguments and "max_results" in arguments:
            raise ValueError("max_results and cursor cannot be used together")

        entries = []
        for provider in QgsApplication.processingRegistry().providers():
            for algorithm in provider.algorithms():
                entries.append((provider, algorithm))

        if "cursor" in arguments:
            cursor_state = _decode_processing_algorithm_cursor(
                arguments["cursor"],
                self._processing_algorithm_cursor_secret,
            )
            if cursor_state["category"] != category:
                raise _processing_algorithm_cursor_error(
                    "cursor belongs to a different category"
                )
            offset = cursor_state["next_offset"]
            max_results = cursor_state["max_results"]
            returned_before = cursor_state["returned_total"]
            _validate_processing_algorithm_cursor_prefix(entries, cursor_state)
        else:
            max_results = arguments.get(
                "max_results",
                DEFAULT_PROCESSING_ALGORITHM_RESULTS,
            )
            _validate_processing_algorithm_max_results(max_results)
            offset = 0
            returned_before = 0

        page_capacity = min(
            PROCESSING_ALGORITHM_PAGE_SIZE,
            max_results - returned_before,
        )
        selected = []
        next_offset = offset
        has_more = False
        for entry_index in range(offset, len(entries)):
            provider, algorithm = entries[entry_index]
            if not _processing_algorithm_is_listable(algorithm, category):
                if len(selected) < page_capacity:
                    next_offset = entry_index + 1
                continue
            if len(selected) >= page_capacity:
                has_more = True
                break
            selected.append((provider, algorithm))
            next_offset = entry_index + 1

        algorithms = []
        for provider, algorithm in selected:
            metadata = processing_algorithm_metadata(
                algorithm,
                provider,
                include_definitions=False,
            )
            metadata.pop("short_help", None)
            algorithms.append(metadata)

        returned_count = len(algorithms)
        returned_total = returned_before + returned_count
        truncated = bool(has_more and returned_total >= max_results)
        next_cursor = None
        if has_more and not truncated:
            next_cursor = _encode_processing_algorithm_cursor(
                {
                    "version": 1,
                    "category": category,
                    "next_offset": next_offset,
                    "max_results": max_results,
                    "returned_total": returned_total,
                    "previous_algorithm_id": str(selected[-1][1].id()),
                    "prefix_digest": _processing_algorithm_prefix_digest(
                        entries,
                        next_offset,
                    ),
                },
                self._processing_algorithm_cursor_secret,
            )
        return {
            "algorithms": algorithms,
            "returned_count": returned_count,
            "returned_total": returned_total,
            "max_results": max_results,
            "next_cursor": next_cursor,
            "truncated": truncated,
        }

    def processing_algorithm_details(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsApplication

        _ensure_processing_initialized()
        algorithm = QgsApplication.processingRegistry().createAlgorithmById(arguments["algorithm_id"])
        if not algorithm:
            raise RuntimeError("Processing algorithm not found")
        category = arguments.get("category")
        if category and not _algorithm_matches_category(algorithm, category):
            raise RuntimeError(f"Processing algorithm is not available for {category} data")
        return processing_algorithm_metadata(algorithm)

    def processing_start_algorithm(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self._processing_jobs().start(arguments)

    def processing_get_job(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self._processing_jobs().get(arguments)

    def processing_list_jobs(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self._processing_jobs().list(arguments)

    def processing_cancel_job(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self._processing_jobs().cancel(arguments)

    def _processing_jobs(self):
        if self._processing_job_manager_instance is None:
            if self._processing_job_manager_factory is not None:
                self._processing_job_manager_instance = (
                    self._processing_job_manager_factory()
                )
            else:
                from qcopilots_common.processing_jobs import ProcessingJobManager

                self._processing_job_manager_instance = ProcessingJobManager(
                    self.iface,
                    dependencies={
                        "parameter_sanitizer": _sanitize_processing_parameters,
                        "category_matcher": _algorithm_matches_category,
                        "filesystem_policy": self._filesystem_policy,
                    },
                )
        return self._processing_job_manager_instance

    def qgis_binary_list_binaries(
        self, arguments: dict[str, Any]
    ) -> dict[str, Any]:
        return self._qgis_binary_jobs().list_binaries(arguments)

    def qgis_binary_get_binary_details(
        self, arguments: dict[str, Any]
    ) -> dict[str, Any]:
        return self._qgis_binary_jobs().get_binary_details(arguments)

    def qgis_binary_start(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self._qgis_binary_jobs().start(arguments)

    def qgis_binary_get_job(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self._qgis_binary_jobs().get(arguments)

    def qgis_binary_list_jobs(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self._qgis_binary_jobs().list_jobs(arguments)

    def qgis_binary_cancel_job(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self._qgis_binary_jobs().cancel(arguments)

    def _qgis_binary_jobs(self):
        if self._qgis_binary_job_manager_instance is None:
            if self._qgis_binary_job_manager_factory is not None:
                self._qgis_binary_job_manager_instance = (
                    self._qgis_binary_job_manager_factory()
                )
            else:
                from qcopilots_common.qgis_binary_jobs import QGISBinaryJobManager

                self._qgis_binary_job_manager_instance = QGISBinaryJobManager(
                    self.iface,
                    dependencies={"filesystem_policy": self._filesystem_policy},
                )
        return self._qgis_binary_job_manager_instance

    def _safe_workspace_path(
        self,
        value: str | Path,
        *,
        access: str = "read",
    ) -> str:
        return str(
            self._filesystem_policy.resolve_path(value, access=access)
        )

    def _validate_layer_source_path(
        self,
        source: str,
        *,
        layer_type: str = "",
        provider: str = "",
    ) -> str:
        path_part, separator, options = source.partition("|")
        explicit_local_path = bool(
            _looks_like_path_value(path_part)
            and not _is_encoded_provider_uri(path_part)
            and "://" not in path_part
            and not path_part.casefold().startswith("file:")
            and not _is_local_provider_datasource(path_part)
        )
        if not self._filesystem_policy.restricted:
            if path_part.lower().startswith("file:"):
                resolved = self._filesystem_policy.resolve_file_uri(
                    path_part, access="read"
                )
                return str(resolved) + (separator + options if separator else "")
            if explicit_local_path:
                resolved = self._safe_workspace_path(path_part, access="read")
                return resolved + (separator + options if separator else "")
            if _is_remote_or_provider_uri(path_part):
                self._filesystem_policy.validate_provider_local_paths(
                    path_part
                    + (("&" + options.replace("|", "&")) if separator else ""),
                    access="read",
                    provider=provider,
                )
                return source
        elif path_part.lower().startswith(("http://", "https://")):
            if separator:
                raise PermissionError(
                    "Formal restricted network layer sources must use a single "
                    "approved URL without provider suffix fields"
                )
            self._filesystem_policy.validate_network_url(path_part)
            return source
        elif _is_remote_or_provider_uri(path_part) and not explicit_local_path:
            fields = _provider_uri_security_fields(
                path_part + (("&" + options.replace("|", "&")) if separator else "")
            )
            keys = set(fields)
            if "url" in fields:
                if keys != {"url"}:
                    raise PermissionError(
                        "Formal restricted network provider URIs may contain only "
                        "one approved url field"
                    )
                self._filesystem_policy.validate_network_url(fields["url"])
                return source
            local_path_keys = keys & _FORMAL_LOCAL_PROVIDER_PATH_FIELDS
            if len(local_path_keys) != 1:
                raise PermissionError(
                    "Formal restricted local provider URIs require exactly one "
                    "approved local path field"
                )
            if keys - (
                _FORMAL_LOCAL_PROVIDER_PATH_FIELDS
                | _FORMAL_LOCAL_PROVIDER_METADATA_FIELDS
            ):
                raise PermissionError(
                    "Formal restricted mode rejects provider connection or unknown fields"
                )
            normalized_layer_type = str(layer_type).strip().casefold()
            normalized_provider = _normalize_layer_provider(
                normalized_layer_type or "auto", provider
            ).casefold()
            if normalized_provider and normalized_provider not in {
                "ogr",
                "gdal",
                "sqlite",
                "spatialite",
                "mdal",
                "pdal",
                "vectortile",
                "cesiumtiles",
            }:
                raise PermissionError(
                    "Formal restricted local provider URI uses an unapproved provider"
                )
            if normalized_layer_type and normalized_layer_type not in {
                "auto",
                "vector",
                "raster",
            }:
                raise PermissionError(
                    "Formal restricted local provider URI uses an unapproved layer type"
                )
            local_key = next(iter(local_path_keys))
            resolved = self._filesystem_policy.resolve_path(
                fields[local_key], access="read"
            )
            if not resolved.exists():
                raise FileNotFoundError(
                    f"Formal restricted layer source does not exist: {resolved}"
                )
            return source
        if _is_remote_or_provider_uri(path_part) and not explicit_local_path:
            return source
        if not _looks_like_path_value(path_part):
            if self._filesystem_policy.restricted and not path_part.lower().startswith(
                "memory:"
            ):
                raise PermissionError(
                    "Formal restricted layer source must be an approved local path, "
                    "an approved loopback URL, or an in-memory source"
                )
            return source
        if self._filesystem_policy.restricted and separator:
            option_fields = _provider_uri_security_fields(
                options.replace("|", "&")
            )
            if set(option_fields) - _FORMAL_LOCAL_PROVIDER_METADATA_FIELDS:
                raise PermissionError(
                    "Formal restricted local layer options contain connection or "
                    "unknown fields"
                )
        resolved = self._safe_workspace_path(path_part, access="read")
        if self._filesystem_policy.restricted and not Path(resolved).exists():
            raise FileNotFoundError(
                f"Formal restricted layer source does not exist: {resolved}"
            )
        return resolved + (separator + options if separator else "")

    def _validate_layer_uri_options(
        self,
        uri_options: dict[str, Any],
        *,
        layer_type: str,
        provider: str,
    ) -> dict[str, Any]:
        normalized = dict(uri_options)
        if not self._filesystem_policy.restricted:
            self._filesystem_policy.validate_provider_options_local_paths(
                normalized,
                access="read",
                provider=provider,
            )
        for key in ("path", "file", "filename"):
            if key in normalized and isinstance(normalized[key], str):
                normalized[key] = self._validate_layer_source_path(
                    normalized[key],
                    layer_type=layer_type,
                    provider=provider,
                )
        url = normalized.get("url")
        if not isinstance(url, str):
            return normalized
        if url.casefold().startswith("file:"):
            normalized["url"] = str(
                self._filesystem_policy.resolve_file_uri(url, access="read")
            )
        elif _looks_like_policy_local_path_value(url):
            normalized["url"] = self._validate_layer_source_path(
                url,
                layer_type=layer_type,
                provider=provider,
            )
        elif self._filesystem_policy.restricted:
            self._filesystem_policy.validate_network_url(url)
        return normalized


def _ensure_processing_initialized() -> None:
    try:
        from processing.core.Processing import Processing
    except Exception as err:
        raise RuntimeError(f"QGIS Processing is not available: {err}") from err
    try:
        Processing.initialize()
    except Exception as err:
        raise RuntimeError(f"QGIS Processing could not be initialized: {err}") from err


def _supported_layer_type_descriptions() -> list[dict[str, Any]]:
    return [
        {
            "layer_type": "auto",
            "providers": ["ogr", "gdal"],
            "description": "Try common local vector and raster providers in order.",
        },
        {
            "layer_type": "vector",
            "providers": [
                "ogr",
                "memory",
                "delimitedtext",
                "WFS",
                "arcgisfeatureserver",
            ],
        },
        {
            "layer_type": "raster",
            "providers": [
                "gdal",
                "wms",
                "wcs",
                "arcgismapserver",
                "arcgisimageserver",
            ],
        },
        {"layer_type": "mesh", "providers": ["mdal", "mesh_memory"]},
        {"layer_type": "vector_tile", "providers": ["vectortile"]},
        {"layer_type": "point_cloud", "providers": ["pdal", "ept"]},
        {
            "layer_type": "tiled_scene",
            "providers": ["cesiumtiles", "esrii3s", "tiled_mesh"],
        },
        {"layer_type": "xyz", "providers": ["wms"]},
        {"layer_type": "wms", "providers": ["wms"]},
        {"layer_type": "wcs", "providers": ["wcs"]},
        {"layer_type": "arcgis_feature_server", "providers": ["arcgisfeatureserver"]},
        {"layer_type": "arcgis_map_server", "providers": ["arcgismapserver"]},
        {"layer_type": "arcgis_image_server", "providers": ["arcgisimageserver"]},
    ]


def _native_resolution_zoom_factor(layer: Any, canvas: Any) -> float:
    map_units_per_pixel = float(canvas.mapUnitsPerPixel())
    if map_units_per_pixel <= 0:
        raise RuntimeError("Map canvas resolution is not available")
    provider = layer.dataProvider()
    native_resolutions = provider.nativeResolutions() if provider else []
    if native_resolutions:
        factor = float(native_resolutions[0]) / map_units_per_pixel
    else:
        factor = math.sqrt(
            layer.rasterUnitsPerPixelX() * layer.rasterUnitsPerPixelX()
            + layer.rasterUnitsPerPixelY() * layer.rasterUnitsPerPixelY()
        ) / map_units_per_pixel
    if not math.isfinite(factor) or factor <= 0:
        raise RuntimeError("Raster native resolution is not available")
    return factor


def _normalize_layer_type(layer_type: str) -> str:
    normalized = str(layer_type).strip().lower().replace("-", "_")
    aliases = {
        "vectortile": "vector_tile",
        "vector_tiles": "vector_tile",
        "pointcloud": "point_cloud",
        "tiledscene": "tiled_scene",
        "arcgisfeatureserver": "arcgis_feature_server",
        "arcgismapserver": "arcgis_map_server",
        "arcgisimageserver": "arcgis_image_server",
    }
    normalized = aliases.get(normalized, normalized)
    valid_types = {
        item["layer_type"]
        for item in _supported_layer_type_descriptions()
    }
    if normalized not in valid_types:
        raise RuntimeError(f"Unsupported layer_type: {layer_type}")
    return normalized


def _default_layer_provider(layer_type: str) -> str:
    return {
        "auto": "",
        "vector": "ogr",
        "raster": "gdal",
        "mesh": "mdal",
        "vector_tile": "vectortile",
        "point_cloud": "pdal",
        "tiled_scene": "cesiumtiles",
        "xyz": "wms",
        "wms": "wms",
        "wcs": "wcs",
        "arcgis_feature_server": "arcgisfeatureserver",
        "arcgis_map_server": "arcgismapserver",
        "arcgis_image_server": "arcgisimageserver",
    }[layer_type]


def _resolve_layer_type_and_provider(
    layer_type: str,
    provider: Any,
    source: str,
    uri_options: dict[str, Any],
) -> tuple[str, str]:
    provider_token = _provider_token(provider)
    resolved_layer_type = _specialized_layer_type(
        layer_type,
        provider_token,
        source,
        uri_options,
    )
    return resolved_layer_type, _normalize_layer_provider(resolved_layer_type, provider)


def _specialized_layer_type(
    layer_type: str,
    provider_token: str,
    source: str,
    uri_options: dict[str, Any],
) -> str:
    if layer_type == "raster":
        if _looks_like_xyz_tile_template(source) or _uri_options_type(uri_options) == "xyz":
            return "xyz"
        if "layers" in uri_options:
            return "wms"
        if "identifier" in uri_options:
            return "wcs"
        encoded_layer_type = _layer_type_from_encoded_provider_uri(source)
        if encoded_layer_type in {
            "xyz",
            "wms",
            "wcs",
            "arcgis_map_server",
            "arcgis_image_server",
        }:
            return encoded_layer_type
    hinted_layer_type = _layer_type_from_provider_token(provider_token)
    if hinted_layer_type and _provider_hint_matches_layer_type(layer_type, hinted_layer_type):
        return hinted_layer_type
    if layer_type == "vector":
        encoded_layer_type = _layer_type_from_encoded_provider_uri(source)
        if encoded_layer_type == "arcgis_feature_server":
            return encoded_layer_type
    return layer_type


def _normalize_layer_provider(layer_type: str, provider: Any) -> str:
    provider_text = str(provider or "").strip()
    token = _provider_token(provider_text)
    if not token:
        return _default_layer_provider(layer_type)

    aliases = {
        "xyz": {"xyz": "wms", "wms": "wms"},
        "wms": {"wms": "wms"},
        "wcs": {"wcs": "wcs"},
        "vector_tile": {
            "vectortile": "vectortile",
            "vector_tile": "vectortile",
            "vector_tiles": "vectortile",
            "xyz": "vectortile",
        },
        "point_cloud": {
            "pointcloud": "pdal",
            "point_cloud": "pdal",
            "pdal": "pdal",
            "ept": "ept",
        },
        "tiled_scene": {
            "tiledscene": "cesiumtiles",
            "tiled_scene": "cesiumtiles",
            "cesiumtiles": "cesiumtiles",
            "cesium_tiles": "cesiumtiles",
            "esrii3s": "esrii3s",
            "tiled_mesh": "tiled_mesh",
        },
        "arcgis_feature_server": {
            "arcgisfeatureserver": "arcgisfeatureserver",
            "arcgis_feature_server": "arcgisfeatureserver",
            "arcgis_feature": "arcgisfeatureserver",
        },
        "arcgis_map_server": {
            "arcgismapserver": "arcgismapserver",
            "arcgis_map_server": "arcgismapserver",
            "arcgis_map": "arcgismapserver",
        },
        "arcgis_image_server": {
            "arcgisimageserver": "arcgisimageserver",
            "arcgis_image_server": "arcgisimageserver",
            "arcgis_image": "arcgisimageserver",
        },
        "mesh": {"mesh": "mdal", "mdal": "mdal", "mesh_memory": "mesh_memory"},
        "vector": {"wfs": "WFS"},
    }
    return aliases.get(layer_type, {}).get(token, provider_text)


def _provider_token(provider: Any) -> str:
    return str(provider or "").strip().lower().replace("-", "_").replace(" ", "_")


def _layer_type_from_provider_token(provider_token: str) -> str:
    return {
        "xyz": "xyz",
        "wms": "wms",
        "wcs": "wcs",
        "vectortile": "vector_tile",
        "vector_tile": "vector_tile",
        "vector_tiles": "vector_tile",
        "pointcloud": "point_cloud",
        "point_cloud": "point_cloud",
        "pdal": "point_cloud",
        "ept": "point_cloud",
        "tiledscene": "tiled_scene",
        "tiled_scene": "tiled_scene",
        "cesiumtiles": "tiled_scene",
        "cesium_tiles": "tiled_scene",
        "esrii3s": "tiled_scene",
        "arcgisfeatureserver": "arcgis_feature_server",
        "arcgis_feature_server": "arcgis_feature_server",
        "arcgismapserver": "arcgis_map_server",
        "arcgis_map_server": "arcgis_map_server",
        "arcgisimageserver": "arcgis_image_server",
        "arcgis_image_server": "arcgis_image_server",
        "mesh": "mesh",
        "mdal": "mesh",
    }.get(provider_token, "")


def _provider_hint_matches_layer_type(layer_type: str, hinted_layer_type: str) -> bool:
    if layer_type == "auto":
        return True
    if layer_type == hinted_layer_type:
        return True
    if layer_type == "raster":
        return hinted_layer_type in {
            "xyz",
            "wms",
            "wcs",
            "arcgis_map_server",
            "arcgis_image_server",
        }
    if layer_type == "vector":
        return hinted_layer_type in {"arcgis_feature_server", "vector_tile"}
    return False


def _uri_options_type(uri_options: dict[str, Any]) -> str:
    return str(uri_options.get("type") or "").strip().lower()


def _looks_like_xyz_tile_template(source: str) -> bool:
    decoded = unquote(str(source or "")).lower()
    return (
        decoded.startswith(("http://", "https://"))
        and "{x}" in decoded
        and "{y}" in decoded
        and ("{z}" in decoded or "{zoom}" in decoded)
    )


def _layer_type_from_encoded_provider_uri(source: str) -> str:
    if not _is_encoded_provider_uri(source):
        return ""
    source_type = _provider_uri_parts(source).get("type", [""])[0].strip().lower()
    if source_type == "xyz":
        return "xyz"
    return ""


def _create_qgis_layer(
    layer_type: str,
    source: str,
    name: str,
    provider: str,
    uri_options: dict[str, Any],
) -> tuple[Any, str, str, str]:
    layer_type = _normalize_layer_type(layer_type)
    layer_type, provider = _resolve_layer_type_and_provider(
        layer_type,
        provider,
        source,
        uri_options,
    )
    if layer_type == "auto":
        if _is_remote_or_provider_uri(source):
            raise RuntimeError(
                "auto layer_type is only supported for local files. "
                "Use an explicit layer_type for remote URLs or provider URIs."
            )
        attempts = [
            ("vector", "ogr"),
            ("raster", "gdal"),
            ("mesh", "mdal"),
            ("point_cloud", "pdal"),
            ("vector_tile", "vectortile"),
            ("tiled_scene", "cesiumtiles"),
        ]
        errors = []
        for candidate_type, candidate_provider in attempts:
            candidate_source = _layer_source_uri(
                candidate_type,
                source,
                candidate_provider,
                uri_options,
            )
            layer = _instantiate_qgis_layer(
                candidate_type,
                candidate_source,
                name,
                candidate_provider,
            )
            if layer.isValid():
                return layer, candidate_type, candidate_provider, candidate_source
            errors.append(f"{candidate_type}:{candidate_provider}")
        raise RuntimeError(
            f"Could not load source with common layer providers: {', '.join(errors)}"
        )

    resolved_provider = provider or _default_layer_provider(layer_type)
    resolved_source = _layer_source_uri(
        layer_type,
        source,
        resolved_provider,
        uri_options,
    )
    layer = _instantiate_qgis_layer(
        layer_type,
        resolved_source,
        name,
        resolved_provider,
    )
    return layer, layer_type, resolved_provider, resolved_source


def _instantiate_qgis_layer(
    layer_type: str,
    source: str,
    name: str,
    provider: str,
) -> Any:
    if layer_type in ("vector", "arcgis_feature_server"):
        from qgis.core import QgsVectorLayer

        return QgsVectorLayer(source, name, provider)
    if layer_type in (
        "raster",
        "xyz",
        "wms",
        "wcs",
        "arcgis_map_server",
        "arcgis_image_server",
    ):
        from qgis.core import QgsRasterLayer

        return QgsRasterLayer(source, name, provider)
    if layer_type == "mesh":
        from qgis.core import QgsMeshLayer

        return QgsMeshLayer(source, name, provider)
    if layer_type == "vector_tile":
        from qgis.core import QgsVectorTileLayer

        return QgsVectorTileLayer(source, name)
    if layer_type == "point_cloud":
        from qgis.core import QgsPointCloudLayer

        return QgsPointCloudLayer(source, name, provider)
    if layer_type == "tiled_scene":
        from qgis.core import QgsTiledSceneLayer

        return QgsTiledSceneLayer(source, name, provider)
    raise RuntimeError(f"Unsupported layer_type: {layer_type}")


def _vector_geometry_type(value: Any) -> str:
    geometry_type = str(value or "Point").strip()
    valid = {
        "Point",
        "LineString",
        "Polygon",
        "MultiPoint",
        "MultiLineString",
        "MultiPolygon",
        "NoGeometry",
    }
    if geometry_type not in valid:
        raise RuntimeError(f"Unsupported vector geometry_type: {geometry_type}")
    return geometry_type


def _qgs_field(field: dict[str, Any]) -> Any:
    from qgis.PyQt.QtCore import QVariant
    from qgis.core import QgsField

    field_type = str(field.get("type") or "string").strip().lower()
    qvariant_type = {
        "string": QVariant.String,
        "int": QVariant.Int,
        "integer": QVariant.Int,
        "long": QVariant.LongLong,
        "double": QVariant.Double,
        "float": QVariant.Double,
        "bool": QVariant.Bool,
        "boolean": QVariant.Bool,
        "date": QVariant.Date,
        "datetime": QVariant.DateTime,
    }.get(field_type)
    if qvariant_type is None:
        raise RuntimeError(f"Unsupported vector field type: {field_type}")
    return QgsField(str(field["name"]), qvariant_type)


def _feature_from_payload(fields: Any, payload: dict[str, Any]) -> Any:
    from qgis.core import QgsFeature, QgsGeometry

    feature = QgsFeature(fields)
    if payload.get("geometry_wkt"):
        geometry = QgsGeometry.fromWkt(payload["geometry_wkt"])
        if geometry.isNull():
            raise RuntimeError(f"Invalid feature geometry WKT: {payload['geometry_wkt']}")
        feature.setGeometry(geometry)
    attributes = payload.get("attributes") or {}
    field_indexes = _field_index_by_name(fields)
    for name, value in attributes.items():
        if name not in field_indexes:
            raise RuntimeError(f"Unknown vector field: {name}")
        feature.setAttribute(field_indexes[name], value)
    return feature


def _field_index_by_name(fields: Any) -> dict[str, int]:
    return {fields.at(index).name(): index for index in range(fields.count())}


def _map_layer_from_id(layer_id: str) -> Any:
    from qgis.core import QgsProject

    layer = QgsProject.instance().mapLayer(layer_id)
    if layer is None:
        raise RuntimeError("Map layer not found")
    return layer


def _vector_layer_from_id(layer_id: str) -> Any:
    from qgis.core import QgsProject, QgsVectorLayer

    layer = QgsProject.instance().mapLayer(layer_id)
    if not isinstance(layer, QgsVectorLayer):
        raise RuntimeError("Vector layer not found")
    return layer


def _raster_layer_from_id(layer_id: str) -> Any:
    from qgis.core import QgsProject, QgsRasterLayer

    layer = QgsProject.instance().mapLayer(layer_id)
    if not isinstance(layer, QgsRasterLayer):
        raise RuntimeError("Raster layer not found")
    return layer


def _feature_id(value: Any) -> int:
    try:
        return int(value)
    except (TypeError, ValueError) as err:
        raise RuntimeError(f"Invalid feature_id: {value}") from err


def _atomic_vector_layer_edit(layer: Any, label: str, operation: Any) -> bool:
    """Apply one reversible command and leave all successful edits staged."""

    started_here = not layer.isEditable()
    if started_here and not layer.startEditing():
        raise RuntimeError("Could not start an atomic vector layer edit")
    command_open = False
    try:
        layer.beginEditCommand(label)
        command_open = True
        operation()
        layer.endEditCommand()
        command_open = False
        return started_here
    except Exception:
        if command_open:
            try:
                layer.destroyEditCommand()
            except Exception:
                pass
        if started_here and layer.isEditable():
            try:
                layer.rollBack()
            except Exception:
                pass
        raise


def _vector_edit_result_state(started_edit_session: bool) -> dict[str, Any]:
    return {
        "applied": True,
        "staged": True,
        "committed": False,
        "edit_state": (
            "staged_in_new_edit_session"
            if started_edit_session
            else "staged_in_existing_edit_session"
        ),
        "edit_buffer_atomic": True,
        "provider_commit_attempted": False,
        "requires_user_commit": True,
    }


def _matching_vector_feature_ids(
    layer: Any,
    arguments: dict[str, Any],
    *,
    maximum: int,
) -> tuple[list[int], bool]:
    from qgis.core import QgsExpression, QgsFeatureRequest

    requested_ids = {
        _feature_id(value) for value in (arguments.get("feature_ids") or [])
    }
    expression_text = str(arguments.get("filter_expression") or "").strip()
    request = QgsFeatureRequest()
    if expression_text:
        expression = QgsExpression(expression_text)
        if expression.hasParserError():
            raise RuntimeError(
                f"Invalid filter_expression: {expression.parserErrorString()}"
            )
        _validate_expression_fields(expression, layer.fields(), "filter_expression")
        request.setFilterExpression(expression_text)
    elif requested_ids:
        request.setFilterFids(sorted(requested_ids))
    request.setOrderBy(
        QgsFeatureRequest.OrderBy(
            [QgsFeatureRequest.OrderByClause("$id", True)]
        )
    )
    request.setLimit(maximum + 1)

    feature_ids = []
    for feature in layer.getFeatures(request):
        feature_id = int(feature.id())
        if requested_ids and feature_id not in requested_ids:
            continue
        feature_ids.append(feature_id)
        if len(feature_ids) > maximum:
            return sorted(feature_ids[:maximum]), True
    if requested_ids and not expression_text:
        missing = requested_ids - set(feature_ids)
        if missing:
            raise RuntimeError(
                "Vector feature ids were not found: "
                + ", ".join(str(value) for value in sorted(missing))
            )
    return sorted(feature_ids), False


def _query_vector_features(layer: Any, arguments: dict[str, Any]) -> dict[str, Any]:
    from qgis.core import QgsExpression, QgsFeatureRequest
    from qcopilots_common.processing_jobs import json_safe_value

    limit = arguments.get("limit", 100)
    offset = arguments.get("offset", 0)
    if isinstance(limit, bool) or not isinstance(limit, int) or not 1 <= limit <= 500:
        raise RuntimeError("limit must be an integer from 1 through 500")
    if (
        isinstance(offset, bool)
        or not isinstance(offset, int)
        or not 0 <= offset <= MAX_VECTOR_QUERY_OFFSET
    ):
        raise RuntimeError(
            f"offset must be an integer from 0 through {MAX_VECTOR_QUERY_OFFSET}"
        )
    requested_ids = {
        _feature_id(value) for value in (arguments.get("feature_ids") or [])
    }
    if bool(arguments.get("selected_only", False)):
        selected_ids = set(int(value) for value in layer.selectedFeatureIds())
        requested_ids = selected_ids & requested_ids if requested_ids else selected_ids
        if not requested_ids:
            return {
                "layer": _layer_summary(layer),
                "features": [],
                "returned_count": 0,
                "offset": offset,
                "limit": limit,
                "truncated": False,
                "geometry_bytes": 0,
                "geometry_byte_limit": MAX_VECTOR_QUERY_GEOMETRY_BYTES,
                "response_bytes": 2,
                "response_byte_limit": MAX_VECTOR_QUERY_RESPONSE_BYTES,
            }
    fields_by_name = _field_index_by_name(layer.fields())
    requested_fields = arguments.get("fields") or list(fields_by_name)
    unknown_fields = [name for name in requested_fields if name not in fields_by_name]
    if unknown_fields:
        raise RuntimeError("Unknown vector fields: " + ", ".join(unknown_fields))
    expression_text = str(arguments.get("filter_expression") or "").strip()
    request = QgsFeatureRequest()
    if expression_text:
        expression = QgsExpression(expression_text)
        if expression.hasParserError():
            raise RuntimeError(
                f"Invalid filter_expression: {expression.parserErrorString()}"
            )
        _validate_expression_fields(expression, layer.fields(), "filter_expression")
        request.setFilterExpression(expression_text)
    elif requested_ids:
        request.setFilterFids(sorted(requested_ids))
    request.setOrderBy(
        QgsFeatureRequest.OrderBy(
            [QgsFeatureRequest.OrderByClause("$id", True)]
        )
    )
    request.setLimit(offset + limit + 1)
    include_geometry = bool(arguments.get("include_geometry", False))
    features = []
    geometry_bytes = 0
    response_bytes = 2
    matched_index = 0
    truncated = False
    for feature in layer.getFeatures(request):
        feature_id = int(feature.id())
        if requested_ids and feature_id not in requested_ids:
            continue
        if matched_index < offset:
            matched_index += 1
            continue
        if len(features) >= limit:
            truncated = True
            break
        item = {
            "feature_id": feature_id,
            "attributes": {
                name: json_safe_value(feature.attribute(name))
                for name in requested_fields
            },
        }
        if include_geometry:
            geometry = feature.geometry()
            geometry_wkt = "" if geometry.isNull() else geometry.asWkt()
            geometry_size = len(geometry_wkt.encode("utf-8"))
            if geometry_bytes + geometry_size > MAX_VECTOR_QUERY_GEOMETRY_BYTES:
                raise RuntimeError(
                    "Vector query geometry response exceeds the 1 MiB limit. "
                    "Use a smaller limit or query without geometry."
                )
            item["geometry_wkt"] = geometry_wkt
            geometry_bytes += geometry_size
        encoded_size = len(
            json.dumps(
                item,
                ensure_ascii=False,
                sort_keys=True,
                allow_nan=False,
                separators=(",", ":"),
            ).encode("utf-8")
        )
        candidate_response_bytes = (
            response_bytes + encoded_size + (1 if features else 0)
        )
        if candidate_response_bytes > MAX_VECTOR_QUERY_RESPONSE_BYTES:
            raise RuntimeError(
                "code=query_response_too_large, "
                f"response_byte_limit={MAX_VECTOR_QUERY_RESPONSE_BYTES}, Vector "
                "query response exceeds the total feature payload limit. Use fewer "
                "fields, a smaller limit or query without geometry."
            )
        features.append(item)
        response_bytes = candidate_response_bytes
        matched_index += 1
    return {
        "layer": _layer_summary(layer),
        "features": features,
        "returned_count": len(features),
        "offset": offset,
        "limit": limit,
        "truncated": truncated,
        "geometry_bytes": geometry_bytes,
        "geometry_byte_limit": MAX_VECTOR_QUERY_GEOMETRY_BYTES,
        "response_bytes": response_bytes,
        "response_byte_limit": MAX_VECTOR_QUERY_RESPONSE_BYTES,
    }


def _validate_expression_fields(expression: Any, fields: Any, label: str) -> None:
    available = set(_field_index_by_name(fields))
    unknown = sorted(str(name) for name in expression.referencedColumns() - available)
    if unknown:
        raise RuntimeError(f"Unknown fields in {label}: " + ", ".join(unknown))


def _vector_feature_fingerprint(layer: Any, feature_ids: list[int]) -> str:
    from qgis.core import QgsFeatureRequest
    from qcopilots_common.processing_jobs import json_safe_value

    request = QgsFeatureRequest().setFilterFids(sorted(set(feature_ids)))
    digest = hashlib.sha256()
    found_ids = []
    for feature in sorted(layer.getFeatures(request), key=lambda value: int(value.id())):
        feature_id = int(feature.id())
        found_ids.append(feature_id)
        digest.update(f"{feature_id}:".encode("ascii"))
        digest.update(
            json.dumps(
                json_safe_value(feature.attributes()),
                sort_keys=True,
                ensure_ascii=False,
                allow_nan=False,
            ).encode("utf-8")
        )
        geometry = feature.geometry()
        if not geometry.isNull():
            digest.update(bytes(geometry.asWkb()))
    if sorted(found_ids) != sorted(feature_ids):
        digest.update(b":missing")
    return digest.hexdigest()


def _write_vector_layer(
    layer: Any,
    path: str,
    driver_name: str | None,
    layer_name: str,
) -> Any:
    from qgis.core import QgsVectorLayer

    driver, new_layer_name = _write_vector_layer_file(
        layer,
        path,
        driver_name,
        layer_name,
    )
    source = path
    if driver == "GPKG" and new_layer_name:
        source = f"{path}|layername={new_layer_name}"
    written_layer = QgsVectorLayer(source, layer.name(), "ogr")
    if not written_layer.isValid():
        raise RuntimeError(f"Could not load written vector layer: {source}")
    return written_layer


def _write_vector_layer_file(
    layer: Any,
    path: str,
    driver_name: str | None,
    layer_name: str,
) -> tuple[str, str]:
    from qgis.core import QgsProject, QgsVectorFileWriter

    options = QgsVectorFileWriter.SaveVectorOptions()
    options.driverName = driver_name or _driver_name_for_path(path)
    options.layerName = layer_name
    result, message, new_file_name, new_layer_name = QgsVectorFileWriter.writeAsVectorFormatV3(
        layer,
        path,
        QgsProject.instance().transformContext(),
        options,
    )
    if result != QgsVectorFileWriter.WriterError.NoError:
        raise RuntimeError(f"Could not write vector layer: {message}")
    del new_file_name
    return options.driverName, str(new_layer_name or "")


def _driver_name_for_path(path: str) -> str:
    suffix = Path(path).suffix.lower()
    if suffix == ".gpkg":
        return "GPKG"
    if suffix == ".shp":
        return "ESRI Shapefile"
    if suffix in (".geojson", ".json"):
        return "GeoJSON"
    return "GPKG"


def _layer_source_uri(
    layer_type: str,
    source: str,
    provider: str,
    uri_options: dict[str, Any],
) -> str:
    if _is_encoded_provider_uri(source):
        return source

    if uri_options or layer_type in _provider_uri_layer_types():
        parts = _normalized_uri_options(layer_type, source, dict(uri_options))
        _validate_uri_options(layer_type, parts, source)
        encoded = _encode_provider_uri(provider, parts)
        if encoded:
            return encoded
        return urlencode(parts, doseq=True)

    return _normalize_layer_source_path(source)


def _normalized_uri_options(
    layer_type: str,
    source: str,
    uri_options: dict[str, Any],
) -> dict[str, Any]:
    if layer_type == "xyz":
        uri_options.setdefault("type", "xyz")
        uri_options.setdefault("zmin", 0)
        uri_options.setdefault("zmax", 19)
        uri_options.setdefault("tilePixelRatio", 1)
    if layer_type == "vector_tile":
        uri_options.setdefault("type", _vector_tile_source_type(source))
        if uri_options["type"] == "xyz":
            uri_options.setdefault("zmin", 0)
            uri_options.setdefault("zmax", 19)
    source_key = _uri_source_key(layer_type, source)
    if source and source_key not in uri_options:
        uri_options[source_key] = source
    for key in ("path", "file", "filename", "url"):
        if key in uri_options and isinstance(uri_options[key], str):
            uri_options[key] = _normalize_layer_source_path(uri_options[key])
    if layer_type == "wms" and "layers" in uri_options and "styles" not in uri_options:
        layers = uri_options["layers"]
        if isinstance(layers, list):
            uri_options["styles"] = [""] * len(layers)
        else:
            uri_options["styles"] = ""
    return uri_options


def _uri_source_key(layer_type: str, source: str) -> str:
    if layer_type in (
        "xyz",
        "vector_tile",
        "wms",
        "wcs",
        "arcgis_feature_server",
        "arcgis_map_server",
        "arcgis_image_server",
    ):
        return "url"
    if _is_remote_or_provider_uri(source):
        return "url"
    return "path"


def _provider_uri_layer_types() -> set[str]:
    return {
        "xyz",
        "vector_tile",
        "wms",
        "wcs",
        "arcgis_feature_server",
        "arcgis_map_server",
        "arcgis_image_server",
    }


def _vector_tile_source_type(source: str) -> str:
    suffix = Path(source.split("|", 1)[0]).suffix.lower()
    if suffix == ".mbtiles":
        return "mbtiles"
    if suffix == ".vtpk":
        return "vtpk"
    return "xyz"


def _validate_uri_options(layer_type: str, parts: dict[str, Any], source: str) -> None:
    if _is_encoded_provider_uri(source):
        return
    if layer_type == "wms" and not parts.get("layers"):
        raise RuntimeError("WMS loading requires uri_options.layers.")
    if layer_type == "wcs" and not parts.get("identifier"):
        raise RuntimeError("WCS loading requires uri_options.identifier.")


def _encode_provider_uri(provider: str, parts: dict[str, Any]) -> str:
    if not provider:
        return ""
    try:
        from qgis.core import QgsProviderRegistry

        metadata = QgsProviderRegistry.instance().providerMetadata(provider)
        if not metadata:
            return ""
        encoded = metadata.encodeUri(parts)
        return _qgis_string(encoded)
    except Exception:
        return ""


def _qgis_string(value: Any) -> str:
    if hasattr(value, "data"):
        try:
            data = value.data()
            if isinstance(data, bytes):
                return data.decode("utf-8")
        except Exception:
            pass
    return str(value)


def _provider_uri_parts(source: str) -> dict[str, list[str]]:
    return parse_qs(str(source), keep_blank_values=True)


_FORMAL_PROVIDER_URI_MAX_CHARS = 16384
_FORMAL_PROVIDER_URI_MAX_FIELDS = 32
_FORMAL_PROVIDER_VALUE_MAX_CHARS = 8192
_FORMAL_LOCAL_PROVIDER_PATH_FIELDS = frozenset(
    {"dbname", "path", "file", "filename"}
)
_FORMAL_LOCAL_PROVIDER_METADATA_FIELDS = frozenset(
    {"layer", "layername", "table", "geometrycolumn", "subset", "encoding"}
)
_FORMAL_NETWORK_URI_OPTION_FIELDS = frozenset(
    {
        "type",
        "layers",
        "styles",
        "format",
        "crs",
        "identifier",
        "zmin",
        "zmax",
        "tilepixelratio",
    }
)
_FORMAL_LOCAL_PROVIDERS = frozenset(
    {
        "ogr",
        "gdal",
        "sqlite",
        "spatialite",
        "mdal",
        "pdal",
        "vectortile",
        "cesiumtiles",
    }
)
_FORMAL_NETWORK_PROVIDERS = frozenset(
    {
        "wfs",
        "wms",
        "wcs",
        "vectortile",
        "arcgisfeatureserver",
        "arcgismapserver",
        "arcgisimageserver",
        "ept",
        "cesiumtiles",
        "esrii3s",
    }
)
_PROVIDER_SECURITY_FIELD_RE = re.compile(
    r"(?i)(?:^|[&\s]+)([a-z][a-z0-9_.-]*)\s*=\s*"
    r"(?:'([^']*)'|\"([^\"]*)\"|([^&\s]*))"
)


def _provider_uri_security_fields(source: str) -> dict[str, str]:
    """Parse every provider URI field or reject the URI as ambiguous."""

    decoded = str(source)
    if len(decoded) > _FORMAL_PROVIDER_URI_MAX_CHARS:
        raise PermissionError("Formal provider URI exceeds the bounded size limit")
    for _index in range(4):
        next_value = unquote(decoded)
        if next_value == decoded:
            break
        decoded = next_value
    if re.search(r"%[0-9A-Fa-f]{2}", decoded):
        raise PermissionError("Formal provider URI contains excessive encoding")
    if len(decoded) > _FORMAL_PROVIDER_URI_MAX_CHARS:
        raise PermissionError("Decoded formal provider URI exceeds the size limit")

    fields = {}
    cursor = 0
    matches = list(_PROVIDER_SECURITY_FIELD_RE.finditer(decoded))
    if not matches or len(matches) > _FORMAL_PROVIDER_URI_MAX_FIELDS:
        raise PermissionError(
            "Formal restricted mode rejects unrecognized or oversized provider URIs"
        )
    for match in matches:
        gap = decoded[cursor : match.start()]
        if gap.strip("& \t\r\n"):
            raise PermissionError(
                "Formal provider URI contains unparsed connection content"
            )
        key = unquote(match.group(1)).strip().casefold()
        value = next(
            (item for item in match.groups()[1:] if item is not None), ""
        ).strip()
        if not value:
            raise PermissionError("Formal provider URI fields must not be empty")
        if len(value) > _FORMAL_PROVIDER_VALUE_MAX_CHARS:
            raise PermissionError("Formal provider URI field exceeds the size limit")
        if key in fields:
            raise PermissionError(
                f"Formal provider URI contains a repeated field: {key}"
            )
        fields[key] = value
        cursor = match.end()
    if decoded[cursor:].strip("& \t\r\n"):
        raise PermissionError(
            "Formal provider URI contains trailing unparsed connection content"
        )
    return fields


def _validate_formal_layer_uri_options(
    source: str,
    uri_options: dict[str, Any],
    *,
    layer_type: str,
    provider: str,
    policy: FilesystemPolicy,
) -> None:
    if len(uri_options) > _FORMAL_PROVIDER_URI_MAX_FIELDS:
        raise PermissionError("Formal layer URI options exceed the field limit")
    normalized_options = {}
    for raw_key, value in uri_options.items():
        key = unquote(str(raw_key)).strip().casefold()
        if not key or key in normalized_options:
            raise PermissionError(
                "Formal layer URI options contain an empty or repeated field"
            )
        encoded_value = json.dumps(
            value,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        )
        if len(encoded_value) > _FORMAL_PROVIDER_VALUE_MAX_CHARS:
            raise PermissionError("Formal layer URI option exceeds the size limit")
        normalized_options[key] = value

    path_part = str(source).partition("|")[0]
    source_is_network = path_part.lower().startswith(("http://", "https://"))
    if _is_encoded_provider_uri(path_part):
        source_fields = _provider_uri_security_fields(path_part)
        source_is_network = "url" in source_fields

    normalized_provider = _normalize_layer_provider(layer_type, provider).casefold()
    if source_is_network:
        if normalized_provider and normalized_provider not in _FORMAL_NETWORK_PROVIDERS:
            raise PermissionError(
                "Formal network layer source uses an unapproved provider"
            )
        if set(normalized_options) - _FORMAL_NETWORK_URI_OPTION_FIELDS:
            raise PermissionError(
                "Formal network layer URI options contain local, connection, auth, "
                "or unknown fields"
            )
        network_url = (
            path_part
            if not _is_encoded_provider_uri(path_part)
            else _provider_uri_security_fields(path_part)["url"]
        )
        policy.validate_network_url(network_url)
        return

    if normalized_provider and normalized_provider not in _FORMAL_LOCAL_PROVIDERS:
        raise PermissionError(
            "Formal local layer source uses an unapproved provider"
        )
    if set(normalized_options) - _FORMAL_LOCAL_PROVIDER_METADATA_FIELDS:
        raise PermissionError(
            "Formal local layer URI options contain network, connection, auth, "
            "path override, or unknown fields"
        )


def _normalize_layer_source_path(source: str) -> str:
    if _is_remote_or_provider_uri(source):
        return source
    path_part, separator, suffix = source.partition("|")
    if not path_part or _is_remote_or_provider_uri(path_part):
        return source
    if _looks_like_path_value(path_part):
        return _safe_workspace_path(path_part) + separator + suffix
    return source


def _is_encoded_provider_uri(source: str) -> bool:
    lowered = source.strip().lower()
    if lowered.startswith(("http://", "https://", "file://", "ftp://")):
        return False
    if Path(source).drive:
        return False
    return bool(
        re.search(
            r"(?:^|[&\s])(?:type|url|dbname|service|host|path|file|filename)\s*=",
            lowered,
            re.IGNORECASE,
        )
    )


def _is_remote_or_provider_uri(source: str) -> bool:
    lowered = source.lower()
    if lowered.startswith(("http://", "https://", "file://", "ftp://")):
        return True
    if "://" in lowered:
        return True
    if lowered.startswith(("type=", "url=", "dbname=", "service=", "host=")):
        return True
    if _is_local_provider_datasource(source):
        return True
    return "=" in lowered and not Path(source).drive


def _is_local_provider_datasource(source: str) -> bool:
    return bool(
        re.match(
            r"^(?:CSV|FILEGDB|GPKG|OPENFILEGDB|PGEO|SQLITE)\s*:",
            source.strip(),
            re.IGNORECASE,
        )
    )


def _source_identity_values(source: str) -> set[str]:
    values = {source, _normalize_layer_source_path(source)}
    if _is_encoded_provider_uri(source):
        parts = _provider_uri_parts(source)
        for key in ("url", "path", "file", "filename"):
            for value in parts.get(key, []):
                if value:
                    values.add(value)
                    values.add(_normalize_layer_source_path(value))
    return {value for value in values if value}


def _layer_name_from_source(source: str, layer_type: str) -> str:
    path_part = source.split("|", 1)[0]
    if not _is_remote_or_provider_uri(path_part):
        stem = Path(path_part).stem
        if stem:
            return stem
    return layer_type.replace("_", " ").title()


def _layer_summary(layer: Any) -> dict[str, Any]:
    provider = ""
    try:
        provider = layer.providerType()
    except Exception:
        provider = ""
    try:
        layer_type_value = int(layer.type())
    except Exception:
        try:
            layer_type_value = str(layer.type())
        except Exception:
            layer_type_value = ""
    return {
        "id": layer.id(),
        "name": layer.name(),
        "source": layer.source(),
        "type": layer_type_value,
        "provider": provider,
        "valid": layer.isValid(),
    }


def _layer_edit_state(layer: Any) -> dict[str, Any]:
    editable = bool(getattr(layer, "isEditable", lambda: False)())
    modified = bool(getattr(layer, "isModified", lambda: False)())
    state = {
        "editable": editable,
        "modified": modified,
        "added_feature_count": 0,
        "deleted_feature_count": 0,
        "changed_attribute_feature_count": 0,
        "changed_geometry_count": 0,
    }
    buffer = getattr(layer, "editBuffer", lambda: None)()
    if buffer is None:
        return state
    for key, method_name in (
        ("added_feature_count", "addedFeatures"),
        ("deleted_feature_count", "deletedFeatureIds"),
        ("changed_attribute_feature_count", "changedAttributeValues"),
        ("changed_geometry_count", "changedGeometries"),
    ):
        try:
            state[key] = len(getattr(buffer, method_name)())
        except Exception:
            pass
    return state


def _layer_has_unsaved_edits(layer: Any) -> bool:
    state = _layer_edit_state(layer)
    return bool(state["editable"] or state["modified"])


def _apply_layer_removal_edit_policy(
    layers: list[Any],
    editable_changes: str,
) -> list[str]:
    editable_layers = _validate_layer_removal_edit_policy(
        layers,
        editable_changes,
    )
    if not editable_layers:
        return []
    if editable_changes == "reject":
        raise RuntimeError(
            "Layer removal rejected because one or more target layers are editable. "
            "Request a new preview with editable_changes set to save or discard."
        )

    method_name = "commitChanges" if editable_changes == "save" else "rollBack"
    action = "save" if editable_changes == "save" else "discard"
    operation = getattr(editable_layers[0], method_name, None)
    if not callable(operation):
        raise RuntimeError(
            f"Could not {action} editable changes for layer {editable_layers[0].id()}: "
            f"{method_name} is unavailable"
        )

    layer = editable_layers[0]
    try:
        succeeded = bool(operation())
    except Exception as err:
        raise RuntimeError(
            f"Could not {action} editable changes for layer {layer.id()}"
        ) from err
    if not succeeded:
        raise RuntimeError(
            f"Could not {action} editable changes for layer {layer.id()}"
        )
    if _layer_has_unsaved_edits(layer):
        raise RuntimeError(
            f"Layer {layer.id()} remained editable after attempting to {action} changes"
        )
    return [str(layer.id())]


def _validate_layer_removal_edit_policy(
    layers: list[Any],
    editable_changes: str,
) -> list[Any]:
    if editable_changes not in LAYER_REMOVAL_EDITABLE_CHANGES:
        raise RuntimeError("editable_changes must be reject, save or discard")
    editable_layers = [layer for layer in layers if _layer_has_unsaved_edits(layer)]
    if editable_changes in {"save", "discard"} and len(editable_layers) > 1:
        raise RuntimeError(
            f"editable_changes={editable_changes} cannot safely process more than "
            "one editable layer in a single removal. Preview and confirm each "
            "editable layer separately."
        )
    return editable_layers


def _layer_removal_fingerprint(layers: list[Any]) -> str:
    from qcopilots_common.processing_jobs import json_safe_value

    if len(layers) > MAX_LAYER_REMOVAL_LAYERS:
        raise RuntimeError(
            "Layer removal preview exceeds the layer count limit of "
            f"{MAX_LAYER_REMOVAL_LAYERS}"
        )
    details = []
    edit_entry_count = 0
    encoded_bytes = 0

    def append_entry(entries: list[Any], value: Any) -> None:
        nonlocal edit_entry_count, encoded_bytes
        edit_entry_count += 1
        if edit_entry_count > MAX_LAYER_REMOVAL_EDIT_ENTRIES:
            raise RuntimeError(
                "Layer removal preview exceeds the edit entry limit of "
                f"{MAX_LAYER_REMOVAL_EDIT_ENTRIES}"
            )
        encoded_bytes += len(
            json.dumps(
                value,
                ensure_ascii=False,
                sort_keys=True,
                allow_nan=False,
                separators=(",", ":"),
            ).encode("utf-8")
        )
        if encoded_bytes > MAX_LAYER_REMOVAL_FINGERPRINT_BYTES:
            raise RuntimeError(
                "Layer removal preview fingerprint exceeds the byte limit of "
                f"{MAX_LAYER_REMOVAL_FINGERPRINT_BYTES}"
            )
        entries.append(value)

    for layer in layers:
        if layer is None:
            details.append(None)
            continue
        buffer = getattr(layer, "editBuffer", lambda: None)()
        buffer_state = {}
        if buffer is not None:
            try:
                added_features = []
                for feature_id, feature in sorted(buffer.addedFeatures().items()):
                    append_entry(
                        added_features,
                        {
                            "feature_id": int(feature_id),
                            "attributes": json_safe_value(feature.attributes()),
                            "geometry_wkb": (
                                bytes(feature.geometry().asWkb()).hex()
                                if not feature.geometry().isNull()
                                else ""
                            ),
                        },
                    )
                buffer_state["added_features"] = added_features
            except RuntimeError:
                raise
            except Exception as err:
                raise RuntimeError(
                    "Could not establish an exact layer removal version for added features"
                ) from err
            try:
                deleted_feature_ids = []
                for feature_id in sorted(buffer.deletedFeatureIds()):
                    append_entry(deleted_feature_ids, int(feature_id))
                buffer_state["deleted_feature_ids"] = deleted_feature_ids
            except RuntimeError:
                raise
            except Exception as err:
                raise RuntimeError(
                    "Could not establish an exact layer removal version for deleted features"
                ) from err
            try:
                changed_attributes = []
                for feature_id, values in sorted(
                    buffer.changedAttributeValues().items()
                ):
                    append_entry(
                        changed_attributes,
                        {
                            "feature_id": int(feature_id),
                            "values": [
                                [int(field_index), json_safe_value(value)]
                                for field_index, value in sorted(values.items())
                            ],
                        },
                    )
                buffer_state["changed_attributes"] = changed_attributes
            except RuntimeError:
                raise
            except Exception as err:
                raise RuntimeError(
                    "Could not establish an exact layer removal version for changed attributes"
                ) from err
            try:
                changed_geometries = []
                for feature_id, geometry in sorted(
                    buffer.changedGeometries().items()
                ):
                    append_entry(
                        changed_geometries,
                        [int(feature_id), bytes(geometry.asWkb()).hex()],
                    )
                buffer_state["changed_geometries"] = changed_geometries
            except RuntimeError:
                raise
            except Exception as err:
                raise RuntimeError(
                    "Could not establish an exact layer removal version for changed geometries"
                ) from err
        layer_version = {
            "id": str(layer.id()),
            "name": str(layer.name()),
            "source": str(layer.source()),
            "provider": str(getattr(layer, "providerType", lambda: "")()),
            "type": str(getattr(layer, "type", lambda: "")()),
            "edit_state": _layer_edit_state(layer),
            "edit_buffer": buffer_state,
        }
        encoded_bytes += len(
            json.dumps(
                layer_version,
                ensure_ascii=False,
                sort_keys=True,
                allow_nan=False,
                separators=(",", ":"),
            ).encode("utf-8")
        )
        if encoded_bytes > MAX_LAYER_REMOVAL_FINGERPRINT_BYTES:
            raise RuntimeError(
                "Layer removal preview fingerprint exceeds the byte limit of "
                f"{MAX_LAYER_REMOVAL_FINGERPRINT_BYTES}"
            )
        details.append(layer_version)
    encoded = json.dumps(details, ensure_ascii=False, sort_keys=True).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def _detailed_layer_metadata(layer: Any, *, include_source: bool) -> dict[str, Any]:
    from qgis.core import QgsRasterLayer, QgsVectorLayer, QgsWkbTypes

    metadata = _layer_summary(layer)
    if not include_source:
        metadata.pop("source", None)
    metadata["crs"] = _crs_metadata(layer.crs())
    metadata["extent"] = _extent_values(layer.extent())
    try:
        renderer = layer.renderer()
        metadata["renderer_type"] = renderer.type() if renderer else None
    except Exception:
        metadata["renderer_type"] = None
    if isinstance(layer, QgsVectorLayer):
        fields = []
        for field in layer.fields():
            fields.append(
                {
                    "name": field.name(),
                    "type": field.typeName(),
                    "alias": field.alias(),
                    "length": field.length(),
                    "precision": field.precision(),
                }
            )
        metadata.update(
            {
                "geometry_type": QgsWkbTypes.displayString(layer.wkbType()),
                "feature_count": int(layer.featureCount()),
                "selected_feature_count": int(layer.selectedFeatureCount()),
                "fields": fields,
                "editable": bool(layer.isEditable()),
                "labels_enabled": bool(layer.labelsEnabled()),
            }
        )
    elif isinstance(layer, QgsRasterLayer):
        metadata.update(
            {
                "band_count": int(layer.bandCount()),
                "width": int(layer.width()),
                "height": int(layer.height()),
                "pixel_size_x": _finite_number(layer.rasterUnitsPerPixelX()),
                "pixel_size_y": _finite_number(layer.rasterUnitsPerPixelY()),
                "band_names": [
                    layer.bandName(index)
                    for index in range(1, layer.bandCount() + 1)
                ],
            }
        )
    return metadata


def _crs_metadata(crs: Any) -> dict[str, Any]:
    try:
        wkt = str(crs.toWkt())
    except Exception:
        wkt = ""
    try:
        proj = str(crs.toProj())
    except Exception:
        proj = ""
    bounded_wkt, wkt_truncated = _bounded_crs_text(wkt)
    bounded_proj, proj_truncated = _bounded_crs_text(proj)
    return {
        "valid": bool(crs.isValid()),
        "auth_id": str(crs.authid()),
        "description": str(crs.description()),
        "geographic": bool(crs.isGeographic()),
        "wkt": bounded_wkt,
        "wkt_truncated": wkt_truncated,
        "proj": bounded_proj,
        "proj_truncated": proj_truncated,
    }


def _bounded_crs_text(value: str, maximum: int = 65536) -> tuple[str, bool]:
    encoded = value.encode("utf-8")
    if len(encoded) <= maximum:
        return value, False
    return encoded[:maximum].decode("utf-8", errors="ignore"), True


def _extent_values(extent: Any) -> list[float | None]:
    return [
        _finite_number(extent.xMinimum()),
        _finite_number(extent.yMinimum()),
        _finite_number(extent.xMaximum()),
        _finite_number(extent.yMaximum()),
    ]


def _finite_number(value: Any) -> float | None:
    try:
        numeric = float(value)
    except (TypeError, ValueError):
        return None
    return numeric if math.isfinite(numeric) else None


def _layer_remove_selectors(arguments: dict[str, Any]) -> dict[str, set[str]]:
    return {
        "ids": _selector_values(arguments, "layer_id", "layer_ids"),
        "names": _selector_values(arguments, "name", "names"),
        "sources": {
            identity
            for value in _selector_values(arguments, "source", "sources")
            for identity in _source_identity_values(value)
        },
    }


def _selector_values(arguments: dict[str, Any], single_key: str, many_key: str) -> set[str]:
    values = set()
    single = arguments.get(single_key)
    if isinstance(single, str) and single:
        values.add(single)
    many = arguments.get(many_key) or []
    if isinstance(many, list):
        values.update(str(value) for value in many if str(value))
    return values


def _layer_matches_selectors(layer: Any, selectors: dict[str, set[str]]) -> bool:
    if selectors["ids"] and layer.id() in selectors["ids"]:
        return True
    if selectors["ids"]:
        return False
    matched = False
    if selectors["names"]:
        matched = True
        if layer.name() not in selectors["names"]:
            return False
    if selectors["sources"]:
        matched = True
        sources = _source_identity_values(layer.source())
        if not sources & selectors["sources"]:
            return False
    return matched


def _layers_matching_selectors(
    layers: list[Any],
    selectors: dict[str, set[str]],
    allow_multiple: bool = False,
) -> list[Any]:
    matches = [
        layer
        for layer in layers
        if _layer_matches_selectors(layer, selectors)
    ]
    if selectors["ids"] or allow_multiple or len(matches) <= 1:
        return matches
    raise RuntimeError(
        "Layer selector matched multiple layers. Use layer_id/layer_ids "
        "or set allow_multiple to true."
    )


def _algorithm_matches_category(algorithm: Any, category: str) -> bool:
    return processing_algorithm_matches_domain(algorithm, category)


def _processing_algorithm_is_listable(algorithm: Any, category: str) -> bool:
    if not _algorithm_matches_category(algorithm, category):
        return False
    return category != "general" or bool(
        processing_algorithm_start_policy(algorithm)["supported"]
    )


def _validate_processing_algorithm_max_results(max_results: Any) -> None:
    if isinstance(max_results, bool) or not isinstance(max_results, int):
        raise ValueError("max_results must be an integer")
    if not 1 <= max_results <= MAX_PROCESSING_ALGORITHM_RESULTS:
        raise ValueError(
            "max_results must be between 1 and "
            f"{MAX_PROCESSING_ALGORITHM_RESULTS}"
        )


def _encode_processing_algorithm_cursor(
    state: dict[str, Any],
    secret: bytes,
) -> str:
    payload = json.dumps(
        state,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    encoded_payload = _processing_algorithm_base64_encode(payload)
    signature = hmac.new(
        secret,
        encoded_payload.encode("ascii"),
        hashlib.sha256,
    ).digest()
    return encoded_payload + "." + _processing_algorithm_base64_encode(signature)


def _decode_processing_algorithm_cursor(
    cursor: Any,
    secret: bytes,
) -> dict[str, Any]:
    try:
        if not isinstance(cursor, str):
            raise ValueError
        if not 1 <= len(cursor) <= MAX_PROCESSING_ALGORITHM_CURSOR_LENGTH:
            raise ValueError
        encoded_payload, encoded_signature = cursor.split(".")
        signature = _processing_algorithm_base64_decode(encoded_signature)
        expected_signature = hmac.new(
            secret,
            encoded_payload.encode("ascii"),
            hashlib.sha256,
        ).digest()
        if not hmac.compare_digest(signature, expected_signature):
            raise ValueError
        state = json.loads(
            _processing_algorithm_base64_decode(encoded_payload).decode("utf-8")
        )
        expected_keys = {
            "version",
            "category",
            "next_offset",
            "max_results",
            "returned_total",
            "previous_algorithm_id",
            "prefix_digest",
        }
        if not isinstance(state, dict) or set(state) != expected_keys:
            raise ValueError
        if type(state["version"]) is not int or state["version"] != 1:
            raise ValueError
        if not isinstance(state["category"], str) or state["category"] not in {
            "vector",
            "raster",
            "general",
        }:
            raise ValueError
        _validate_processing_algorithm_max_results(state["max_results"])
        if (
            isinstance(state["next_offset"], bool)
            or not isinstance(state["next_offset"], int)
            or state["next_offset"] < 1
        ):
            raise ValueError
        if (
            isinstance(state["returned_total"], bool)
            or not isinstance(state["returned_total"], int)
            or not 1 <= state["returned_total"] < state["max_results"]
            or state["returned_total"] > state["next_offset"]
        ):
            raise ValueError
        if (
            not isinstance(state["previous_algorithm_id"], str)
            or not state["previous_algorithm_id"]
        ):
            raise ValueError
        if not isinstance(state["prefix_digest"], str) or not re.fullmatch(
            r"[0-9a-f]{64}", state["prefix_digest"]
        ):
            raise ValueError
        return state
    except (
        binascii.Error,
        json.JSONDecodeError,
        UnicodeError,
        ValueError,
    ) as err:
        raise _processing_algorithm_cursor_error(
            "cursor is invalid or expired"
        ) from err


def _processing_algorithm_base64_encode(value: bytes) -> str:
    return base64.urlsafe_b64encode(value).rstrip(b"=").decode("ascii")


def _processing_algorithm_base64_decode(value: str) -> bytes:
    if not value or not re.fullmatch(r"[A-Za-z0-9_-]+", value):
        raise ValueError
    padding = "=" * (-len(value) % 4)
    return base64.b64decode(
        (value + padding).encode("ascii"),
        altchars=b"-_",
        validate=True,
    )


def _validate_processing_algorithm_cursor_prefix(
    entries: list[tuple[Any, Any]],
    state: dict[str, Any],
) -> None:
    offset = state["next_offset"]
    if offset > len(entries):
        raise _processing_algorithm_cursor_error(
            "algorithm list changed while paging"
        )
    previous_algorithm_id = str(entries[offset - 1][1].id())
    if (
        previous_algorithm_id != state["previous_algorithm_id"]
        or _processing_algorithm_prefix_digest(entries, offset)
        != state["prefix_digest"]
    ):
        raise _processing_algorithm_cursor_error(
            "algorithm list changed while paging"
        )


def _processing_algorithm_prefix_digest(
    entries: list[tuple[Any, Any]],
    end: int,
) -> str:
    digest = hashlib.sha256()
    for provider, algorithm in entries[:end]:
        identity = json.dumps(
            [str(provider.id()), str(algorithm.id())],
            ensure_ascii=False,
            separators=(",", ":"),
        ).encode("utf-8")
        digest.update(len(identity).to_bytes(4, "big"))
        digest.update(identity)
    return digest.hexdigest()


def _processing_algorithm_cursor_error(detail: str) -> ProcessingAlgorithmCursorError:
    return ProcessingAlgorithmCursorError(
        f"{PROCESSING_ALGORITHM_CURSOR_ERROR_PREFIX} {detail}. "
        "Start again without cursor."
    )


def _safe_workspace_path(value: str | Path) -> str:
    candidate = Path(value).expanduser()
    if not candidate.is_absolute():
        candidate = Path.cwd().resolve() / candidate
    candidate = candidate.resolve(strict=False)
    return str(candidate)


def _require_available_output_path(
    path: Path,
    overwrite: bool,
    description: str,
) -> None:
    if path.exists() and not overwrite:
        raise FileExistsError(
            f"{description} already exists. Set overwrite to true only after "
            f"approving the exact target: {path}"
        )


def _authorize_interactive_overwrite(
    store: DestructiveActionConfirmationStore,
    arguments: dict[str, Any],
    target: Path,
    action: str,
    description: str,
    *,
    binding: dict[str, Any],
    family_paths: list[Path] | None = None,
) -> tuple[dict[str, Any] | None, list[dict[str, Any]]]:
    paths = family_paths or [target]
    metadata_versions = [_interactive_file_metadata(path) for path in paths]
    collisions = [item for item in metadata_versions if item["exists"]]
    overwrite = bool(arguments.get("overwrite", False))
    token = str(arguments.get("overwrite_confirmation_token") or "").strip()
    if not collisions:
        if token:
            raise RuntimeError(
                "overwrite_confirmation_token was supplied, but the output target "
                "does not exist"
            )
        return None, metadata_versions
    if not overwrite:
        raise FileExistsError(
            f"{description} already exists. Set overwrite to true to request an "
            f"exact, one-use confirmation preview: {target}"
        )
    oversized = [
        item
        for item in collisions
        if not item["is_directory"]
        and item["size"] > MAX_INTERACTIVE_OVERWRITE_HASH_BYTES
    ]
    if oversized:
        raise RuntimeError(
            "Interactive overwrite preview refuses existing files larger than "
            f"{MAX_INTERACTIVE_OVERWRITE_HASH_BYTES} bytes on the QGIS GUI "
            "thread. Use a new output path: "
            + ", ".join(item["path"] for item in oversized)
        )
    directories = [item["path"] for item in collisions if item["is_directory"]]
    if directories:
        raise RuntimeError(
            "Interactive overwrite does not support directory targets: "
            + ", ".join(directories)
        )
    versions = [_interactive_file_version(path) for path in paths]
    snapshot = {
        "action": action,
        "binding": binding,
        "targets": versions,
    }
    fingerprint = hashlib.sha256(
        json.dumps(snapshot, ensure_ascii=False, sort_keys=True).encode("utf-8")
    ).hexdigest()
    if token:
        store.consume(token, action, fingerprint)
        return None, versions
    issued = store.issue(action, fingerprint, snapshot)
    return (
        {
            "preview": True,
            "confirmation_required": True,
            "action": action,
            "overwrite": True,
            "overwrite_targets": collisions,
            "overwrite_confirmation_token": issued,
            "confirmation_expires_in_seconds": store.ttl_seconds,
        },
        versions,
    )


def _interactive_file_metadata(path: Path) -> dict[str, Any]:
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
    return version


def _interactive_file_version(path: Path) -> dict[str, Any]:
    version = _interactive_file_metadata(path)
    if not version["exists"] or version["is_directory"]:
        return version
    if version["size"] > MAX_INTERACTIVE_OVERWRITE_HASH_BYTES:
        raise RuntimeError(
            "Interactive overwrite cannot strongly version an existing file larger "
            f"than {MAX_INTERACTIVE_OVERWRITE_HASH_BYTES} bytes: {path}"
        )
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    current = _interactive_file_metadata(path)
    if current != version:
        raise RuntimeError(f"Output target changed while it was being versioned: {path}")
    version["sha256"] = digest.hexdigest()
    return version


def _validate_interactive_versions(
    expected_versions: list[dict[str, Any]],
) -> None:
    changed = [
        item["path"]
        for item in expected_versions
        if _interactive_file_version(Path(item["path"])) != item
    ]
    if changed:
        raise RuntimeError(
            "Output target changed after overwrite confirmation: "
            + ", ".join(changed)
        )


def _layout_page_dimensions(page_size: str, orientation: str) -> tuple[float, float]:
    dimensions = {
        "a4": (210.0, 297.0),
        "a3": (297.0, 420.0),
        "a2": (420.0, 594.0),
        "a1": (594.0, 841.0),
        "a0": (841.0, 1189.0),
        "letter": (215.9, 279.4),
        "legal": (215.9, 355.6),
    }
    normalized_size = page_size.strip().lower()
    normalized_orientation = orientation.strip().lower()
    if normalized_size not in dimensions:
        raise RuntimeError(f"Unsupported print layout page_size: {page_size}")
    if normalized_orientation not in {"portrait", "landscape"}:
        raise RuntimeError(f"Unsupported print layout orientation: {orientation}")
    width, height = dimensions[normalized_size]
    if normalized_orientation == "landscape":
        width, height = height, width
    return width, height


def _layout_export_target(
    value: str | Path,
    requested_format: Any,
) -> tuple[Path, str]:
    path = Path(value)
    suffix = path.suffix.lower()
    suffix_formats = {
        ".pdf": "pdf",
        ".png": "png",
        ".jpg": "jpeg",
        ".jpeg": "jpeg",
        ".svg": "svg",
    }
    suffix_format = suffix_formats.get(suffix)
    export_format = str(requested_format or suffix_format or "").strip().lower()
    if export_format == "jpg":
        export_format = "jpeg"
    if export_format not in {"pdf", "png", "jpeg", "svg"}:
        raise RuntimeError(
            "Layout output path must end in .pdf, .png, .jpg, .jpeg or .svg"
        )
    if suffix_format != export_format:
        raise RuntimeError(
            "Layout export format must match the output path suffix: "
            f"format={export_format}, path={path}"
        )
    return path, export_format


def _validate_layout_export_pixels(layout: Any, dpi: float) -> None:
    from qgis.core import QgsLayoutMeasurementConverter, QgsUnitTypes

    converter = QgsLayoutMeasurementConverter()
    total_pixels = 0
    pages = layout.pageCollection()
    for index in range(pages.pageCount()):
        page_size = converter.convert(
            pages.page(index).pageSize(),
            QgsUnitTypes.LayoutUnit.LayoutMillimeters,
        )
        width_pixels = math.ceil(float(page_size.width()) / 25.4 * dpi)
        height_pixels = math.ceil(float(page_size.height()) / 25.4 * dpi)
        total_pixels += width_pixels * height_pixels
        if total_pixels > MAX_LAYOUT_EXPORT_PIXELS:
            raise RuntimeError(
                "Layout export exceeds the synchronous pixel limit of "
                f"{MAX_LAYOUT_EXPORT_PIXELS} pixels. Reduce DPI or page size."
            )


def _new_staged_output_path(target: Path) -> Path:
    with tempfile.NamedTemporaryFile(
        mode="wb",
        prefix=f".{target.stem}.qcopilots-",
        suffix=target.suffix,
        dir=str(target.parent),
        delete=False,
    ) as handle:
        return Path(handle.name)


def _map_image_world_file_path(path: Path) -> Path:
    suffix = path.suffix.lower().lstrip(".")
    if not suffix:
        raise RuntimeError(f"Map image output has no suffix: {path}")
    return path.with_suffix(f".{suffix[0]}{suffix[-1]}w")


def _publish_staged_output_family(
    staging_root: Path,
    target: Path,
    family_paths: list[Path],
    *,
    overwrite: bool,
    expected_versions: list[dict[str, Any]],
) -> list[str]:
    from qcopilots_common.processing_jobs import (
        _commit_processing_output_promotion,
        _promote_processing_outputs,
    )

    expected_names = {path.name for path in family_paths}
    artifact_names = {
        path.name for path in staging_root.iterdir() if path.is_file()
    }
    if artifact_names != expected_names:
        raise RuntimeError(
            "Staged output family does not match its authorized targets. "
            f"Expected {sorted(expected_names)}, produced {sorted(artifact_names)}"
        )
    stage = {
        "parameter": "path",
        "target": target,
        "family_paths": family_paths,
        "staging_root": staging_root,
        "staged_target": staging_root / target.name,
        "authorized_versions": expected_versions,
    }
    try:
        receipt = _promote_processing_outputs([stage], overwrite=overwrite)
    except RuntimeError as err:
        marker = "Processing overwrite target changed after confirmation:"
        if marker in str(err):
            raise RuntimeError(
                str(err).replace(
                    marker,
                    "Output target changed after overwrite confirmation:",
                    1,
                )
            ) from err
        raise
    return _commit_processing_output_promotion(receipt)


def _publish_staged_output(
    staged: Path,
    target: Path,
    *,
    overwrite: bool,
    expected_versions: list[dict[str, Any]] | None = None,
) -> list[str]:
    staging_root = Path(
        tempfile.mkdtemp(
            prefix=f".{target.stem}.qcopilots-publish-",
            dir=str(target.parent),
        )
    )
    family_staged = staging_root / target.name
    cleanup_residuals = []
    primary_error = None
    try:
        os.link(staged, family_staged)
        cleanup_residuals.extend(
            _publish_staged_output_family(
                staging_root,
                target,
                [target],
                overwrite=overwrite,
                expected_versions=expected_versions or [],
            )
        )
    except Exception as err:
        primary_error = err
        raise
    finally:
        stage_residuals = _cleanup_interactive_paths([staging_root])
        if stage_residuals:
            if primary_error is not None:
                raise RuntimeError(
                    f"{primary_error}. Output publication staging cleanup was "
                    "incomplete: " + ", ".join(stage_residuals)
                ) from primary_error
            cleanup_residuals.extend(stage_residuals)
    return sorted(set(cleanup_residuals))


def _cleanup_interactive_paths(paths: list[Path]) -> list[str]:
    residuals = []
    for path in paths:
        try:
            if path.is_dir():
                shutil.rmtree(path)
                if path.exists():
                    raise RuntimeError("directory still exists after removal")
            else:
                path.unlink(missing_ok=True)
                if path.exists():
                    raise RuntimeError("file still exists after removal")
        except Exception:
            residuals.append(str(path))
    return residuals


def _sanitize_processing_parameters(
    value: Any,
    parameter_definitions: Any | None = None,
    key: str = "",
) -> Any:
    if parameter_definitions is not None and isinstance(value, dict):
        definitions_by_name = {
            str(parameter.name()).lower(): parameter
            for parameter in parameter_definitions
        }
        return {
            item_key: _sanitize_processing_value(
                item_value,
                str(item_key),
                _processing_parameter_accepts_workspace_path(
                    definitions_by_name.get(str(item_key).lower())
                ),
                str(item_key).lower() in definitions_by_name,
            )
            for item_key, item_value in value.items()
        }
    return _sanitize_processing_value(value, key, False, False)


def _sanitize_processing_value(
    value: Any,
    key: str,
    parameter_accepts_paths: bool,
    known_parameter: bool,
) -> Any:
    if isinstance(value, dict):
        return {
            item_key: _sanitize_processing_value(
                item_value,
                str(item_key),
                parameter_accepts_paths,
                False,
            )
            for item_key, item_value in value.items()
        }
    if isinstance(value, list):
        return [
            _sanitize_processing_value(
                item,
                key,
                parameter_accepts_paths,
                known_parameter,
            )
            for item in value
        ]
    if isinstance(value, str) and (
        _looks_like_processing_path_value(value)
        and (parameter_accepts_paths or (not known_parameter and _looks_like_file_parameter(key, value)))
    ):
        return _normalize_processing_path_value(value)
    return value


def _processing_parameter_accepts_workspace_path(parameter: Any | None) -> bool:
    if parameter is None:
        return False
    type_text = _processing_parameter_type_text(parameter)
    class_text = _processing_parameter_class_text(parameter)
    path_type_tokens = (
        "destination",
        "file",
        "folder",
        "maplayer",
        "mesh",
        "multilayer",
        "multiplelayers",
        "raster",
        "sink",
        "source",
        "vector",
    )
    path_class_tokens = (
        "qgsprocessingparameterfeature",
        "qgsprocessingparameterfile",
        "qgsprocessingparameterfolder",
        "qgsprocessingparametermaplayer",
        "qgsprocessingparametermeshlayer",
        "qgsprocessingparametermultiplelayers",
        "qgsprocessingparameterraster",
        "qgsprocessingparametersink",
        "qgsprocessingparametervector",
    )
    return any(token in type_text for token in path_type_tokens) or any(
        token in class_text for token in path_class_tokens
    )


def _processing_parameter_type_text(parameter: Any) -> str:
    try:
        return str(parameter.type()).lower()
    except Exception:
        return ""


def _processing_parameter_class_text(parameter: Any) -> str:
    for accessor in ("className", "typeName"):
        try:
            value = getattr(parameter, accessor)()
        except Exception:
            continue
        if value:
            return str(value).lower()
    return type(parameter).__name__.lower()


def _looks_like_file_parameter(key: str, value: str) -> bool:
    if not _looks_like_processing_path_value(value):
        return False
    lowered_key = key.lower()
    if any(token in lowered_key for token in ("path", "file", "folder", "output", "destination")):
        return True
    if lowered_key in ("input", "source", "uri", "database"):
        return _looks_like_path_value(value)
    return Path(value).is_absolute()


def _looks_like_processing_path_value(value: str) -> bool:
    if not value or value.lower().startswith(("memory:", "qgis:", "/vsi")):
        return False
    if value.upper() == "TEMPORARY_OUTPUT":
        return False
    path_part = value.split("|", 1)[0]
    scheme = _processing_uri_scheme(path_part)
    if scheme and scheme != "file":
        return False
    return scheme == "file" or _looks_like_path_value(path_part)


def _normalize_processing_path_value(value: str) -> str:
    path_part, separator, provider_options = value.partition("|")
    scheme = _processing_uri_scheme(path_part)
    if scheme and scheme != "file":
        return value
    if scheme == "file":
        normalized = _file_uri_path(path_part)
    else:
        normalized = _safe_workspace_path(path_part)
    return normalized + (separator + provider_options if separator else "")


def _processing_uri_scheme(value: str) -> str:
    if re.match(r"^[A-Za-z]:[\\/]", value):
        return ""
    match = re.match(r"^([A-Za-z][A-Za-z0-9+.-]*):", value)
    return match.group(1).lower() if match else ""


def _file_uri_path(value: str) -> str:
    parsed = urlsplit(value)
    if parsed.scheme.lower() != "file":
        raise RuntimeError(f"Not a file URI: {value}")
    path = unquote(parsed.path)
    if parsed.netloc and parsed.netloc.lower() != "localhost":
        path = f"//{parsed.netloc}{path}"
    elif re.match(r"^/[A-Za-z]:/", path):
        path = path[1:]
    return _safe_workspace_path(path)


def _looks_like_path_value(value: str) -> bool:
    candidate = Path(value)
    return bool(
        "\0" in value
        or re.match(r"^[A-Za-z]:", value)
        or value.startswith("~")
        or candidate.is_absolute()
        or "/" in value
        or "\\" in value
        or candidate.suffix
    )


def _looks_like_policy_local_path_value(value: str) -> bool:
    raw = str(value).strip()
    if not raw or raw.casefold().startswith("/vsi"):
        return False
    if "\0" in raw or re.match(r"^[A-Za-z]:", raw):
        return True
    if re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", raw) or "://" in raw:
        return False
    return raw.startswith("~") or _looks_like_path_value(raw)
