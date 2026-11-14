# SPDX-License-Identifier: GPL-2.0-or-later
"""Bounded REST client with explicit deadlines, no proxies and no redirects."""
from __future__ import annotations
import http.client
import json
import socket
import ssl
import threading
import time
from concurrent.futures import Future, TimeoutError as WaitTimeout
from urllib.parse import urlsplit
from .config_store import ConfigError, load_config

_ROUTES = {"search": ("POST", "/v1/search"), "open": ("POST", "/v1/open"),
           "find": ("POST", "/v1/find"), "status": ("GET", "/v1/status")}
_MESSAGES = {
    "invalid_argument": "The backend rejected the tool arguments.",
    "unauthorized": "Backend authentication failed.",
    "forbidden": "The request is forbidden by the active policy.",
    "destination_blocked": "The requested destination is not public.",
    "busy": "The backend request capacity is currently exhausted.",
    "rate_limited": "The search provider is rate limited.",
    "timeout": "The backend request exceeded its deadline.",
    "cancelled": "The backend request was cancelled.",
    "provider_unavailable": "The configured search provider is unavailable.",
    "upstream_http_error": "The webpage returned an unsuccessful HTTP status.",
    "backend_unavailable": "The WebSearch backend is unavailable.",
    "cache_unavailable": "The WebSearch cache is unavailable.",
    "page_unavailable": "The requested page is unknown or has expired.",
    "unsupported_content": "The requested content type or encoding is unsupported.",
    "response_too_large": "The upstream response exceeds its size limit.",
    "request_too_large": "The backend request exceeds its size limit.",
    "invalid_configuration": "The WebSearch backend configuration is invalid.",
    "internal_error": "The WebSearch backend reported an internal error.",
}
_RETRYABLE = {"busy", "rate_limited", "timeout", "provider_unavailable", "backend_unavailable", "cache_unavailable"}


class BackendError(Exception):
    def __init__(self, code):
        self.code = code if code in _MESSAGES else "backend_unavailable"
        self.safe_message = _MESSAGES[self.code]
        self.retryable = self.code in _RETRYABLE
        super().__init__(self.safe_message)


class _Request:
    def __init__(self, timeout):
        self.deadline = time.monotonic() + timeout
        self.cancelled = threading.Event()
        self.lock = threading.Lock()
        self.sock = None
        self.thread = None

    def remaining(self):
        if self.cancelled.is_set():
            raise BackendError("cancelled")
        remaining = self.deadline - time.monotonic()
        if remaining <= 0:
            raise BackendError("timeout")
        return remaining

    def attach(self, sock):
        with self.lock:
            if self.cancelled.is_set():
                sock.close()
                raise BackendError("cancelled")
            self.sock = sock
        sock.settimeout(self.remaining())

    def cancel(self):
        self.cancelled.set()
        with self.lock:
            sock, self.sock = self.sock, None
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            sock.close()


def _reject_constant(_value):
    raise ValueError("Non-finite JSON numbers are not permitted.")


