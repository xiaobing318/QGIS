from __future__ import annotations

from dataclasses import dataclass
import os
from typing import Optional

from qgis.core import QgsSettings

@dataclass(frozen=True)
class MCPServerConfig:
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


def _parse_int(value: object, default: int) -> int:
    if value is None:
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _parse_list(value: object) -> Optional[list[str]]:
    if value is None:
        return None
    text = str(value).strip()
    if not text:
        return None
    items = [item.strip() for item in text.split(",") if item.strip()]
    return items or None


def _parse_transport(value: object, default: str) -> str:
    if value is None:
        return default
    text = str(value).strip().lower()
    if text in ("stdio", "sse", "streamable-http"):
        return text
    if text in ("streamable_http", "streamablehttp"):
        return "streamable-http"
    return default


def load_mcp_config() -> MCPServerConfig:
    settings = QgsSettings()

    enabled = _parse_bool(
        os.environ.get("QGIS_MCP_ENABLED"),
        settings.value("Processing/MCP/enabled", True, type=bool),
    )
    transport = _parse_transport(
        os.environ.get("QGIS_MCP_TRANSPORT"),
        settings.value("Processing/MCP/transport", "streamable-http", type=str),
    )
    host = os.environ.get("QGIS_MCP_HOST") or settings.value(
        "Processing/MCP/host", "127.0.0.1"
    )
    port = _parse_int(
        os.environ.get("QGIS_MCP_PORT"),
        settings.value("Processing/MCP/port", 8000, type=int),
    )
    mount_path = os.environ.get("QGIS_MCP_MOUNT_PATH") or settings.value(
        "Processing/MCP/mount_path", "/", type=str
    )
    sse_path = os.environ.get("QGIS_MCP_SSE_PATH") or settings.value(
        "Processing/MCP/sse_path", "/sse", type=str
    )
    message_path = os.environ.get("QGIS_MCP_MESSAGE_PATH") or settings.value(
        "Processing/MCP/message_path", "/messages/", type=str
    )
    streamable_http_path = os.environ.get(
        "QGIS_MCP_STREAMABLE_HTTP_PATH"
    ) or settings.value("Processing/MCP/streamable_http_path", "/mcp")
    stateless_http = _parse_bool(
        os.environ.get("QGIS_MCP_STATELESS_HTTP"),
        settings.value("Processing/MCP/stateless_http", True, type=bool),
    )
    json_response = _parse_bool(
        os.environ.get("QGIS_MCP_JSON_RESPONSE"),
        settings.value("Processing/MCP/json_response", True, type=bool),
    )
    log_level = (
        os.environ.get("QGIS_MCP_LOG_LEVEL")
        or settings.value("Processing/MCP/log_level", "INFO")
    ).upper()
    cors_origins = _parse_list(
        os.environ.get("QGIS_MCP_CORS_ORIGINS")
        or settings.value("Processing/MCP/cors_origins", "", type=str)
    )
    cors_allow_headers = _parse_list(
        os.environ.get("QGIS_MCP_CORS_ALLOW_HEADERS")
        or settings.value(
            "Processing/MCP/cors_allow_headers",
            "mcp-session-id,mcp-protocol-version,last-event-id,authorization",
            type=str,
        )
    )

    return MCPServerConfig(
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
