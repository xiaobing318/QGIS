from __future__ import annotations

"""Processing MCP server assembly and lifecycle management."""

import traceback
from typing import TYPE_CHECKING, Optional

from qgis.core import Qgis, QgsMessageLog

from processingmcpserver.config import (
    MCP_LOG_CATEGORY,
    ProcessingMCPServerConfig,
    load_processing_mcp_server_config,
)
from processingmcpserver.mcp_prompts import register_prompts
from processingmcpserver.mcp_resources import register_resources
from processingmcpserver.mcp_tools import ProcessingMCPTools, register_tools
from processingmcpserver.mcp_main_thread_runner import McpMainThreadRunner
from processingmcpserver.transports import create_transport

if TYPE_CHECKING:
    from mcp.server.fastmcp import FastMCP


_SERVER_INSTRUCTIONS = (
    "该 MCP Server 提供与 QGIS Processing 框架交互的工具，支持查询项目状态、图层详情、执行算法等功能。"
)


class ProcessingMCPServer:
    """封装 FastMCP 与传输层，负责 Processing MCP 服务启动与停止。"""

    def __init__(
        self, iface, config: Optional[ProcessingMCPServerConfig] = None
    ) -> None:
        """初始化服务对象并根据配置构建 MCP 实例与传输对象。"""
        self._iface = iface
        self._config = config or load_processing_mcp_server_config()
        self._config_logged = False
        self._runner = McpMainThreadRunner()
        self._mcp = self._build_mcp_server()
        self._transport = create_transport(self._mcp, self._config)

    def _build_mcp_server(self) -> "FastMCP":
        """创建 FastMCP 服务并注册工具，副作用是加载 MCP 运行时依赖。"""
        try:
            from mcp.server.fastmcp import FastMCP
        except Exception as exc:
            raise RuntimeError(
                "Missing required dependency 'mcp'. Install MCP runtime first."
            ) from exc

        try:
            mcp_server = FastMCP(
                "QGIS Processing MCP Server",
                instructions=_SERVER_INSTRUCTIONS,
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
            tools = ProcessingMCPTools(self._iface, self._runner, self._config)
            register_tools(
                mcp_server,
                tools,
                enable_execute_code=self._config.enable_execute_code,
            )
            register_prompts(mcp_server, tools)
            register_resources(mcp_server, tools)
            return mcp_server
        except Exception:
            self._log_exception("Failed to initialize Processing MCP server")
            raise

    def start(self) -> bool:
        """按配置启动传输层服务，副作用是启动后台线程监听请求。"""
        self._log_config_summary_once()

        if not self._config.enabled:
            QgsMessageLog.logMessage(
                "Processing MCP server disabled by configuration.",
                MCP_LOG_CATEGORY,
                Qgis.Info,
            )
            return False

        if self.is_running():
            return True

        QgsMessageLog.logMessage(
            f"Starting Processing MCP server ({self._config.transport}): "
            f"{self._transport.describe()}",
            MCP_LOG_CATEGORY,
            Qgis.Info,
        )

        try:
            return self._transport.start()
        except Exception:
            self._log_exception("Failed to start Processing MCP server")
            return False

    def stop(self) -> None:
        """停止传输层服务并记录停止结果。"""
        try:
            self._transport.stop()
        except Exception:
            self._log_exception("Failed to stop Processing MCP server")
            return

        if not self._transport.is_running():
            QgsMessageLog.logMessage(
                "Processing MCP server stopped.",
                MCP_LOG_CATEGORY,
                Qgis.Info,
            )

    def is_running(self) -> bool:
        """返回传输层是否处于运行状态。"""
        return self._transport.is_running()

    def _log_exception(self, message: str) -> None:
        """记录异常堆栈到 QGIS 日志，副作用是输出 Critical 级日志。"""
        QgsMessageLog.logMessage(
            f"{message}\n{traceback.format_exc()}",
            MCP_LOG_CATEGORY,
            Qgis.Critical,
        )

    def _log_config_summary_once(self) -> None:
        """仅首次输出配置摘要，便于定位配置来源与依赖策略。"""
        if self._config_logged:
            return
        self._config_logged = True

        config_path = self._config.config_file_path or "<unknown>"
        QgsMessageLog.logMessage(
            f"Processing MCP config file: {config_path}",
            MCP_LOG_CATEGORY,
            Qgis.Info,
        )

        if self._config.config_sources:
            source_text = ", ".join(
                f"{key}={source}"
                for key, source in sorted(self._config.config_sources.items())
            )
            QgsMessageLog.logMessage(
                f"Processing MCP config sources: {source_text}",
                MCP_LOG_CATEGORY,
                Qgis.Info,
            )

        QgsMessageLog.logMessage(
            (
                "Processing MCP tool limits are defined inside mcp_tools.py and "
                "can be overridden per tool call arguments where supported."
            ),
            MCP_LOG_CATEGORY,
            Qgis.Info,
        )

        dependencies = self._config.dependencies
        QgsMessageLog.logMessage(
            (
                "Processing MCP dependencies: "
                f"auto_install={dependencies.auto_install}, "
                "source=processingmcpserver/requirements.txt"
            ),
            MCP_LOG_CATEGORY,
            Qgis.Info,
        )

        filesystem = self._config.filesystem
        QgsMessageLog.logMessage(
            (
                "Processing MCP filesystem policy: "
                f"disable_filesystem_tools={filesystem.disable_filesystem_tools}, "
                f"allowed_roots={filesystem.allowed_roots}, "
                f"readonly_roots={filesystem.readonly_roots}"
            ),
            MCP_LOG_CATEGORY,
            Qgis.Info,
        )
