# SPDX-License-Identifier: GPL-2.0-or-later
"""Offline MCP relay transport, credential isolation and lifecycle tests."""

import base64
import http.client
import json
import os
import ssl
import sys
import threading
import time
import unittest
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from unittest import mock

PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))
from qcopilots_mcp_server_web_search import backend_client
from qcopilots_mcp_server_web_search.backend_client import BackendClient, BackendError
from qcopilots_mcp_server_web_search.config_store import BackendConfig, ConfigError
from qcopilots_mcp_server_web_search.relay_server import WebSearchRelayServer


_BUSINESS_HEADERS = {
    "Authorization": "Bearer business-secret",
    "Accept": "application/json, text/event-stream",
    "Content-Type": "application/json",
}
_INITIALIZE = b'{ "jsonrpc": "2.0", "id": 7, "method": "initialize", "params": {"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"test","version":"1"}} }'
_ICON = {
    "src": "data:image/svg+xml;base64," + base64.b64encode(
        b'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><circle cx="8" cy="8" r="6"/></svg>'
    ).decode("ascii"),
    "mimeType": "image/svg+xml",
    "sizes": ["any"],
}
# Keep noncanonical whitespace so byte equality also catches response rewriting.
_RESULT = (
    b'{ "jsonrpc": "2.0", "id": 7, "result": { "protocolVersion": "2025-06-18", "capabilities": {"tools":{}}, "serverInfo":{"name":"upstream","version":"1","icons":'
    + json.dumps([_ICON], separators=(",", ":")).encode("ascii")
    + b'} } }'
)


class _UpstreamHandler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_GET(self):
        self.do_POST()

    def do_DELETE(self):
        self.do_POST()

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        self.server.records.append((self.command, self.path, dict(self.headers.items()), body))
        try:
            message = json.loads(body) if body else {}
        except ValueError:
            message = {}
        method = message.get("method") if isinstance(message, dict) else None
        mode = self.server.mode
        status, data, content_type = 200, _RESULT, "application/json"
        if self.headers.get("Authorization") not in {"Bearer business-secret", "Bearer replacement-secret"}:
            status, data = 401, b'{"error":{"code":"unauthorized"}}'
        elif self.command == "GET":
            status, data = 405, b""
        elif self.command == "DELETE":
            status, data = 204, b""
        elif method and method.startswith("notifications/"):
            status, data = 202, b""
            if method == "notifications/cancelled":
                self.server.release.set()
        elif mode == "unauthorized":
            status, data = 401, b'{ "error": {"code":"unauthorized","retryable":false} }'
        elif mode == "error":
            status, data = 429, b'{ "error": {"code":"rate_limited","retry_after_seconds":75} }'
        elif mode == "empty_error":
            status, data = 503, b""
        elif mode == "large":
            data = json.dumps({"padding": "x" * 2048}).encode()
        elif mode == "redirect":
            status = 302
        elif mode == "html":
            content_type, data = "text/html", b"<html>upstream diagnostic</html>"
        elif mode == "sse":
            content_type, data = "text/event-stream", b"data: {}\n\n"
        self.send_response(status)
        if data:
            self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data) + (1 if mode == "short" else 0)))
        self.send_header("Connection", "close")
        self.send_header("Set-Cookie", "upstream-cookie=private")
        if status == 200:
            self.send_header("Mcp-Session-Id", "opaque-upstream-session")
            self.send_header("MCP-Protocol-Version", "2025-06-18")
        if status == 405:
            self.send_header("Allow", "POST, DELETE")
        if status == 429:
            self.send_header("Retry-After", "75")
        if mode == "redirect":
            self.send_header("Location", "/other")
        self.end_headers()
        try:
            if mode == "slow" and status == 200:
                self.wfile.write(data[:1])
                self.wfile.flush()
                self.server.started.set()
                self.server.release.wait(5)
                self.wfile.write(data[1:])
            else:
                self.wfile.write(data)
        except OSError:
            pass


