# SPDX-License-Identifier: GPL-2.0-or-later
"""Bounded HTTP transport for the WebSearch JSON Streamable HTTP endpoint."""
from __future__ import annotations
import http.client
import json
import socket
import ssl
import threading
import time
from concurrent.futures import Future, TimeoutError as WaitTimeout
from dataclasses import dataclass
from urllib.parse import urlsplit
from .config_store import ConfigError, load_config

REQUEST_HEADERS = frozenset({"authorization", "accept", "content-type", "mcp-session-id",
                             "mcp-protocol-version", "last-event-id"})
RESPONSE_HEADERS = frozenset({"content-type", "mcp-session-id", "mcp-protocol-version",
                              "retry-after", "www-authenticate", "allow", "cache-control"})
_MESSAGES = {
    "invalid_configuration": "The WebSearch MCP endpoint configuration is invalid.",
    "backend_unavailable": "The WebSearch MCP endpoint is unavailable.",
    "busy": "The WebSearch relay capacity is currently exhausted.",
    "timeout": "The WebSearch MCP request exceeded its deadline.",
    "cancelled": "The WebSearch MCP request was cancelled.",
    "response_too_large": "The WebSearch MCP response exceeds its size limit.",
    "unsupported_response": "This WebSearch relay requires JSON Streamable HTTP responses. SSE and other content types are not supported.",
    "invalid_response": "The WebSearch MCP endpoint returned an invalid response.",
    "redirect_not_allowed": "The WebSearch MCP endpoint returned a redirect.",
}


class BackendError(Exception):
    def __init__(self, code, status=None):
        self.code = code if code in _MESSAGES else "backend_unavailable"
        self.status = status or {"busy": 429, "timeout": 504, "cancelled": 499}.get(self.code, 502)
        self.safe_message = _MESSAGES[self.code]
        super().__init__(self.safe_message)


@dataclass(frozen=True)
class RelayResponse:
    status: int
    headers: dict[str, str]
    body: bytes


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
        self._maintenance_gate = threading.BoundedSemaphore(2)
        self._active = set()
        self._closed = False

    def exchange(self, method, headers, body=b"", *, disconnected=None, maintenance=False):
        try:
            config = self._config_loader()
        except (ConfigError, OSError, ValueError):
            raise BackendError("invalid_configuration") from None
        if method not in {"GET", "POST", "DELETE"} or not isinstance(body, bytes) or len(body) > 1024 * 1024:
            raise BackendError("invalid_configuration")
        with self._lock:
            if self._closed:
                raise BackendError("backend_unavailable")
            if self._business_gate is None:
                self._capacity = config.max_concurrency
                self._business_gate = threading.BoundedSemaphore(self._capacity)
            elif self._capacity != config.max_concurrency:
                raise BackendError("invalid_configuration")
            gate = self._maintenance_gate if maintenance else self._business_gate
            if not gate.acquire(blocking=False):
                raise BackendError("busy")
            job, future = _Request(config.timeout_seconds), Future()
            self._active.add(job)
            job.thread = threading.Thread(target=self._worker,
                args=(job, future, gate, config, method, dict(headers), body),
                name="qcopilots-websearch-mcp", daemon=True)
            try:
                job.thread.start()
            except RuntimeError:
                self._active.remove(job)
                gate.release()
                raise BackendError("backend_unavailable") from None
        try:
            while True:
                if disconnected is not None and disconnected():
                    raise BackendError("cancelled")
                remaining = job.deadline - time.monotonic()
                if remaining <= 0:
                    raise BackendError("timeout")
                try:
                    return future.result(timeout=min(0.05, remaining))
                except WaitTimeout:
                    continue
        except BaseException:
            job.cancel()
            raise

    def _worker(self, job, future, gate, config, method, headers, body):
        result = failure = None
        try:
            result = self._exchange(job, config, method, headers, body)
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

    def _exchange(self, job, config, method, headers, body):
        parsed = urlsplit(config.backend_url)
        host = parsed.hostname.encode("idna").decode("ascii")
        port = parsed.port or (443 if parsed.scheme == "https" else 80)
        # Stalled OS DNS retains this bounded worker until the lookup exits.
        sock = socket.create_connection((host, port), timeout=job.remaining())
        job.attach(sock)
        if parsed.scheme == "https":
            context = ssl.create_default_context(cafile=config.ca_file)
            sock = context.wrap_socket(sock, server_hostname=host, do_handshake_on_connect=False)
            job.attach(sock)
            sock.settimeout(job.remaining())
            sock.do_handshake()
        authority = f"[{host}]" if ":" in host else host
        if parsed.port is not None:
            authority += f":{parsed.port}"
        outgoing = {}
        for name, value in headers.items():
            lower = name.lower()
            if (lower not in REQUEST_HEADERS or lower in outgoing or not isinstance(value, str)
                    or len(value) > (8199 if lower == "authorization" else 8192)
                    or any(ord(ch) < 32 or ord(ch) > 126 for ch in value)):
                raise BackendError("invalid_configuration")
            outgoing[lower] = value
        request = (f"{method} {parsed.path or '/mcp'} HTTP/1.1\r\nHost: {authority}\r\n"
                   + "".join(f"{name}: {value}\r\n" for name, value in outgoing.items())
                   + f"User-Agent: QCopilots-WebSearch-Relay/0.3.0\r\nAccept-Encoding: identity\r\nConnection: close\r\nContent-Length: {len(body)}\r\n\r\n")
        sock.settimeout(job.remaining())
        sock.sendall(request.encode("ascii") + body)
        response = http.client.HTTPResponse(sock, method=method)
        try:
            sock.settimeout(job.remaining())
            response.begin()
            if 300 <= response.status < 400:
                raise BackendError("redirect_not_allowed")
            if response.getheader("Content-Encoding", "identity").lower() not in {"", "identity"}:
                raise BackendError("unsupported_response")
            lengths = response.headers.get_all("Content-Length", [])
            transfers = response.headers.get_all("Transfer-Encoding", [])
            if (len(lengths) > 1 or (lengths and (not lengths[0].isdigit() or transfers))
                    or len(transfers) > 1 or (transfers and transfers[0].lower() != "chunked")):
                raise BackendError("invalid_response")
            if lengths and int(lengths[0]) > config.max_response_bytes:
                raise BackendError("response_too_large")
            content_type = response.getheader("Content-Type", "").split(";", 1)[0].strip().lower()
            if content_type and content_type != "application/json":
                raise BackendError("unsupported_response")
            selected = {}
            for name in RESPONSE_HEADERS:
                values = response.headers.get_all(name, [])
                if len(values) > 1:
                    raise BackendError("invalid_response")
                if values:
                    value = values[0]
                    if len(value) > 8192 or any(ord(ch) < 32 or ord(ch) > 126 for ch in value):
                        raise BackendError("invalid_response")
                    selected[name] = value
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
                raise BackendError("invalid_response")
            data = bytes(payload)
            if data:
                if content_type != "application/json":
                    raise BackendError("unsupported_response")
                try:
                    json.loads(data.decode("utf-8"), parse_constant=_reject_constant)
                except (ValueError, UnicodeError, RecursionError):
                    raise BackendError("invalid_response") from None
            elif response.status < 400 and response.status not in {202, 204}:
                raise BackendError("invalid_response")
            return RelayResponse(response.status, selected, data)
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
