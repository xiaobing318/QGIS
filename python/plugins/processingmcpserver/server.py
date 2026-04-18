"""Processing MCP server assembly and lifecycle management."""

from __future__ import annotations

import base64
import configparser
import inspect
import traceback
from pathlib import Path
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
    "能力说明：该服务运行在 QGIS 桌面会话内，可读取当前工程与图层上下文，支持空间数据管理与分析、地理处理执行、任务提示模板与项目资源摘要，以及受控的文件系统探查与编辑能力。"
    "使用背景：适用于需要让模型基于真实 QGIS 工程状态完成分析与处理任务的场景，尤其适合把自动化流程和人工复核结合起来。"
    "推荐工作流：先探查工程与图层现状并明确目标，再核对输入输出和关键参数；随后以小范围、非破坏方式试跑并检查结果与告警；确认后再执行批量处理，并在每轮后复核输出质量。"
    "注意事项：默认采用非破坏执行策略；所有文件系统写操作都需显式提供 confirm_write=true；删除与覆盖操作还需 confirm_destructive=true；执行失败时先回查参数与图层引用，再进行最小化重试。"
)
_PLUGIN_METADATA_FILENAME = "metadata.txt"
_SERVER_ICON_FILENAME = "processingmcpserver.svg"
_SERVER_ICON_MIME_TYPE = "image/svg+xml"
_SERVER_ICON_SIZE = "128x128"


def _load_plugin_version_from_metadata() -> str | None:
    """
    作用：封装内部辅助步骤 `_load_plugin_version_from_metadata`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_load_plugin_version_from_metadata`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
    参数与返回：
    - 参数：无。
    - 返回：返回 `str | None` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str | None` 类型结果，返回值语义遵循该函数实现约定。
    """
    metadata_path = Path(__file__).resolve().parent / _PLUGIN_METADATA_FILENAME
    parser = configparser.ConfigParser()

    try:
        with metadata_path.open("r", encoding="utf-8") as handle:
            parser.read_file(handle)
    except (OSError, configparser.Error) as exc:
        QgsMessageLog.logMessage(
            f"Failed to read processingmcpserver metadata version: {exc}",
            MCP_LOG_CATEGORY,
            Qgis.Warning,
        )
        return None

    version = parser.get("general", "version", fallback="").strip()
    if version:
        return version

    QgsMessageLog.logMessage(
        "processingmcpserver metadata version is empty; fallback to MCP package "
        "version.",
        MCP_LOG_CATEGORY,
        Qgis.Warning,
    )
    return None


