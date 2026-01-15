from __future__ import annotations

from dataclasses import dataclass
import os
from typing import Optional

from qgis.core import QgsSettings

"""
配置说明：
- 读取优先级：环境变量 > QGIS 设置(QgsSettings) > 默认值。
- 手动设置方式：
  1) 环境变量：启动 QGIS 前设置 QGIS_MCP_*。
  2) QGIS 设置：QgsSettings().setValue("Processing/MCP/键名", 值)。
- 列表型配置（cors_origins / cors_allow_headers）使用逗号分隔字符串。
- transport 支持：stdio、sse、streamable-http（也接受 streamable_http / streamablehttp）。
- 配置项一览（环境变量 / QGIS 设置 / 默认值）：
  enabled:              QGIS_MCP_ENABLED / Processing/MCP/enabled / True
  transport:            QGIS_MCP_TRANSPORT / Processing/MCP/transport / streamable-http
  host:                 QGIS_MCP_HOST / Processing/MCP/host / 127.0.0.1
  port:                 QGIS_MCP_PORT / Processing/MCP/port / 8000
  mount_path:           QGIS_MCP_MOUNT_PATH / Processing/MCP/mount_path / /
  sse_path:             QGIS_MCP_SSE_PATH / Processing/MCP/sse_path / /sse
  message_path:         QGIS_MCP_MESSAGE_PATH / Processing/MCP/message_path / /messages/
  streamable_http_path: QGIS_MCP_STREAMABLE_HTTP_PATH / Processing/MCP/streamable_http_path / /mcp
  stateless_http:       QGIS_MCP_STATELESS_HTTP / Processing/MCP/stateless_http / True
  json_response:        QGIS_MCP_JSON_RESPONSE / Processing/MCP/json_response / True
  log_level:            QGIS_MCP_LOG_LEVEL / Processing/MCP/log_level / INFO
  cors_origins:         QGIS_MCP_CORS_ORIGINS / Processing/MCP/cors_origins / 空字符串
  cors_allow_headers:   QGIS_MCP_CORS_ALLOW_HEADERS / Processing/MCP/cors_allow_headers / 默认值见下方代码
"""
# MCP 服务器的不可变配置值。
@dataclass(frozen=True)
class ProcessingMCPServerConfig:
    enabled: bool
    transport: str
    host: str
    port: int
    mount_path: str
    sse_path: str
    message_path: str
    streamable_http_path: str
    stateless_http: bool
    json_response: bool
    log_level: str
    cors_origins: Optional[list[str]]
    cors_allow_headers: Optional[list[str]]


# 解析常见真假值为布尔值，无法解析时使用默认值。
def _parse_bool(value: object, default: bool) -> bool:
    if value is None:
        return default
    if isinstance(value, bool):
        return value
    text = str(value).strip().lower()
    if text in ("1", "true", "yes", "on"):
        return True
    if text in ("0", "false", "no", "off"):
        return False
    return default


# 解析整数，失败时使用默认值。
def _parse_int(value: object, default: int) -> int:
    if value is None:
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


# 解析逗号分隔的列表，空值时返回 None。
def _parse_list(value: object) -> Optional[list[str]]:
    if value is None:
        return None
    text = str(value).strip()
    if not text:
        return None
    items = [item.strip() for item in text.split(",") if item.strip()]
    return items or None


# 规范化传输类型名称到支持的取值。
def _parse_transport(value: object, default: str) -> str:
    if value is None:
        return default
    text = str(value).strip().lower()
    if text in ("stdio", "sse", "streamable-http"):
        return text
    if text in ("streamable_http", "streamablehttp"):
        return "streamable-http"
    return default


# 从环境变量与 QGIS 设置中加载配置。
def load_processing_mcp_server_config() -> ProcessingMCPServerConfig:
    # QGIS 设置存储，用于读取用户/系统默认值。
    settings = QgsSettings()

    # 服务器开关配置。
    enabled = _parse_bool(
        os.environ.get("QGIS_MCP_ENABLED"),
        settings.value("Processing/MCP/enabled", True, type=bool),
    )
    # 传输类型选择（stdio、sse、streamable-http）。
    transport = _parse_transport(
        os.environ.get("QGIS_MCP_TRANSPORT"),
        settings.value("Processing/MCP/transport", "streamable-http", type=str),
    )
    # HTTP 传输绑定的主机地址。
    host = os.environ.get("QGIS_MCP_HOST") or settings.value(
        "Processing/MCP/host", "127.0.0.1"
    )
    # HTTP 传输使用的 TCP 端口。
    port = _parse_int(
        os.environ.get("QGIS_MCP_PORT"),
        settings.value("Processing/MCP/port", 8000, type=int),
    )
    # 服务基础挂载路径。
    mount_path = os.environ.get("QGIS_MCP_MOUNT_PATH") or settings.value(
        "Processing/MCP/mount_path", "/", type=str
    )
    # SSE 端点路径。
    sse_path = os.environ.get("QGIS_MCP_SSE_PATH") or settings.value(
        "Processing/MCP/sse_path", "/sse", type=str
    )
    # SSE 消息端点路径。
    message_path = os.environ.get("QGIS_MCP_MESSAGE_PATH") or settings.value(
        "Processing/MCP/message_path", "/messages/", type=str
    )
    # Streamable HTTP 端点路径。
    streamable_http_path = os.environ.get(
        "QGIS_MCP_STREAMABLE_HTTP_PATH"
    ) or settings.value("Processing/MCP/streamable_http_path", "/mcp")
    # 是否启用无状态 HTTP 模式。
    stateless_http = _parse_bool(
        os.environ.get("QGIS_MCP_STATELESS_HTTP"),
        settings.value("Processing/MCP/stateless_http", True, type=bool),
    )
    # 是否返回 JSON 响应。
    json_response = _parse_bool(
        os.environ.get("QGIS_MCP_JSON_RESPONSE"),
        settings.value("Processing/MCP/json_response", True, type=bool),
    )
    # MCP 服务器日志级别。
    log_level = (
        os.environ.get("QGIS_MCP_LOG_LEVEL")
        or settings.value("Processing/MCP/log_level", "INFO")
    ).upper()
    # 可选的 CORS 允许来源列表。
    cors_origins = _parse_list(
        os.environ.get("QGIS_MCP_CORS_ORIGINS")
        or settings.value("Processing/MCP/cors_origins", "", type=str)
    )
    # 可选的 CORS 允许请求头列表。
    cors_allow_headers = _parse_list(
        os.environ.get("QGIS_MCP_CORS_ALLOW_HEADERS")
        or settings.value(
            "Processing/MCP/cors_allow_headers",
            "mcp-session-id,mcp-protocol-version,last-event-id,authorization",
            type=str,
        )
    )

    # 构建并返回不可变的配置对象。
    return ProcessingMCPServerConfig(
        enabled=enabled,
        transport=transport,
        host=host,
        port=port,
        mount_path=mount_path,
        sse_path=sse_path,
        message_path=message_path,
        streamable_http_path=streamable_http_path,
        stateless_http=stateless_http,
        json_response=json_response,
        log_level=log_level,
        cors_origins=cors_origins,
        cors_allow_headers=cors_allow_headers,
    )