@contextmanager
def _upstream():
    server = ThreadingHTTPServer(("127.0.0.1", 0), _UpstreamHandler)
    server.mode, server.records = "ok", []
    server.started, server.release = threading.Event(), threading.Event()
    server.url = f"http://127.0.0.1:{server.server_address[1]}/mcp"
    thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.02}, daemon=True)
    thread.start()
    try:
        yield server
    finally:
        server.release.set()
        server.shutdown()
        server.server_close()
        thread.join(5)
        assert not thread.is_alive()


@contextmanager
def _client(config):
    client = BackendClient(lambda: config)
    try:
        yield client
    finally:
        client.close()


@contextmanager
def _relay(client, origins=()):
    relay = WebSearchRelayServer(host="127.0.0.1", port=0, path="/mcp",
        auth_token="manager-secret", cors_origins=list(origins), client=client)
    server = relay.create_http_server()
    thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.02}, daemon=True)
    thread.start()
    try:
        yield server.server_address[1]
    finally:
        server.shutdown()
        relay.close()
        server.server_close()
        thread.join(5)
        assert not thread.is_alive()


def _request(port, method="POST", path="/mcp", headers=None, body=_INITIALIZE):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    try:
        connection.request(method, path, body=body, headers=headers or {})
        with connection.getresponse() as response:
            return response.status, dict(response.headers.items()), response.read()
    finally:
        connection.close()


def _header(headers, name):
    return next((value for key, value in headers.items() if key.lower() == name.lower()), None)


