from __future__ import annotations

import builtins
import io
from dataclasses import replace
from types import ModuleType, SimpleNamespace
import sys
from unittest.mock import MagicMock, patch

from processingmcpserver.transports import (
    BaseMcpTransport,
    StdioTransport,
    SseTransport,
    StreamableHttpTransport,
    create_transport,
)

from ._shared_case_base import ProcessingMCPTestBase


class _FakeMcp:
    def __init__(self):
        self.settings = SimpleNamespace(
            host="127.0.0.1",
            port=8000,
            log_level="INFO",
            mount_path="/",
            sse_path="/sse",
            message_path="/messages/",
            streamable_http_path="/mcp",
        )

    def streamable_http_app(self):
        return {"app": "streamable-http"}

    def sse_app(self, _mount_path):
        return {"app": "sse"}


class _DummyTransport(BaseMcpTransport):
    def _run(self) -> None:
        return None


class TransportsContractsTest(ProcessingMCPTestBase):
    def test_transport_fallback_to_streamable_http(self):
        transport = create_transport(object(), self._build_config("invalid-transport"))
        self.assertIsInstance(transport, StreamableHttpTransport)

    def test_transport_selects_sse_and_stdio(self):
        self.assertIsInstance(
            create_transport(object(), self._build_config("sse")), SseTransport
        )
        self.assertIsInstance(
            create_transport(object(), self._build_config("stdio")), StdioTransport
        )

    def test_apply_cors_returns_original_app_when_disabled(self):
        transport = StreamableHttpTransport(
            _FakeMcp(), self._build_config("streamable-http")
        )
        app = object()
        self.assertIs(transport._apply_cors(app), app)

    def test_apply_cors_wraps_app_when_enabled(self):
        config = replace(
            self._build_config("streamable-http"),
            cors_origins=["http://localhost:8080"],
            cors_allow_headers=["authorization"],
        )
        transport = StreamableHttpTransport(_FakeMcp(), config)
        fake_module = ModuleType("starlette.middleware.cors")
        fake_module.CORSMiddleware = lambda app, **kwargs: {"app": app, **kwargs}

        with patch.dict(sys.modules, {"starlette.middleware.cors": fake_module}):
            wrapped = transport._apply_cors({"app": "base"})

        self.assertEqual(wrapped["allow_origins"], ["http://localhost:8080"])
        self.assertEqual(wrapped["allow_headers"], ["authorization"])

    def test_apply_cors_returns_original_app_when_import_fails(self):
        config = replace(
            self._build_config("streamable-http"),
            cors_origins=["http://localhost:8080"],
        )
        transport = StreamableHttpTransport(_FakeMcp(), config)
        app = {"app": "base"}
        original_import = builtins.__import__

        def fake_import(name, globals=None, locals=None, fromlist=(), level=0):
            if name == "starlette.middleware.cors":
                raise ImportError("cors import boom")
            return original_import(name, globals, locals, fromlist, level)

        with (
            patch("builtins.__import__", side_effect=fake_import),
            patch.object(transport, "_log_exception") as mock_log_exception,
        ):
            wrapped = transport._apply_cors(app)

        self.assertIs(wrapped, app)
        mock_log_exception.assert_called_once()

    def test_create_uvicorn_config_retries_without_custom_logging(self):
        transport = StreamableHttpTransport(
            _FakeMcp(), self._build_config("streamable-http")
        )
        calls: list[object] = []

        def fake_config(_app, host, port, log_level, log_config):
            calls.append(log_config)
            self.assertEqual(host, "127.0.0.1")
            self.assertEqual(port, 8000)
            self.assertEqual(log_level, "info")
            if log_config is not None:
                raise RuntimeError("bad logging config")
            return {"host": host, "port": port, "log_config": log_config}

        config = transport._create_uvicorn_config(
            SimpleNamespace(Config=fake_config),
            app={"app": "streamable-http"},
        )

        self.assertEqual(calls[1], None)
        self.assertEqual(config["log_config"], None)

    @patch("processingmcpserver.transports.threading.Thread")
    def test_base_transport_start_reentry_returns_true(self, mock_thread_class):
        fake_thread = MagicMock()
        fake_thread.is_alive.return_value = True
        mock_thread_class.return_value = fake_thread
        transport = _DummyTransport(_FakeMcp(), self._build_config("streamable-http"))

        self.assertTrue(transport.start())
        self.assertTrue(transport.start())

        self.assertEqual(mock_thread_class.call_count, 1)
        fake_thread.start.assert_called_once_with()

    def test_base_transport_stop_without_thread_requests_stop(self):
        transport = _DummyTransport(_FakeMcp(), self._build_config("streamable-http"))

        with patch.object(transport, "_request_stop") as mock_request_stop:
            transport.stop()

        mock_request_stop.assert_called_once_with()

    @patch("processingmcpserver.transports.sys.stdout", None)
    @patch("processingmcpserver.transports.sys.stderr", None)
    def test_ensure_stdio_streams_restores_stdout_and_stderr(self):
        transport = _DummyTransport(_FakeMcp(), self._build_config("streamable-http"))
        fake_stdout = io.StringIO()
        fake_stderr = io.StringIO()

        with (
            patch.object(
                __import__("processingmcpserver.transports").transports.sys,
                "__stdout__",
                fake_stdout,
            ),
            patch.object(
                __import__("processingmcpserver.transports").transports.sys,
                "__stderr__",
                fake_stderr,
            ),
        ):
            transport._ensure_stdio_streams()

            self.assertIs(
                __import__("processingmcpserver.transports").transports.sys.stdout,
                fake_stdout,
            )
            self.assertIs(
                __import__("processingmcpserver.transports").transports.sys.stderr,
                fake_stderr,
            )

    @patch("processingmcpserver.transports.sys.stdout", None)
    @patch("processingmcpserver.transports.sys.stdin", None)
    def test_stdio_transport_reports_missing_streams(self):
        transport = StdioTransport(_FakeMcp(), self._build_config("stdio"))

        with patch.object(transport, "_log") as mock_log:
            transport._run()

        self.assertTrue(
            any(
                "STDIO transport unavailable" in call.args[0]
                for call in mock_log.call_args_list
            )
        )