class BackendClient:
    def __init__(self, config_loader=load_config):
        self._config_loader = config_loader
        self._lock = threading.Lock()
        self._business_gate = None
        self._capacity = None
        self._status_gate = threading.BoundedSemaphore(1)
        self._active = set()
        self._closed = False

    def call(self, operation, arguments):
        if operation not in _ROUTES or not isinstance(arguments, dict) or (operation == "status" and arguments):
            raise BackendError("invalid_argument")
        try:
            config = self._config_loader()
        except PermissionError:
            raise BackendError("forbidden") from None
        except (ConfigError, OSError, ValueError):
            raise BackendError("invalid_configuration") from None
        try:
            body = json.dumps(arguments, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8") if operation != "status" else b""
        except (TypeError, ValueError, UnicodeError):
            raise BackendError("invalid_argument") from None
        if len(body) > 1024 * 1024:
            raise BackendError("request_too_large")
        with self._lock:
            if self._closed:
                raise BackendError("backend_unavailable")
            if self._business_gate is None:
                self._capacity = config.max_concurrency
                self._business_gate = threading.BoundedSemaphore(self._capacity)
            elif self._capacity != config.max_concurrency:
                raise BackendError("invalid_configuration")
            gate = self._status_gate if operation == "status" else self._business_gate
            if not gate.acquire(blocking=False):
                raise BackendError("busy")
            job = _Request(config.timeout_seconds)
            future = Future()
            self._active.add(job)
            job.thread = threading.Thread(target=self._worker, args=(job, future, gate, config, operation, body),
                                          name="qcopilots-websearch-rest", daemon=True)
            try:
                job.thread.start()
            except RuntimeError:
                self._active.remove(job)
                gate.release()
                raise BackendError("backend_unavailable") from None
        try:
            # The worker closes its socket before publishing the result. That
            # cleanup sets cancelled, so only the deadline controls this wait.
            return future.result(timeout=max(0.0, job.deadline - time.monotonic()))
        except WaitTimeout:
            job.cancel()
            raise BackendError("timeout") from None
        except BackendError:
            job.cancel()
            raise

    def _worker(self, job, future, gate, config, operation, body):
        result = failure = None
        try:
            result = self._exchange(job, config, operation, body)
        except BackendError as error:
            failure = error
        except (TimeoutError, socket.timeout):
            failure = BackendError("timeout")
        except Exception:
            failure = BackendError("backend_unavailable")
        finally:
            job.cancel()
            with self._lock:
                self._active.discard(job)
            gate.release()
        if failure is not None:
            future.set_exception(failure)
        else:
            future.set_result(result)

    def _exchange(self, job, config, operation, body):
        parsed = urlsplit(config.backend_url)
        host = parsed.hostname.encode("idna").decode("ascii")
        port = parsed.port or (443 if parsed.scheme == "https" else 80)
        # A stalled OS DNS lookup retains its bounded worker until it actually exits.
        sock = socket.create_connection((host, port), timeout=job.remaining())
        job.attach(sock)
        if parsed.scheme == "https":
            context = ssl.create_default_context(cafile=config.ca_file)
            sock = context.wrap_socket(sock, server_hostname=host, do_handshake_on_connect=False)
            job.attach(sock)
            sock.settimeout(job.remaining())
            sock.do_handshake()
        method, route = _ROUTES[operation]
        authority = f"[{host}]" if ":" in host else host
        if parsed.port is not None:
            authority += f":{parsed.port}"
        headers = (f"{method} {route} HTTP/1.1\r\nHost: {authority}\r\nAuthorization: Bearer {config.token}\r\n"
            "Accept: application/json\r\nAccept-Encoding: identity\r\nContent-Type: application/json\r\n"
            f"Connection: close\r\nContent-Length: {len(body)}\r\n\r\n").encode("ascii")
        sock.settimeout(job.remaining())
        sock.sendall(headers + body)
        response = http.client.HTTPResponse(sock, method=method)
        try:
            sock.settimeout(job.remaining())
            response.begin()
            if 300 <= response.status < 400:
                raise BackendError("backend_unavailable")
            if response.getheader("Content-Encoding", "identity").lower() not in {"", "identity"}:
                raise BackendError("backend_unavailable")
            lengths = response.headers.get_all("Content-Length", [])
            if lengths and response.getheader("Transfer-Encoding"):
                raise BackendError("backend_unavailable")
            if len(lengths) > 1 or (lengths and not lengths[0].isdigit()):
                raise BackendError("backend_unavailable")
            if lengths and int(lengths[0]) > config.max_response_bytes:
                raise BackendError("response_too_large")
            payload = bytearray()
            while True:
                sock.settimeout(job.remaining())
                chunk = response.read1(min(65536, config.max_response_bytes + 1 - len(payload)))
                job.remaining()
                if not chunk:
                    break
                payload.extend(chunk)
                if len(payload) > config.max_response_bytes:
                    raise BackendError("response_too_large")
            if lengths and len(payload) != int(lengths[0]):
                raise BackendError("backend_unavailable")
            if response.getheader("Content-Type", "").split(";")[0].strip().lower() != "application/json":
                raise BackendError("backend_unavailable")
            value = json.loads(bytes(payload).decode("utf-8"), parse_constant=_reject_constant)
            if not isinstance(value, dict):
                raise BackendError("backend_unavailable")
            if response.status >= 400 or "error" in value:
                error = value.get("error")
                code = error.get("code") if isinstance(error, dict) else None
                if not isinstance(code, str) or code not in _MESSAGES:
                    code = {401: "unauthorized", 403: "forbidden", 429: "busy", 408: "timeout", 504: "timeout"}.get(response.status, "backend_unavailable")
                raise BackendError(code)
            if response.status != 200 or type(value.get("contract_version")) is not int or value["contract_version"] != 1:
                raise BackendError("backend_unavailable")
            return value
        finally:
            response.close()

    def close(self):
        with self._lock:
            self._closed = True
            jobs = tuple(self._active)
        for job in jobs:
            job.cancel()
        deadline = time.monotonic() + 1.0
        for job in jobs:
            if job.thread is not threading.current_thread():
                job.thread.join(max(0.0, deadline - time.monotonic()))
