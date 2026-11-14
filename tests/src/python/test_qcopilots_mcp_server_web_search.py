# SPDX-License-Identifier: GPL-2.0-or-later
"""Offline WebSearch adapter contract, transport and lifecycle tests."""
import json
import os
import ssl
import sys
import tempfile
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
from qcopilots_common.mcp_http import McpJsonRpcServer
from qcopilots_mcp_server_web_search import backend_client
from qcopilots_mcp_server_web_search.backend_client import BackendClient, BackendError
from qcopilots_mcp_server_web_search.config_store import BackendConfig, ConfigError, load_config
from qcopilots_mcp_server_web_search.tools import build_web_search_tools


class _Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_GET(self):
        self.do_POST()

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        self.server.records.append((self.command, self.path, self.headers.get("Authorization"), body))
        mode = self.server.mode
        value = {"contract_version": 1, "service": "ready"}
        status, content_type = 200, "application/json"
        if mode == "unauthorized":
            status, value = 401, {"error": {"code": "unauthorized", "message": "backend-secret"}}
        elif mode == "error":
            status, value = 502, {"error": self.server.error}
        elif mode == "bool_version":
            value["contract_version"] = True
        elif mode == "missing_version":
            value.pop("contract_version")
        elif mode == "large":
            value["padding"] = "x" * 2048
        elif mode == "redirect":
            status = 302
        elif mode == "wrong_type":
            content_type = "text/html"
        data = b"{" if mode == "invalid_json" else json.dumps(value).encode()
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data) + (1 if mode == "short" else 0)))
        self.send_header("Connection", "close")
        if mode == "redirect":
            self.send_header("Location", "/v1/status")
        self.end_headers()
        try:
            if mode == "slow":
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
def _backend():
    server = ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
    server.mode, server.records = "ok", []
    server.started, server.release = threading.Event(), threading.Event()
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    server.url = f"http://127.0.0.1:{server.server_address[1]}"
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


