# SPDX-License-Identifier: GPL-2.0-or-later
"""Loopback MCP relay with a separate Manager-only health endpoint."""
from __future__ import annotations
import hmac
import json
import logging
import select
import socket
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from qcopilots_common.constants import MAX_HTTP_REQUEST_BODY_BYTES
from qcopilots_common.mcp_http_transport import (
    HttpTransportError, cors_response_headers, discard_request_body,
    request_body_length, validate_preflight, validate_request_host,
    validate_request_origin,
)
from .backend_client import BackendClient, BackendError, REQUEST_HEADERS


class _BoundedHttpServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = False

    def __init__(self, *args):
        self._handlers = threading.BoundedSemaphore(80)
        super().__init__(*args)

    def get_request(self):
        request, address = super().get_request()
        request.settimeout(5)
        return request, address

    def process_request(self, request, client_address):
        if not self._handlers.acquire(blocking=False):
            try:
                request.sendall(b"HTTP/1.0 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
            except OSError:
                pass
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self._handlers.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self._handlers.release()


class WebSearchRelayServer:
    def __init__(self, host="127.0.0.1", port=0, path="/mcp", auth_token="",
                 cors_origins=None, client=None, logger=None):
        if host != "127.0.0.1" or path != "/mcp":
            raise ValueError("The WebSearch relay requires the loopback /mcp endpoint.")
        if not auth_token or not auth_token.strip():
            raise ValueError("The WebSearch relay requires a Manager health token.")
        self.host, self.port, self.path = host, port, path
        self._auth_token = auth_token
        self.cors_origins = list(cors_origins or [])
        for origin in self.cors_origins:
            if (not isinstance(origin, str) or not origin or origin != origin.strip()
                    or origin in {"*", "null"} or "\r" in origin or "\n" in origin):
                raise ValueError("Invalid relay browser origin.")
        self.client = client if client is not None else BackendClient()
        self.logger = logger or logging.getLogger("qcopilots.mcp_server_web_search")
        self._httpd = None
        self._state_lock = threading.Lock()
        self._serving_thread = None
        self._closed = False

    def create_http_server(self):
        controller = self

        class Handler(BaseHTTPRequestHandler):
            server_version = "QCopilotsWebSearchRelay/0.3"
            _cors_origin = None
            _body_consumed = False

            def log_message(self, *_args):
                # Base logging includes the untrusted request target.
                pass

            def _policy(self, require_origin=False):
                validate_request_host(self.headers, controller.host, self.server.server_address[1])
                self._cors_origin = validate_request_origin(
                    self.headers, controller.cors_origins, require_origin=require_origin)

            def _send(self, status, body=b"", headers=None, *, preflight=False):
                self.close_connection = True
                self.send_response(status)
                selected = dict(headers or {})
                selected.update(cors_response_headers(self._cors_origin, preflight=preflight))
                selected.setdefault("Cache-Control", "no-store")
                for name, value in selected.items():
                    self.send_header(name, value)
                if status not in {204, 304} and not 100 <= status < 200:
                    self.send_header("Content-Length", str(len(body)))
                self.send_header("Connection", "close")
                self.end_headers()
                if body:
                    self.wfile.write(body)

            def _error(self, status, code, message=None):
                if not self._body_consumed:
                    discard_request_body(self)
                headers = {"Content-Type": "application/json"}
                if status == 401:
                    headers["WWW-Authenticate"] = "Bearer"
                if status == 429:
                    headers["Retry-After"] = "1"
                body = json.dumps({"error": {"code": code, "message": message or code}}).encode("utf-8")
                try:
                    self._send(status, body, headers)
                except OSError:
                    self.close_connection = True

            def do_OPTIONS(self):
                try:
                    self._policy(require_origin=True)
                    if self.path != controller.path:
                        raise HttpTransportError(404, "not_found")
                    validate_preflight(self.headers)
                    if request_body_length(self.headers):
                        raise HttpTransportError(400, "body_not_allowed")
                    self._send(204, preflight=True)
                except HttpTransportError as error:
                    self._error(error.status, error.code)
                except OSError:
                    self.close_connection = True

            def do_GET(self):
                self._handle()

            def do_DELETE(self):
                self._handle()

            def do_POST(self):
                self._handle()

            def _disconnected(self):
                try:
                    ready, _, _ = select.select([self.connection], [], [], 0)
                    return bool(ready) and not self.connection.recv(1, socket.MSG_PEEK)
                except OSError:
                    return True

            def _read_body(self, length):
                deadline = time.monotonic() + 5
                data = bytearray()
                while len(data) < length:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise HttpTransportError(408, "request_timeout")
                    self.connection.settimeout(remaining)
                    chunk = self.rfile.read1(min(65536, length - len(data)))
                    if not chunk:
                        raise HttpTransportError(400, "incomplete_body")
                    data.extend(chunk)
                self._body_consumed = True
                return bytes(data)

            def _handle(self):
                try:
                    self._policy()
                    length = request_body_length(self.headers)
                    if self.path == "/health" and self.command == "GET":
                        if length:
                            raise HttpTransportError(400, "body_not_allowed")
                        values = self.headers.get_all("Authorization", [])
                        expected = f"Bearer {controller._auth_token}"
                        if len(values) != 1 or not hmac.compare_digest(values[0].encode("utf-8"), expected.encode("utf-8")):
                            raise HttpTransportError(401, "unauthorized")
                        self._send(200, json.dumps({"status": "ok", "server": "QCopilots MCP Server Web Search",
                                                  "tools": [], "transport": "mcp-relay"}).encode(),
                                   {"Content-Type": "application/json"})
                        return
                    if self.path != controller.path:
                        raise HttpTransportError(404, "not_found")
                    if length > MAX_HTTP_REQUEST_BODY_BYTES:
                        raise HttpTransportError(413, "request_too_large")
                    if self.command != "POST" and length:
                        raise HttpTransportError(400, "body_not_allowed")
                    forwarded = {}
                    for name in REQUEST_HEADERS:
                        values = self.headers.get_all(name, [])
                        limit = 8199 if name == "authorization" else 8192
                        if (len(values) > 1 or (values and (len(values[0]) > limit
                                or any(ord(ch) < 32 or ord(ch) > 126 for ch in values[0])))):
                            raise HttpTransportError(400, "invalid_header")
                        if values:
                            forwarded[name] = values[0]
                    authorization = forwarded.get("authorization", "")
                    token = authorization[7:] if authorization.startswith("Bearer ") else ""
                    if (not token or any(not 33 <= ord(ch) <= 126 for ch in token)
                            or hmac.compare_digest(token, controller._auth_token)):
                        raise HttpTransportError(401, "unauthorized")
                    body = self._read_body(length)
                    # Only classify for scheduling. The wire bytes and all MCP
                    # state remain owned by the remote server and client.
                    maintenance = self.command != "POST"
                    if self.command == "POST":
                        try:
                            message = json.loads(body)
                            maintenance = (isinstance(message, dict) and "id" not in message
                                and message.get("method") in {"notifications/initialized", "notifications/cancelled"})
                        except (ValueError, UnicodeError, RecursionError):
                            pass
                    result = controller.client.exchange(self.command, forwarded, body,
                        disconnected=self._disconnected, maintenance=maintenance)
                    self._send(result.status, result.body, result.headers)
                except HttpTransportError as error:
                    self._error(error.status, error.code)
                except BackendError as error:
                    if error.code != "cancelled":
                        self._error(error.status, error.code, error.safe_message)
                except (OSError, ValueError):
                    self.close_connection = True

        self._httpd = _BoundedHttpServer((self.host, self.port), Handler)
        self.port = self._httpd.server_address[1]
        return self._httpd

    def serve_forever(self):
        httpd = self.create_http_server()
        with self._state_lock:
            if self._closed:
                httpd.server_close()
                return
            self._serving_thread = threading.get_ident()
        self.logger.info("Serving WebSearch MCP relay on http://%s:%s/mcp", self.host, self.port)
        try:
            httpd.serve_forever()
        finally:
            with self._state_lock:
                self._serving_thread = None
            self.close()

    def close(self):
        with self._state_lock:
            self._closed = True
            serving_thread = self._serving_thread
        self.client.close()
        if self._httpd is not None:
            # shutdown must run outside serve_forever's own thread. In its
            # finally block the loop has already stopped and only close remains.
            if serving_thread is not None and serving_thread != threading.get_ident():
                self._httpd.shutdown()
            self._httpd.server_close()
