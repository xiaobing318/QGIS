from __future__ import annotations

import builtins
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

from processingmcpserver.plugin import ProcessingMCPServerPlugin

from ._shared_case_base import ProcessingMCPTestBase


class PluginRuntimeTest(ProcessingMCPTestBase):
    @patch("processingmcpserver.plugin.QgsMessageLog.logMessage")
    def test_initgui_logs_unavailable_when_runtime_import_fails(self, mock_log_message):
        """验证 initgui logs unavailable when runtime import fails 场景。"""
        plugin = ProcessingMCPServerPlugin(iface="iface")
        original_import = builtins.__import__

        def fake_import(name, globals=None, locals=None, fromlist=(), level=0):
            """提供用于测试的导入替身函数。"""
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
        """验证 initgui starts server after dependency check 场景。"""
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
        """验证 initgui does not store server when start returns false 场景。"""
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
        """验证 initgui skips server on dependency failure 场景。"""
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
        """验证 unload stops server and clears handle 场景。"""
        plugin = ProcessingMCPServerPlugin(iface="iface")
        server_instance = MagicMock()
        plugin._server = server_instance

        plugin.unload()

        server_instance.stop.assert_called_once_with()
        self.assertIsNone(plugin._server)
        self.assertEqual(mock_log_message.call_count, 0)