class TestQCopilotsMcpServerWebSearch(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="qcopilots-websearch-")
        self.addCleanup(temp.cleanup)
        self.root, self.path = Path(temp.name), Path(temp.name) / "config.json"
        self.value = {"backend_url": "http://127.0.0.1:8088", "auth": {"token_env": "QCOPILOTS_WEBSEARCH_BACKEND_TOKEN"}}
        patch = mock.patch.dict(os.environ, {"QCOPILOTS_SECURITY_POLICY": "", "QCOPILOTS_MCP_AUTH_TOKEN": "local-secret", "QCOPILOTS_WEBSEARCH_BACKEND_TOKEN": "backend-secret"})
        patch.start()
        self.addCleanup(patch.stop)

    def config(self):
        self.path.write_text(json.dumps(self.value), encoding="utf-8")
        return load_config(self.path)

    def error(self, code, function, *args):
        with self.assertRaises(BackendError) as caught:
            function(*args)
        self.assertEqual(caught.exception.code, code)
        self.assertNotIn("backend-secret", str(caught.exception))
        return caught.exception

    def test_config_tokens_and_policy(self):
        self.assertEqual(self.config().token, "backend-secret")
        self.assertNotIn("backend-secret", repr(self.config()))
        token = self.root / "token"
        token.write_text("backend-secret\n")
        token.chmod(0o600)
        self.value["auth"] = {"token_file": "token"}
        self.assertEqual(self.config().token, "backend-secret")
        self.value["auth"]["token_env"] = "QCOPILOTS_WEBSEARCH_BACKEND_TOKEN"
        with self.assertRaises(ConfigError):
            self.config()
        self.value["auth"] = {"token_env": "QCOPILOTS_MCP_AUTH_TOKEN"}
        with self.assertRaises(ConfigError):
            self.config()
        self.value["auth"] = {"token_env": "QCOPILOTS_WEBSEARCH_BACKEND_TOKEN"}
        self.value["backend_url"] = "https://backend.example.invalid"
        self.assertEqual(self.config().backend_url, self.value["backend_url"])
        policy = {"mode": "formal_restricted", "shell": {"enabled": False, "executables": []},
                  "network": {"enabled": True, "allowed_origins": ["http://127.0.0.1:8088"]}}
        with mock.patch.dict(os.environ, {"QCOPILOTS_SECURITY_POLICY": json.dumps(policy)}):
            with self.assertRaises(PermissionError):
                self.config()
            self.value["backend_url"] = "http://127.0.0.1:8088"
            self.assertEqual(self.config().backend_url, self.value["backend_url"])

    def test_rejects_unsafe_configuration(self):
        for url in ("http://backend.example.invalid", "https://user:pass@example.invalid", "https://example.invalid/path", "https://example.invalid/?token=x", "https://example.invalid/#fragment"):
            with self.subTest(url=url):
                self.value["backend_url"] = url
                with self.assertRaises(ConfigError):
                    self.config()

    def test_discovery_offline_and_safe_errors(self):
        loader = mock.Mock(side_effect=ConfigError("sensitive detail"))
        client = BackendClient(loader)
        self.addCleanup(client.close)
        rpc = McpJsonRpcServer("test", "1", build_web_search_tools(client))
        listed = rpc.handle_json_rpc({"jsonrpc": "2.0", "id": 1, "method": "tools/list"})
        self.assertEqual(len(listed["result"]["tools"]), 4)
        loader.assert_not_called()
        reply = rpc.handle_json_rpc({"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "web_search", "arguments": {"query": "QGIS"}}})
        self.assertTrue(reply["result"]["isError"])
        self.assertEqual(reply["result"]["structuredContent"]["error"]["code"], "invalid_configuration")
        self.assertNotIn("sensitive detail", json.dumps(reply))

    def test_find_pagination_and_open_exclusivity(self):
        client = mock.Mock()
        client.call.return_value = {"contract_version": 1}
        tools = build_web_search_tools(client)
        arguments = {"page_id": "page", "pattern": "地图", "start_line": 17}
        next(t for t in tools if t.name == "web_find").handler(arguments)
        client.call.assert_called_once_with("find", arguments)
        rpc = McpJsonRpcServer("test", "1", tools)
        reply = rpc.handle_json_rpc({"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "web_open", "arguments": {"url": "https://example.invalid", "page_id": "page"}}})
        self.assertEqual(reply["error"]["code"], -32602)

    def test_transport_routes_auth_no_proxy_and_bad_responses(self):
        with _backend() as backend:
            config = BackendConfig(backend.url, "backend-secret", max_response_bytes=128)
            with _client(config) as client, mock.patch.dict(os.environ, {"HTTP_PROXY": "http://127.0.0.1:1", "http_proxy": "http://127.0.0.1:1"}):
                self.assertEqual(client.call("search", {"query": "地图"})["contract_version"], 1)
                self.assertEqual(client.call("status", {})["service"], "ready")
                self.assertEqual(backend.records[0][:3], ("POST", "/v1/search", "Bearer backend-secret"))
                self.assertEqual(json.loads(backend.records[0][3]), {"query": "地图"})
                self.assertEqual(backend.records[1][:2], ("GET", "/v1/status"))
                for mode, code in {"redirect": "backend_unavailable", "invalid_json": "backend_unavailable", "bool_version": "backend_unavailable", "missing_version": "backend_unavailable", "wrong_type": "backend_unavailable", "large": "response_too_large", "short": "backend_unavailable", "unauthorized": "unauthorized"}.items():
                    backend.mode = mode
                    before = len(backend.records)
                    with self.subTest(mode=mode):
                        self.error(code, client.call, "search", {"query": "test"})
                        self.assertEqual(len(backend.records), before + 1)

    def test_preserves_backend_retryable_with_safe_fallback(self):
        with _backend() as backend:
            backend.mode = "error"
            with _client(BackendConfig(backend.url, "backend-secret")) as client:
                for code in ("upstream_http_error", "provider_unavailable"):
                    for retryable in (True, False):
                        with self.subTest(code=code, retryable=retryable):
                            backend.error = {"code": code, "message": "backend-secret", "retryable": retryable}
                            error = self.error(code, client.call, "open", {"url": "https://example.invalid"})
                            self.assertIs(error.retryable, retryable)
                for invalid in (None, 0, 1, "true", [], {}):
                    backend.error = {"code": "upstream_http_error", "retryable": invalid}
                    self.assertIs(self.error("upstream_http_error", client.call, "open", {}).retryable, False)
                backend.error = {"code": "unknown", "retryable": False}
                self.assertIs(self.error("backend_unavailable", client.call, "open", {}).retryable, True)

    def test_deadline_closes_slow_response(self):
        with _backend() as backend:
            backend.mode = "slow"
            with _client(BackendConfig(backend.url, "backend-secret", timeout_seconds=0.15)) as client:
                start = time.monotonic()
                self.error("timeout", client.call, "search", {"query": "test"})
                self.assertLess(time.monotonic() - start, 1.5)
                self.assertTrue(backend.started.is_set())

    def test_capacity_and_independent_status(self):
        entered, release = threading.Barrier(6), threading.Event()
        class BlockingClient(BackendClient):
            def _exchange(self, job, config, operation, body):
                if operation != "status":
                    entered.wait(5)
                    release.wait(5)
                job.remaining()
                return {"contract_version": 1}
        client = BlockingClient(lambda: BackendConfig("http://127.0.0.1:1", "secret"))
        self.addCleanup(client.close)
        with ThreadPoolExecutor(max_workers=5) as pool:
            futures = [pool.submit(client.call, "search", {"query": str(i)}) for i in range(5)]
            try:
                entered.wait(5)
                self.error("busy", client.call, "search", {"query": "sixth"})
                self.assertEqual(client.call("status", {})["contract_version"], 1)
            finally:
                release.set()
            for future in futures:
                self.assertEqual(future.result(5)["contract_version"], 1)

    def test_timed_out_dns_retains_capacity(self):
        entered, release = threading.Event(), threading.Event()
        def connect(*_args, **_kwargs):
            entered.set()
            release.wait(5)
            raise OSError("fake DNS failure")
        client = BackendClient(lambda: BackendConfig("http://127.0.0.1:1", "secret", timeout_seconds=0.05, max_concurrency=1))
        with mock.patch.object(backend_client.socket, "create_connection", connect):
            try:
                self.error("timeout", client.call, "search", {"query": "first"})
                self.assertTrue(entered.is_set())
                self.error("busy", client.call, "search", {"query": "second"})
            finally:
                release.set()
                client.close()
        with client._lock:
            self.assertFalse(client._active)

    def test_tls_failure_is_not_disabled_or_exposed(self):
        with _backend() as backend:
            config = BackendConfig(backend.url.replace("http:", "https:"), "backend-secret")
            context = mock.Mock()
            context.wrap_socket.side_effect = ssl.SSLCertVerificationError("private detail")
            with _client(config) as client, mock.patch.object(backend_client.ssl, "create_default_context", return_value=context):
                error = self.error("backend_unavailable", client.call, "search", {"query": "test"})
                self.assertNotIn("private detail", str(error))
                context.wrap_socket.assert_called_once()


if __name__ == "__main__":
    unittest.main()
