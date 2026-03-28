"""QGIS plugin entrypoint for the Processing MCP Server lifecycle."""

from __future__ import annotations

from qgis.core import Qgis, QgsMessageLog

from processingmcpserver.config import MCP_LOG_CATEGORY


class ProcessingMCPServerPlugin:
    """Manage the Processing MCP Server lifecycle and QGIS plugin entrypoint."""

    def __init__(self, iface):
        """Store the QGIS interface object and initialize the server handle."""
        self.iface = iface
        self._server = None

    def initGui(self):
        """Run dependency checks on load and start the MCP server when enabled."""
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
        """Stop the MCP server during plugin unload and clear internal state."""
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