def _plugin_icon_data_uri() -> str:
    """
    作用：封装内部辅助步骤 `_plugin_icon_data_uri`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_plugin_icon_data_uri`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
    参数与返回：
    - 参数：无。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    icon_path = Path(__file__).resolve().parent / "icons" / _SERVER_ICON_FILENAME
    encoded = base64.b64encode(icon_path.read_bytes()).decode("ascii")
    return f"data:{_SERVER_ICON_MIME_TYPE};base64,{encoded}"


def _fastmcp_supports_keyword_parameter(fastmcp_init, parameter_name: str) -> bool:
    """
    作用：封装内部辅助步骤 `_fastmcp_supports_keyword_parameter`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_fastmcp_supports_keyword_parameter`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
    参数与返回：
    - 参数 `fastmcp_init`：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `parameter_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
    """
    try:
        signature = inspect.signature(fastmcp_init)
    except (TypeError, ValueError):
        return False

    if parameter_name in signature.parameters:
        return True

    return any(
        parameter.kind == inspect.Parameter.VAR_KEYWORD
        for parameter in signature.parameters.values()
    )


class ProcessingMCPServer:
    """Wrap FastMCP and the transport layer to start and stop the server."""

    def __init__(
        self, iface, config: Optional[ProcessingMCPServerConfig] = None
    ) -> None:
        """
        作用：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `iface`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `config`（`Optional[ProcessingMCPServerConfig]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self._iface = iface
        self._config = config or load_processing_mcp_server_config()
        self._config_logged = False
        self._runner = McpMainThreadRunner()
        self._mcp = self._build_mcp_server()
        self._transport = create_transport(self._mcp, self._config)

    def _build_mcp_server(self) -> "FastMCP":
        """
        作用：构建 `_build_mcp_server` 相关对象或配置数据，供后续流程直接复用。
        用途：构建 `_build_mcp_server` 相关对象或配置数据，供后续流程直接复用。
        使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `"FastMCP"` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `"FastMCP"` 类型结果，返回值语义遵循该函数实现约定。
        异常：可能显式抛出 `RuntimeError`，并在部分分支中透传当前异常。
        """
        try:
            from mcp.server.fastmcp import FastMCP
        except Exception as exc:
            raise RuntimeError(
                "Missing required dependency 'mcp'. Install MCP runtime first."
            ) from exc

        try:
            plugin_version = _load_plugin_version_from_metadata()
            fastmcp_identity_kwargs = self._build_fastmcp_identity_kwargs(
                FastMCP.__init__, plugin_version
            )
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
                **fastmcp_identity_kwargs,
            )
            self._apply_plugin_version_to_low_level_server(mcp_server, plugin_version)
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

    def _build_fastmcp_identity_kwargs(
        self, fastmcp_init, plugin_version: str | None
    ) -> dict[str, object]:
        """
        作用：构建 `_build_fastmcp_identity_kwargs` 相关对象或配置数据，供后续流程直接复用。
        用途：构建 `_build_fastmcp_identity_kwargs` 相关对象或配置数据，供后续流程直接复用。
        使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `fastmcp_init`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `plugin_version`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回 `dict[str, object]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, object]` 类型结果，返回值语义遵循该函数实现约定。
        """
        identity_kwargs: dict[str, object] = {}

        if plugin_version and _fastmcp_supports_keyword_parameter(
            fastmcp_init, "version"
        ):
            identity_kwargs["version"] = plugin_version

        if not _fastmcp_supports_keyword_parameter(fastmcp_init, "icons"):
            return identity_kwargs

        try:
            icon_data_uri = _plugin_icon_data_uri()
        except OSError as exc:
            QgsMessageLog.logMessage(
                f"Failed to read Processing MCP icon file: {exc}",
                MCP_LOG_CATEGORY,
                Qgis.Warning,
            )
            return identity_kwargs

        identity_kwargs["icons"] = [
            {
                "src": icon_data_uri,
                "mimeType": _SERVER_ICON_MIME_TYPE,
                "sizes": [_SERVER_ICON_SIZE],
            }
        ]
        return identity_kwargs

    def _apply_plugin_version_to_low_level_server(
        self, mcp_server: object, plugin_version: str | None
    ) -> None:
        """
        作用：封装内部辅助步骤 `_apply_plugin_version_to_low_level_server`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_apply_plugin_version_to_low_level_server`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mcp_server`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `plugin_version`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        if not plugin_version:
            return

        low_level_server = getattr(mcp_server, "_mcp_server", None)
        if low_level_server is None or not hasattr(low_level_server, "version"):
            return

        try:
            setattr(low_level_server, "version", plugin_version)
        except Exception as exc:
            QgsMessageLog.logMessage(
                f"Failed to override Processing MCP server version: {exc}",
                MCP_LOG_CATEGORY,
                Qgis.Warning,
            )

    def start(self) -> bool:
        """
        作用：实现 `start` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `start` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        """
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
            started = self._transport.start()
            if started:
                endpoint_getter = getattr(self._transport, "endpoint_url", None)
                endpoint = endpoint_getter() if callable(endpoint_getter) else None
                if endpoint:
                    QgsMessageLog.logMessage(
                        f"Processing MCP final endpoint URL: {endpoint}",
                        MCP_LOG_CATEGORY,
                        Qgis.Info,
                    )
            return started
        except Exception:
            self._log_exception("Failed to start Processing MCP server")
            return False

    def stop(self) -> None:
        """
        作用：实现 `stop` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `stop` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
        """
        作用：实现 `is_running` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `is_running` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        """
        return self._transport.is_running()

    def _log_exception(self, message: str) -> None:
        """
        作用：封装内部辅助步骤 `_log_exception`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_log_exception`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `message`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        QgsMessageLog.logMessage(
            f"{message}\n{traceback.format_exc()}",
            MCP_LOG_CATEGORY,
            Qgis.Critical,
        )

    def _log_config_summary_once(self) -> None:
        """
        作用：封装内部辅助步骤 `_log_config_summary_once`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_log_config_summary_once`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 Processing MCP 服务器运行流程中被调用，用于组装服务、注册能力并管理运行状态。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
                "Processing MCP tool limits are defined inside processingmcpserver.mcp_tools package and "
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
