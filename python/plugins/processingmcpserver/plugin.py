from __future__ import annotations

from qgis.core import Qgis, QgsMessageLog

from processingmcpserver.config import MCP_LOG_CATEGORY


class ProcessingMCPServerPlugin:
    """管理 Processing MCP Server 的生命周期并对接 QGIS 插件入口。"""

    def __init__(self, iface):
        """保存 QGIS 接口对象并初始化服务器句柄。"""
        self.iface = iface
        self._server = None

    def initGui(self):
        """在插件加载时执行依赖检测并按配置启动 MCP 服务。"""
        try:
            from processingmcpserver.config import load_processing_mcp_server_config
            from processingmcpserver.dependency_manager import (
                ensure_processing_mcp_dependencies,
            )
            from processingmcpserver.server import ProcessingMCPServer
        except Exception as exc:
            QgsMessageLog.logMessage(
                f"Processing MCP server is unavailable: {exc}",
                MCP_LOG_CATEGORY,
                Qgis.Warning,
            )
            return

        try:
            config = load_processing_mcp_server_config()
            dependency_result = ensure_processing_mcp_dependencies(
                config.dependencies
            )
            if not dependency_result.success:
                QgsMessageLog.logMessage(
                    "Processing MCP server startup skipped due to dependency check failure.",
                    MCP_LOG_CATEGORY,
                    Qgis.Critical,
                )
                return

            server = ProcessingMCPServer(self.iface, config=config)
            if server.start():
                self._server = server
        except Exception as exc:
            QgsMessageLog.logMessage(
                f"Failed to start Processing MCP server: {exc}",
                MCP_LOG_CATEGORY,
                Qgis.Warning,
            )

    def unload(self):
        """在插件卸载时停止 MCP 服务并清理内部状态。"""
        if not self._server:
            return

        try:
            self._server.stop()
        except Exception as exc:
            QgsMessageLog.logMessage(
                f"Failed to stop Processing MCP server: {exc}",
                MCP_LOG_CATEGORY,
                Qgis.Warning,
            )
        finally:
            self._server = None

