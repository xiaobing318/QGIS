# 从其他模块中导入所需要的类、函数、类型等等
from __future__ import annotations

import traceback
from typing import Optional

from mcp.server.fastmcp import FastMCP
from qgis.core import Qgis, QgsMessageLog

from processing.processingmcpserver.config import ProcessingMCPServerConfig, load_processing_mcp_server_config
from processing.processingmcpserver.qtRunner import MainThreadRunner
from processing.processingmcpserver.transports import create_transport
from processing.processingmcpserver.tools import ProcessingMCPTools, register_tools


# 创建 processing mcp 服务器类
class ProcessingMCPServer:
    # 初始化 processing mcp server 类，相当于 C++ 类中的构造函数，如果没有传入配置参数，则加载默认配置。
    def __init__(self, iface, config: Optional[ProcessingMCPServerConfig] = None) -> None:
        # 保存 QGIS 界面对象与解析后的配置。
        self._iface = iface
        # 如果没有传入配置，那么就在加载默认配置。
        self._config = config or load_processing_mcp_server_config()
        # 运行器确保 QGIS API 在主线程调用。
        self._runner = MainThreadRunner()
        # 构建 MCP 服务器。
        self._mcp = self._build_mcp_server()
        # 根据配置创建传输层。
        self._transport = create_transport(self._mcp, self._config)

    def _build_mcp_server(self) -> FastMCP:
        # 实例化 FastMCP 并注册所有 Processing 工具。
        try:
            mcpServer = FastMCP(
                "QGIS Processing MCP Server",
                instructions=("Expose QGIS processing, project, and layer operations via MCP."),
                host=self._config.host,
                port=self._config.port,
                mount_path=self._config.mount_path,
                sse_path=self._config.sse_path,
                message_path=self._config.message_path,
                streamable_http_path=self._config.streamable_http_path,
                stateless_http=self._config.stateless_http,
                json_response=self._config.json_response,
                log_level=self._config.log_level,
            )
            tools = ProcessingMCPTools(self._iface, self._runner)
            register_tools(mcpServer, tools)
            return mcpServer
        except Exception:
            # 记录根因并向调用方抛出异常。
            self._log_exception("Failed to initialize Processing MCP server")
            raise

    def start(self) -> bool:
        # 遵循配置并避免重复启动。
        if not self._config.enabled:
            QgsMessageLog.logMessage(
                "Processing MCP server disabled by configuration.",
                "Processing MCP",
                Qgis.Info,
            )
            return False
        if self.is_running():
            return True

        # 记录启动信息并交给传输层处理。
        QgsMessageLog.logMessage(
            f"Starting Processing MCP server ({self._config.transport}): "
            f"{self._transport.describe()}",
            "Processing MCP",
            Qgis.Info,
        )
        try:
            return self._transport.start()
        except Exception:
            # 记录失败信息但不影响 QGIS 继续运行。
            self._log_exception("Failed to start Processing MCP server")
            return False

    def stop(self) -> None:
        # 尝试优雅地停止传输层。
        try:
            self._transport.stop()
        except Exception:
            # 记录异常，停止尽力而为即可。
            self._log_exception("Failed to stop Processing MCP server")
            return
        if not self._transport.is_running():
            QgsMessageLog.logMessage(
                "Processing MCP server stopped.",
                "Processing MCP",
                Qgis.Info,
            )

    def is_running(self) -> bool:
        # 透传传输层的运行状态。
        return self._transport.is_running()

    def _log_exception(self, message: str) -> None:
        # 附加回溯信息，便于诊断。
        QgsMessageLog.logMessage(
            f"{message}\n{traceback.format_exc()}",
            "Processing MCP",
            Qgis.Critical,
        )
