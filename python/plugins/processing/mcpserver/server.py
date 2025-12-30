from __future__ import annotations

import traceback
from typing import Optional

from mcp.server.fastmcp import FastMCP
from qgis.core import Qgis, QgsMessageLog

from processing.mcpserver.config import McpServerConfig, load_mcp_config
from processing.mcpserver.qtRunner import MainThreadRunner
from processing.mcpserver.transports import create_transport
from processing.mcpserver.tools import ProcessingMCPTools, register_tools


class ProcessingMCPServer:
    def __init__(self, iface, config: Optional[McpServerConfig] = None) -> None:
        self._iface = iface
        self._config = config or load_mcp_config()
        self._runner = MainThreadRunner()
        self._mcp = self._build_mcp_server()
        self._transport = create_transport(self._mcp, self._config)

    def _build_mcp_server(self) -> FastMCP:
        try:
            mcpServer = FastMCP(
                "QGIS Processing MCP",
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
            self._log_exception("Failed to initialize Processing MCP server")
            raise

    def start(self) -> bool:
        if not self._config.enabled:
            QgsMessageLog.logMessage(
                "Processing MCP server disabled by configuration.",
                "Processing MCP",
                Qgis.Info,
            )
            return False
        if self.is_running():
            return True

        QgsMessageLog.logMessage(
            f"Starting Processing MCP server ({self._config.transport}): "
            f"{self._transport.describe()}",
            "Processing MCP",
            Qgis.Info,
        )
        try:
            return self._transport.start()
        except Exception:
            self._log_exception("Failed to start Processing MCP server")
            return False

    def stop(self) -> None:
        try:
            self._transport.stop()
        except Exception:
            self._log_exception("Failed to stop Processing MCP server")
            return
        if not self._transport.is_running():
            QgsMessageLog.logMessage(
                "Processing MCP server stopped.",
                "Processing MCP",
                Qgis.Info,
            )

    def is_running(self) -> bool:
        return self._transport.is_running()

    def _log_exception(self, message: str) -> None:
        QgsMessageLog.logMessage(
            f"{message}\n{traceback.format_exc()}",
            "Processing MCP",
            Qgis.Critical,
        )
