from __future__ import annotations

import builtins
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

from processingmcpserver.plugin import ProcessingMCPServerPlugin

from ._shared_case_base import ProcessingMCPTestBase


class PluginRuntimeTest(ProcessingMCPTestBase):
    @patch("processingmcpserver.plugin.QgsMessageLog.logMessage")
    def test_initgui_logs_unavailable_when_runtime_import_fails(self, mock_log_message):
        """
        作用：执行测试用例 `initgui logs unavailable when runtime import fails`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `initgui logs unavailable when runtime import fails`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_log_message`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        plugin = ProcessingMCPServerPlugin(iface="iface")
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
            if name == "processingmcpserver.server":
                raise ImportError("server import boom")
            return original_import(name, globals, locals, fromlist, level)

        with patch("builtins.__import__", side_effect=fake_import):
            plugin.initGui()

        self.assertTrue(
            any(
                "Processing MCP server is unavailable" in call.args[0]
                for call in mock_log_message.call_args_list
            )
        )

    def test_initgui_starts_server_after_dependency_check(self):
        """
        作用：执行测试用例 `initgui starts server after dependency check`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `initgui starts server after dependency check`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        plugin = ProcessingMCPServerPlugin(iface="iface")
        config = self._build_config("streamable-http")
        server_instance = MagicMock()
        server_instance.start.return_value = True

        with (
            patch(
                "processingmcpserver.config.load_processing_mcp_server_config",
                return_value=config,
            ),
            patch(
                "processingmcpserver.dependency_manager.ensure_processing_mcp_dependencies",
                return_value=SimpleNamespace(success=True),
            ),
            patch(
                "processingmcpserver.server.ProcessingMCPServer",
                return_value=server_instance,
            ) as mock_server_class,
        ):
            plugin.initGui()

        mock_server_class.assert_called_once_with("iface", config=config)
        server_instance.start.assert_called_once_with()
        self.assertIs(plugin._server, server_instance)

    def test_initgui_does_not_store_server_when_start_returns_false(self):
        """
        作用：执行测试用例 `initgui does not store server when start returns false`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `initgui does not store server when start returns false`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        plugin = ProcessingMCPServerPlugin(iface="iface")
        config = self._build_config("streamable-http")
        server_instance = MagicMock()
        server_instance.start.return_value = False

        with (
            patch(
                "processingmcpserver.config.load_processing_mcp_server_config",
                return_value=config,
            ),
            patch(
                "processingmcpserver.dependency_manager.ensure_processing_mcp_dependencies",
                return_value=SimpleNamespace(success=True),
            ),
            patch(
                "processingmcpserver.server.ProcessingMCPServer",
                return_value=server_instance,
            ),
        ):
            plugin.initGui()

        server_instance.start.assert_called_once_with()
        self.assertIsNone(plugin._server)

    @patch("processingmcpserver.plugin.QgsMessageLog.logMessage")
    def test_initgui_skips_server_on_dependency_failure(self, mock_log_message):
        """
        作用：执行测试用例 `initgui skips server on dependency failure`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `initgui skips server on dependency failure`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_log_message`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        plugin = ProcessingMCPServerPlugin(iface="iface")
        config = self._build_config("streamable-http")

        with (
            patch(
                "processingmcpserver.config.load_processing_mcp_server_config",
                return_value=config,
            ),
            patch(
                "processingmcpserver.dependency_manager.ensure_processing_mcp_dependencies",
                return_value=SimpleNamespace(success=False),
            ),
            patch("processingmcpserver.server.ProcessingMCPServer") as mock_server_class,
        ):
            plugin.initGui()

        mock_server_class.assert_not_called()
        self.assertIsNone(plugin._server)
        self.assertTrue(
            any(
                "startup skipped due to dependency check failure" in call.args[0]
                for call in mock_log_message.call_args_list
            )
        )

    @patch("processingmcpserver.plugin.QgsMessageLog.logMessage")
    def test_unload_stops_server_and_clears_handle(self, mock_log_message):
        """
        作用：执行测试用例 `unload stops server and clears handle`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `unload stops server and clears handle`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_log_message`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        plugin = ProcessingMCPServerPlugin(iface="iface")
        server_instance = MagicMock()
        plugin._server = server_instance

        plugin.unload()

        server_instance.stop.assert_called_once_with()
        self.assertIsNone(plugin._server)
        self.assertEqual(mock_log_message.call_count, 0)
