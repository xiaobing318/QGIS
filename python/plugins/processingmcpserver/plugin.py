"""QGIS plugin entrypoint for the Processing MCP Server lifecycle."""

from __future__ import annotations

from qgis.core import Qgis, QgsMessageLog

from processingmcpserver.config import MCP_LOG_CATEGORY


class ProcessingMCPServerPlugin:
    """Manage the Processing MCP Server lifecycle and QGIS plugin entrypoint."""

    def __init__(self, iface):
        """
        作用：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 QGIS 插件生命周期流程中被调用，用于联动界面生命周期与 MCP 服务器生命周期。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `iface`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.iface = iface
        self._server = None

    def initGui(self):
        """
        作用：实现 `initGui` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `initGui` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 QGIS 插件生命周期流程中被调用，用于联动界面生命周期与 MCP 服务器生命周期。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
        """
        作用：实现 `unload` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `unload` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 QGIS 插件生命周期流程中被调用，用于联动界面生命周期与 MCP 服务器生命周期。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
