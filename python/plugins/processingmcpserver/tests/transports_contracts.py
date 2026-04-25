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
        """
        作用：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
        """
        作用：实现 `streamable_http_app` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `streamable_http_app` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return {"app": "streamable-http"}

    def sse_app(self, _mount_path):
        """
        作用：实现 `sse_app` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `sse_app` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `_mount_path`：路径类参数，用于定位输入或输出文件系统位置。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return {"app": "sse"}


class _DummyTransport(BaseMcpTransport):
    def _run(self) -> None:
        """
        作用：封装内部辅助步骤 `_run`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_run`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        return None


class TransportsContractsTest(ProcessingMCPTestBase):
    def test_transport_fallback_to_streamable_http(self):
        """
        作用：执行测试用例 `transport fallback to streamable http`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `transport fallback to streamable http`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        transport = create_transport(object(), self._build_config("invalid-transport"))
        self.assertIsInstance(transport, StreamableHttpTransport)

    def test_transport_selects_sse_and_stdio(self):
        """
        作用：执行测试用例 `transport selects sse and stdio`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `transport selects sse and stdio`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.assertIsInstance(
            create_transport(object(), self._build_config("sse")), SseTransport
        )
        self.assertIsInstance(
            create_transport(object(), self._build_config("stdio")), StdioTransport
        )

    def test_apply_cors_returns_original_app_when_disabled(self):
        """
        作用：执行测试用例 `apply cors returns original app when disabled`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `apply cors returns original app when disabled`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        transport = StreamableHttpTransport(
            _FakeMcp(), self._build_config("streamable-http")
        )
        app = object()
        self.assertIs(transport._apply_cors(app), app)

    def test_apply_cors_wraps_app_when_enabled(self):
        """
        作用：执行测试用例 `apply cors wraps app when enabled`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `apply cors wraps app when enabled`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
        """
        作用：执行测试用例 `apply cors returns original app when import fails`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `apply cors returns original app when import fails`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        config = replace(
            self._build_config("streamable-http"),
            cors_origins=["http://localhost:8080"],
        )
        transport = StreamableHttpTransport(_FakeMcp(), config)
        app = {"app": "base"}
        original_import = builtins.__import__

        def fake_import(name, globals=None, locals=None, fromlist=(), level=0):
            """
            作用：处理 `fake_import` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            用途：处理 `fake_import` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
            参数与返回：
            - 参数 `name`：标识或模式参数，用于指定目标对象或流程分支。
            - 参数 `globals`：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
            - 参数 `locals`：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
            - 参数 `fromlist`：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `()`。
            - 参数 `level`：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `0`。
            - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
            返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
            异常：可能显式抛出 `ImportError`。
            """
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
        """
        作用：执行测试用例 `create uvicorn config retries without custom logging`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `create uvicorn config retries without custom logging`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        transport = StreamableHttpTransport(
            _FakeMcp(), self._build_config("streamable-http")
        )
        calls: list[object] = []

        def fake_config(_app, host, port, log_level, log_config):
            """
            作用：处理 `fake_config` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            用途：处理 `fake_config` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
            参数与返回：
            - 参数 `_app`：业务输入参数，由调用方提供以驱动当前函数逻辑。
            - 参数 `host`：业务输入参数，由调用方提供以驱动当前函数逻辑。
            - 参数 `port`：数值控制参数，用于限制范围、数量或时限。
            - 参数 `log_level`：标识或模式参数，用于指定目标对象或流程分支。
            - 参数 `log_config`：业务输入参数，由调用方提供以驱动当前函数逻辑。
            - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
            返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
            异常：可能显式抛出 `RuntimeError`。
            """
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

    def test_streamable_transport_exposes_endpoint_url(self):
        transport = StreamableHttpTransport(
            _FakeMcp(), self._build_config("streamable-http")
        )
        self.assertEqual(transport.endpoint_url(), "http://127.0.0.1:8000/mcp")

    def test_sse_transport_exposes_endpoint_url(self):
        transport = SseTransport(_FakeMcp(), self._build_config("sse"))
        self.assertEqual(transport.endpoint_url(), "http://127.0.0.1:8000/sse")

    @patch("processingmcpserver.transports._is_tcp_bind_available")
    @patch("processingmcpserver.transports.threading.Thread")
    def test_uvicorn_transport_port_check_failure_blocks_thread_start(
        self, mock_thread_class, mock_port_check
    ):
        mock_port_check.return_value = (False, "address already in use")
        transport = StreamableHttpTransport(
            _FakeMcp(), self._build_config("streamable-http")
        )

        with patch.object(transport, "_log") as mock_log:
            self.assertFalse(transport.start())

        mock_thread_class.assert_not_called()
        self.assertTrue(
            any(
                "Processing MCP port check failed" in call.args[0]
                for call in mock_log.call_args_list
            )
        )

    @patch("processingmcpserver.transports._find_available_port")
    @patch("processingmcpserver.transports._is_tcp_bind_available")
    @patch("processingmcpserver.transports.threading.Thread")
    def test_uvicorn_transport_port_check_uses_fallback_port(
        self, mock_thread_class, mock_port_check, mock_find_port
    ):
        fake_thread = MagicMock()
        mock_thread_class.return_value = fake_thread
        mock_port_check.return_value = (False, "address already in use")
        mock_find_port.return_value = 19000
        transport = StreamableHttpTransport(
            _FakeMcp(), self._build_config("streamable-http")
        )

        with patch.object(transport, "_log") as mock_log:
            self.assertTrue(transport.start())

        self.assertEqual(transport._mcp.settings.port, 19000)
        mock_thread_class.assert_called_once()
        self.assertTrue(
            any(
                "automatically selected fallback port 19000" in call.args[0]
                for call in mock_log.call_args_list
            )
        )

    @patch("processingmcpserver.transports.threading.Thread")
    def test_base_transport_start_reentry_returns_true(self, mock_thread_class):
        """
        作用：执行测试用例 `base transport start reentry returns true`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `base transport start reentry returns true`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_thread_class`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        fake_thread = MagicMock()
        fake_thread.is_alive.return_value = True
        mock_thread_class.return_value = fake_thread
        transport = _DummyTransport(_FakeMcp(), self._build_config("streamable-http"))

        self.assertTrue(transport.start())
        self.assertTrue(transport.start())

        self.assertEqual(mock_thread_class.call_count, 1)
        fake_thread.start.assert_called_once_with()

    def test_base_transport_stop_without_thread_requests_stop(self):
        """
        作用：执行测试用例 `base transport stop without thread requests stop`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `base transport stop without thread requests stop`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        transport = _DummyTransport(_FakeMcp(), self._build_config("streamable-http"))

        with patch.object(transport, "_request_stop") as mock_request_stop:
            transport.stop()

        mock_request_stop.assert_called_once_with()

    @patch("processingmcpserver.transports.sys.stdout", None)
    @patch("processingmcpserver.transports.sys.stderr", None)
    def test_ensure_stdio_streams_restores_stdout_and_stderr(self):
        """
        作用：执行测试用例 `ensure stdio streams restores stdout and stderr`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `ensure stdio streams restores stdout and stderr`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
        """
        作用：执行测试用例 `stdio transport reports missing streams`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `stdio transport reports missing streams`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        transport = StdioTransport(_FakeMcp(), self._build_config("stdio"))

        with patch.object(transport, "_log") as mock_log:
            transport._run()

        self.assertTrue(
            any(
                "STDIO transport unavailable" in call.args[0]
                for call in mock_log.call_args_list
            )
        )
