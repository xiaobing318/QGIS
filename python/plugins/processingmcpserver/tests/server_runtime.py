from __future__ import annotations

from dataclasses import replace
from types import ModuleType, SimpleNamespace
import sys
from unittest.mock import MagicMock, patch

from processingmcpserver.server import ProcessingMCPServer

from ._shared_case_base import ProcessingMCPTestBase


class _FakeTransport:
    def __init__(self, start_result: bool = True):
        """初始化 _FakeTransport 实例状态。"""
        self.start_result = start_result
        self.running = False
        self.start_calls = 0
        self.stop_calls = 0

    def start(self) -> bool:
        """启动当前对象管理的运行流程。"""
        self.start_calls += 1
        self.running = self.start_result
        return self.start_result

    def stop(self) -> None:
        """停止当前对象管理的运行流程。"""
        self.stop_calls += 1
        self.running = False

    def is_running(self) -> bool:
        """判断 running 是否成立。"""
        return self.running

    def describe(self) -> str:
        """返回当前对象的描述信息。"""
        return "fake transport"


class _FakeFastMCP:
    def __init__(self, name: str, instructions: str = "", **kwargs):
        """初始化 _FakeFastMCP 实例状态。"""
        self.name = name
        self.instructions = instructions
        self.kwargs = kwargs
        self.settings = SimpleNamespace(**kwargs)
        self._mcp_server = SimpleNamespace(version="mcp-package-version")


class _FakeFastMCPWithoutServerIdentity:
    def __init__(
        self,
        name: str,
        instructions: str = "",
        host: str = "127.0.0.1",
        port: int = 8000,
        mount_path: str = "/",
        sse_path: str = "/sse",
        message_path: str = "/messages/",
        streamable_http_path: str = "/mcp",
        stateless_http: bool = False,
        json_response: bool = False,
        log_level: str = "INFO",
    ):
        """模拟不支持 website_url 与 icons 参数的 FastMCP。"""
        self.name = name
        self.instructions = instructions
        self.settings = SimpleNamespace(
            host=host,
            port=port,
            mount_path=mount_path,
            sse_path=sse_path,
            message_path=message_path,
            streamable_http_path=streamable_http_path,
            stateless_http=stateless_http,
            json_response=json_response,
            log_level=log_level,
        )
        self._mcp_server = SimpleNamespace(version="mcp-package-version")