class TestQCopilotsMcpServerWebSearch(unittest.TestCase):
    def error(self, code, function, *args, **kwargs):
        with self.assertRaises(BackendError) as caught:
            function(*args, **kwargs)
        self.assertEqual(caught.exception.code, code)
        self.assertNotIn("business-secret", str(caught.exception))
        return caught.exception

    def test_transport_keeps_mcp_bytes_sessions_and_request_credentials(self):
        with _upstream() as upstream, _client(BackendConfig(upstream.url)) as client:
            with mock.patch.dict(os.environ, {
                "HTTP_PROXY": "http://127.0.0.1:1", "http_proxy": "http://127.0.0.1:1",
                "QCOPILOTS_WEBSEARCH_BACKEND_TOKEN": "must-not-be-used",
            }):
                headers = dict(_BUSINESS_HEADERS)
                headers.update({"Mcp-Session-Id": "from-client", "MCP-Protocol-Version": "2025-06-18",
                                "Last-Event-ID": "opaque-event"})
                reply = client.exchange("POST", headers, _INITIALIZE)
                self.assertEqual((reply.status, reply.body), (200, _RESULT))
                self.assertEqual(_header(reply.headers, "Mcp-Session-Id"), "opaque-upstream-session")
                self.assertEqual(_header(reply.headers, "MCP-Protocol-Version"), "2025-06-18")
                self.assertIsNone(_header(reply.headers, "Set-Cookie"))
                method, path, received, body = upstream.records[0]
                self.assertEqual((method, path, body), ("POST", "/mcp", _INITIALIZE))
                for name, value in headers.items():
                    self.assertEqual(_header(received, name), value)
                headers["Authorization"] = "Bearer replacement-secret"
                self.assertEqual(client.exchange("POST", headers, _INITIALIZE).status, 200)
                self.assertEqual(_header(upstream.records[-1][2], "Authorization"), "Bearer replacement-secret")

    def test_http_status_and_empty_notifications_are_not_reinterpreted(self):
        with _upstream() as upstream, _client(BackendConfig(upstream.url)) as client:
            cases = [("POST", b'{"jsonrpc":"2.0","method":"notifications/initialized"}', 202),
                     ("DELETE", b"", 204), ("GET", b"", 405)]
            for method, body, expected in cases:
                with self.subTest(method=method):
                    reply = client.exchange(method, _BUSINESS_HEADERS, body)
                    self.assertEqual((reply.status, reply.body), (expected, b""))
            upstream.mode = "unauthorized"
            reply = client.exchange("POST", _BUSINESS_HEADERS, _INITIALIZE)
            self.assertEqual(reply.status, 401)
            self.assertEqual(reply.body, b'{ "error": {"code":"unauthorized","retryable":false} }')
            upstream.mode = "error"
            reply = client.exchange("POST", _BUSINESS_HEADERS, _INITIALIZE)
            self.assertEqual(reply.status, 429)
            self.assertEqual(_header(reply.headers, "Retry-After"), "75")
            upstream.mode = "empty_error"
            self.assertEqual(client.exchange("POST", _BUSINESS_HEADERS, _INITIALIZE).body, b"")

    def test_relay_forwards_lifecycle_and_rejects_local_manager_credentials(self):
        with _upstream() as upstream, _client(BackendConfig(upstream.url)) as client, _relay(client) as port:
            status, headers, body = _request(port, headers=_BUSINESS_HEADERS)
            self.assertEqual((status, body), (200, _RESULT))
            self.assertEqual(json.loads(body)["result"]["serverInfo"]["icons"], [_ICON])
            self.assertEqual(_header(headers, "Mcp-Session-Id"), "opaque-upstream-session")
            session_headers = dict(_BUSINESS_HEADERS, **{
                "Mcp-Session-Id": "opaque-upstream-session", "MCP-Protocol-Version": "2025-06-18"})
            for method, payload, expected in (
                ("POST", b'{"jsonrpc":"2.0","method":"notifications/initialized"}', 202),
                ("GET", b"", 405), ("DELETE", b"", 204)):
                self.assertEqual(_request(port, method, headers=session_headers, body=payload)[0], expected)
                self.assertEqual(_header(upstream.records[-1][2], "Mcp-Session-Id"), "opaque-upstream-session")
            count = len(upstream.records)
            self.assertEqual(_request(port, headers={"Authorization": "Bearer manager-secret"})[0], 401)
            self.assertEqual(len(upstream.records), count)
            with mock.patch.dict(os.environ, {"QCOPILOTS_WEBSEARCH_BACKEND_TOKEN": "business-secret"}):
                self.assertEqual(_request(port, headers={"Content-Type": "application/json"})[0], 401)
            bad_headers = dict(_BUSINESS_HEADERS, Authorization="Bearer wrong-secret")
            self.assertEqual(_request(port, headers=bad_headers)[0], 401)
            self.assertEqual(_header(upstream.records[-1][2], "Authorization"), "Bearer wrong-secret")

    def test_health_is_independent_and_cannot_accept_business_credentials(self):
        client = mock.Mock()
        with _relay(client) as port:
            for headers, expected in (({}, 401), (_BUSINESS_HEADERS, 401),
                                      ({"Authorization": "Bearer manager-secret"}, 200)):
                self.assertEqual(_request(port, "GET", "/health", headers, b"")[0], expected)
            client.exchange.assert_not_called()

    def test_ambiguous_credentials_and_request_framing_do_not_reach_upstream(self):
        with _upstream() as upstream, _client(BackendConfig(upstream.url)) as client, _relay(client) as port:
            for authorization in ("Basic business-secret", "Bearer ", "Bearer two words"):
                with self.subTest(authorization=authorization):
                    headers = dict(_BUSINESS_HEADERS, Authorization=authorization)
                    self.assertEqual(_request(port, headers=headers)[0], 401)
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
            try:
                connection.putrequest("POST", "/mcp")
                connection.putheader("Authorization", "Bearer business-secret")
                connection.putheader("Authorization", "Bearer replacement-secret")
                connection.putheader("Content-Type", "application/json")
                connection.putheader("Content-Length", str(len(_INITIALIZE)))
                connection.endheaders(_INITIALIZE)
                with connection.getresponse() as response:
                    self.assertEqual(response.status, 400)
                    response.read()
            finally:
                connection.close()
            oversized = dict(_BUSINESS_HEADERS, **{"Content-Length": str(1024 * 1024 + 1)})
            self.assertEqual(_request(port, headers=oversized, body=b"")[0], 413)
            self.assertFalse(upstream.records)

    def test_origin_preflight_host_and_header_boundaries(self):
        origin = "http://127.0.0.1:8282"
        with _upstream() as upstream, _client(BackendConfig(upstream.url)) as client, _relay(client, [origin]) as port:
            status, headers, body = _request(port, "OPTIONS", headers={
                "Origin": origin, "Access-Control-Request-Method": "POST",
                "Access-Control-Request-Headers": "authorization,content-type,mcp-session-id,mcp-protocol-version",
            }, body=b"")
            self.assertEqual(status, 204)
            self.assertEqual(_header(headers, "Access-Control-Allow-Origin"), origin)
            self.assertFalse(upstream.records)
            valid = dict(_BUSINESS_HEADERS, Origin=origin, Cookie="private-cookie",
                         **{"X-Extra": "private-header", "User-Agent": "untrusted-user-agent"})
            status, headers, body = _request(port, headers=valid)
            self.assertEqual(status, 200)
            self.assertEqual(_header(headers, "Access-Control-Allow-Origin"), origin)
            self.assertIn("mcp-session-id", _header(headers, "Access-Control-Expose-Headers").lower())
            received = upstream.records[-1][2]
            for name in ("Origin", "Cookie", "X-Extra"):
                self.assertIsNone(_header(received, name))
            self.assertEqual(_header(received, "User-Agent"), "QCopilots-WebSearch-Relay/0.3.0")
            count = len(upstream.records)
            for extra in ({"Origin": "https://untrusted.invalid"}, {"Host": "untrusted.invalid"}):
                self.assertEqual(_request(port, headers=dict(_BUSINESS_HEADERS, **extra))[0], 403)
            self.assertEqual(len(upstream.records), count)
            self.assertEqual(_request(port, path="/mcp?url=https://other.invalid", headers=_BUSINESS_HEADERS)[0], 404)
            self.assertEqual(len(upstream.records), count)

    def test_bounded_json_only_transport_rejects_sse_html_redirects_and_large_responses(self):
        with _upstream() as upstream, _client(BackendConfig(upstream.url, max_response_bytes=1024)) as client:
            for mode in ("sse", "html", "redirect", "short", "large"):
                upstream.mode = mode
                before = len(upstream.records)
                with self.subTest(mode=mode), self.assertRaises(BackendError) as caught:
                    client.exchange("POST", _BUSINESS_HEADERS, _INITIALIZE)
                self.assertEqual(caught.exception.status, 502)
                self.assertEqual(len(upstream.records), before + 1)
                self.assertNotIn("upstream diagnostic", caught.exception.safe_message)

    def test_relay_maps_unsupported_upstream_to_safe_502(self):
        with _upstream() as upstream, _client(BackendConfig(upstream.url)) as client, _relay(client) as port:
            for mode in ("sse", "html", "redirect"):
                upstream.mode = mode
                with self.subTest(mode=mode):
                    status, headers, body = _request(port, headers=_BUSINESS_HEADERS)
                    self.assertEqual(status, 502)
                    self.assertEqual(_header(headers, "Content-Type").split(";")[0], "application/json")
                    self.assertNotIn(b"upstream diagnostic", body)
                    self.assertNotIn(b"business-secret", body)

    def test_deadline_and_disconnect_release_slow_response(self):
        with _upstream() as upstream:
            upstream.mode = "slow"
            with _client(BackendConfig(upstream.url, timeout_seconds=0.15)) as client:
                start = time.monotonic()
                error = self.error("timeout", client.exchange, "POST", _BUSINESS_HEADERS, _INITIALIZE)
                self.assertEqual(error.status, 504)
                self.assertLess(time.monotonic() - start, 2)
                self.assertTrue(upstream.started.is_set())
            upstream.started.clear()
            disconnected = threading.Event()
            with _client(BackendConfig(upstream.url, timeout_seconds=3)) as client, ThreadPoolExecutor(max_workers=1) as pool:
                future = pool.submit(client.exchange, "POST", _BUSINESS_HEADERS, _INITIALIZE,
                                     disconnected=disconnected.is_set)
                try:
                    self.assertTrue(upstream.started.wait(2))
                    disconnected.set()
                    self.error("cancelled", future.result, 2)
                finally:
                    upstream.release.set()

    def test_busy_requests_leave_cancellation_and_health_capacity(self):
        with _upstream() as upstream, _client(BackendConfig(upstream.url, max_concurrency=1)) as client, _relay(client) as port:
            upstream.mode = "slow"
            with ThreadPoolExecutor(max_workers=1) as pool:
                future = pool.submit(_request, port, headers=_BUSINESS_HEADERS)
                try:
                    self.assertTrue(upstream.started.wait(2))
                    error = self.error("busy", client.exchange, "POST", _BUSINESS_HEADERS, _INITIALIZE)
                    self.assertEqual(error.status, 429)
                    self.assertEqual(_request(port, "GET", "/health", {"Authorization": "Bearer manager-secret"}, b"")[0], 200)
                    reply = _request(port, headers=_BUSINESS_HEADERS,
                        body=b'{"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":7}}')
                    self.assertEqual(reply[0], 202)
                    self.assertEqual(future.result(2)[0], 200)
                finally:
                    upstream.release.set()

    def test_timed_out_dns_retains_capacity_until_worker_finishes(self):
        entered, release = threading.Event(), threading.Event()
        def connect(*_args, **_kwargs):
            entered.set()
            release.wait(5)
            raise OSError("private DNS detail")
        with _client(BackendConfig("http://127.0.0.1:1/mcp", timeout_seconds=0.1, max_concurrency=1)) as client:
            with mock.patch.object(backend_client.socket, "create_connection", connect):
                try:
                    self.error("timeout", client.exchange, "POST", _BUSINESS_HEADERS, _INITIALIZE)
                    self.assertTrue(entered.is_set())
                    self.error("busy", client.exchange, "POST", _BUSINESS_HEADERS, _INITIALIZE)
                finally:
                    release.set()

    def test_tls_validation_and_safe_configuration_errors(self):
        with _upstream() as upstream:
            context = mock.Mock()
            context.wrap_socket.side_effect = ssl.SSLCertVerificationError("private certificate detail")
            with _client(BackendConfig(upstream.url.replace("http:", "https:"))) as client:
                with mock.patch.object(backend_client.ssl, "create_default_context", return_value=context) as factory:
                    error = self.error("backend_unavailable", client.exchange, "POST", _BUSINESS_HEADERS, _INITIALIZE)
                    self.assertNotIn("private certificate detail", error.safe_message)
                    factory.assert_called_once()
                    context.wrap_socket.assert_called_once()
        client = BackendClient(mock.Mock(side_effect=ConfigError("private config detail")))
        self.addCleanup(client.close)
        error = self.error("invalid_configuration", client.exchange, "POST", _BUSINESS_HEADERS, _INITIALIZE)
        self.assertNotIn("private config detail", error.safe_message)

    def test_close_stops_controller_serving_from_another_thread(self):
        controller = WebSearchRelayServer(auth_token="manager-secret", client=mock.Mock())
        failures = []
        def run():
            try:
                controller.serve_forever()
            except Exception as error:
                failures.append(error)
        thread = threading.Thread(target=run, daemon=True)
        thread.start()
        try:
            deadline = time.monotonic() + 2
            while not controller.port and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertEqual(_request(controller.port, "GET", "/health",
                {"Authorization": "Bearer manager-secret"}, b"")[0], 200)
        finally:
            controller.close()
            thread.join(2)
        self.assertFalse(thread.is_alive())
        self.assertEqual(failures, [])


if __name__ == "__main__":
    unittest.main()