class ProcessingMCPServerRuntimeTest(ProcessingMCPTestBase):
    def test_build_mcp_server_registers_tools_prompts_and_resources(self):
        """验证 build MCP server registers tools prompts and resources 场景。"""
        fake_transport = _FakeTransport()
        fake_mcp_module = ModuleType("mcp.server.fastmcp")
        fake_mcp_module.FastMCP = _FakeFastMCP
        config = self._build_config("streamable-http")

        with (
            patch.dict(
                sys.modules,
                {
                    "mcp": ModuleType("mcp"),
                    "mcp.server": ModuleType("mcp.server"),
                    "mcp.server.fastmcp": fake_mcp_module,
                },
            ),
            patch(
                "processingmcpserver.server.create_transport",
                return_value=fake_transport,
            ),
            patch("processingmcpserver.server.ProcessingMCPTools") as mock_tools_class,
            patch("processingmcpserver.server.register_tools") as mock_register_tools,
            patch("processingmcpserver.server.register_prompts") as mock_register_prompts,
            patch(
                "processingmcpserver.server.register_resources"
            ) as mock_register_resources,
            patch(
                "processingmcpserver.server._load_plugin_version_from_metadata",
                return_value="9.9.9",
            ),
        ):
            server = ProcessingMCPServer(iface="iface", config=config)

        self.assertEqual(server._mcp.name, "QGIS Processing MCP Server")
        self.assertTrue(server._mcp.instructions)
        self.assertIn("能力说明", server._mcp.instructions)
        self.assertIn("使用背景", server._mcp.instructions)
        self.assertIn("推荐工作流", server._mcp.instructions)
        self.assertIn("注意事项", server._mcp.instructions)
        self.assertIn("读取当前工程与图层上下文", server._mcp.instructions)
        self.assertIn("所有文件系统写操作都需显式提供 confirm_write=true", server._mcp.instructions)
        self.assertNotIn("common_get_qgis_info", server._mcp.instructions)
        self.assertNotIn("processing_execute_algorithm", server._mcp.instructions)
        self.assertEqual(server._mcp.kwargs["host"], config.host)
        self.assertEqual(server._mcp.kwargs["port"], config.port)
        self.assertNotIn("website_url", server._mcp.kwargs)
        self.assertEqual(server._mcp.kwargs["version"], "9.9.9")
        self.assertEqual(server._mcp._mcp_server.version, "9.9.9")
        self.assertIn("icons", server._mcp.kwargs)
        self.assertEqual(server._mcp.kwargs["icons"][0]["mimeType"], "image/svg+xml")
        self.assertEqual(server._mcp.kwargs["icons"][0]["sizes"], ["128x128"])
        self.assertTrue(
            server._mcp.kwargs["icons"][0]["src"].startswith(
                "data:image/svg+xml;base64,"
            )
        )
        mock_register_tools.assert_called_once_with(
            server._mcp,
            mock_tools_class.return_value,
            enable_execute_code=config.enable_execute_code,
        )
        mock_tools_class.assert_called_once_with("iface", server._runner, config)
        mock_register_prompts.assert_called_once_with(
            server._mcp, mock_tools_class.return_value
        )
        mock_register_resources.assert_called_once_with(
            server._mcp, mock_tools_class.return_value
        )

    def test_build_mcp_server_degrades_without_identity_constructor_params(self):
        """验证 FastMCP 不支持 identity 参数时仍能成功初始化。"""
        fake_transport = _FakeTransport()
        fake_mcp_module = ModuleType("mcp.server.fastmcp")
        fake_mcp_module.FastMCP = _FakeFastMCPWithoutServerIdentity
        config = self._build_config("streamable-http")

        with (
            patch.dict(
                sys.modules,
                {
                    "mcp": ModuleType("mcp"),
                    "mcp.server": ModuleType("mcp.server"),
                    "mcp.server.fastmcp": fake_mcp_module,
                },
            ),
            patch(
                "processingmcpserver.server.create_transport",
                return_value=fake_transport,
            ),
            patch("processingmcpserver.server.ProcessingMCPTools") as mock_tools_class,
            patch("processingmcpserver.server.register_tools") as mock_register_tools,
            patch("processingmcpserver.server.register_prompts") as mock_register_prompts,
            patch(
                "processingmcpserver.server.register_resources"
            ) as mock_register_resources,
            patch(
                "processingmcpserver.server._load_plugin_version_from_metadata",
                return_value="9.9.9",
            ),
        ):
            server = ProcessingMCPServer(iface="iface", config=config)

        self.assertEqual(server._mcp.name, "QGIS Processing MCP Server")
        self.assertEqual(server._mcp.settings.host, config.host)
        self.assertEqual(server._mcp.settings.port, config.port)
        self.assertEqual(server._mcp._mcp_server.version, "9.9.9")
        mock_register_tools.assert_called_once_with(
            server._mcp,
            mock_tools_class.return_value,
            enable_execute_code=config.enable_execute_code,
        )
        mock_register_prompts.assert_called_once_with(
            server._mcp, mock_tools_class.return_value
        )
        mock_register_resources.assert_called_once_with(
            server._mcp, mock_tools_class.return_value
        )

    def test_start_returns_false_when_disabled(self):
        """验证 start returns false when disabled 场景。"""
        fake_transport = _FakeTransport()
        config = replace(self._build_config("streamable-http"), enabled=False)

        with (
            patch.object(
                ProcessingMCPServer, "_build_mcp_server", return_value=SimpleNamespace()
            ),
            patch(
                "processingmcpserver.server.create_transport",
                return_value=fake_transport,
            ),
        ):
            server = ProcessingMCPServer(iface="iface", config=config)

        self.assertFalse(server.start())
        self.assertEqual(fake_transport.start_calls, 0)

    def test_start_and_stop_delegate_to_transport(self):
        """验证 start and stop delegate to transport 场景。"""
        fake_transport = _FakeTransport()
        config = self._build_config("streamable-http")

        with (
            patch.object(
                ProcessingMCPServer, "_build_mcp_server", return_value=SimpleNamespace()
            ),
            patch(
                "processingmcpserver.server.create_transport",
                return_value=fake_transport,
            ),
        ):
            server = ProcessingMCPServer(iface="iface", config=config)

        self.assertTrue(server.start())
        self.assertEqual(fake_transport.start_calls, 1)
        self.assertTrue(server.is_running())

        server.stop()

        self.assertEqual(fake_transport.stop_calls, 1)
        self.assertFalse(server.is_running())

    @patch("processingmcpserver.server.QgsMessageLog.logMessage")
    def test_stop_logs_exception_when_transport_stop_raises(self, mock_log_message):
        """验证 stop logs exception when transport stop raises 场景。"""
        fake_transport = MagicMock()
        fake_transport.stop.side_effect = RuntimeError("stop boom")
        config = self._build_config("streamable-http")

        with (
            patch.object(
                ProcessingMCPServer, "_build_mcp_server", return_value=SimpleNamespace()
            ),
            patch(
                "processingmcpserver.server.create_transport",
                return_value=fake_transport,
            ),
        ):
            server = ProcessingMCPServer(iface="iface", config=config)

        server.stop()

        self.assertTrue(
            any(
                "Failed to stop Processing MCP server" in call.args[0]
                for call in mock_log_message.call_args_list
            )
        )

    @patch("processingmcpserver.server.QgsMessageLog.logMessage")
    def test_start_returns_false_when_transport_raises(self, mock_log_message):
        """验证 start returns false when transport raises 场景。"""
        fake_transport = MagicMock()
        fake_transport.describe.return_value = "broken transport"
        fake_transport.is_running.return_value = False
        fake_transport.start.side_effect = RuntimeError("transport boom")
        config = self._build_config("streamable-http")

        with (
            patch.object(
                ProcessingMCPServer, "_build_mcp_server", return_value=SimpleNamespace()
            ),
            patch(
                "processingmcpserver.server.create_transport",
                return_value=fake_transport,
            ),
        ):
            server = ProcessingMCPServer(iface="iface", config=config)

        self.assertFalse(server.start())
        self.assertTrue(
            any(
                "Failed to start Processing MCP server" in call.args[0]
                for call in mock_log_message.call_args_list
            )
        )
