from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from qgis.core import QgsSettings

from processingmcpserver.config_defaults import (
    default_processing_mcp_json_document,
    processing_mcp_config_file_path,
)
from processingmcpserver.config_parsers import (
    _int_parser,
    _log_warning,
    _parse_bool,
    _parse_log_level,
    _parse_string,
    _parse_string_list_or_none,
    _parse_transport,
    _resolve_value,
)
from processingmcpserver.config_types import (
    DEFAULT_CORS_ALLOW_HEADERS,
    DEFAULT_CORS_ORIGINS,
    ProcessingMCPServerConfig,
    ProcessingMCPServerDependencies,
)


def _load_json_config_file(path: Path) -> dict[str, Any]:
    """
    作用：封装内部辅助步骤 `_load_json_config_file`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_load_json_config_file`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在插件启动读取配置时被调用，用于合并 JSON、Settings 与默认值。
    参数与返回：
    - 参数 `path`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    default_doc = default_processing_mcp_json_document()

    if not path.exists():
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                json.dumps(default_doc, ensure_ascii=False, indent=2),
                encoding="utf-8",
            )
        except Exception as exc:
            _log_warning(
                f"Failed to create default Processing MCP config file {path}: {exc}"
            )
            return {}
        return default_doc

    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        _log_warning(
            f"Failed to parse Processing MCP config file {path}: {exc}. "
            "Fallback to QGIS settings and defaults."
        )
        return {}

    if not isinstance(payload, dict):
        _log_warning(
            f"Processing MCP config file {path} must contain a JSON object. "
            "Fallback to QGIS settings and defaults."
        )
        return {}

    return payload


def load_processing_mcp_server_config() -> ProcessingMCPServerConfig:
    """
    作用：加载 `processing_mcp_server_config`，完成当前函数负责的处理步骤并产出结果。
    用途：加载 `processing_mcp_server_config`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在插件启动读取配置时被调用，用于合并 JSON、Settings 与默认值。
    参数与返回：
    - 参数：无。
    - 返回：返回 `ProcessingMCPServerConfig` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `ProcessingMCPServerConfig` 类型结果，返回值语义遵循该函数实现约定。
    """
    settings = QgsSettings()
    json_path = processing_mcp_config_file_path()
    payload = _load_json_config_file(json_path)
    json_mcp = payload.get("processing_mcp") if isinstance(payload, dict) else None

    if json_mcp is not None and not isinstance(json_mcp, dict):
        _log_warning(
            "The 'processing_mcp' section in JSON config must be an object. "
            "Fallback to QGIS settings and defaults."
        )
        json_mcp = {}
    if not isinstance(json_mcp, dict):
        json_mcp = {}

    json_dependencies = json_mcp.get("dependencies")
    if json_dependencies is not None and not isinstance(json_dependencies, dict):
        _log_warning(
            "The 'processing_mcp.dependencies' section in JSON config must be an "
            "object. Fallback to QGIS settings and defaults."
        )
        json_dependencies = {}
    if not isinstance(json_dependencies, dict):
        json_dependencies = {}

    sources: dict[str, str] = {}

    enabled = _resolve_value(
        "enabled",
        json_mcp.get("enabled"),
        settings.value("Processing/MCP/enabled", True),
        True,
        _parse_bool,
        sources,
    )
    transport = _resolve_value(
        "transport",
        json_mcp.get("transport"),
        settings.value("Processing/MCP/transport", "streamable-http"),
        "streamable-http",
        _parse_transport,
        sources,
    )
    host = _resolve_value(
        "host",
        json_mcp.get("host"),
        settings.value("Processing/MCP/host", "127.0.0.1"),
        "127.0.0.1",
        _parse_string,
        sources,
    )
    port = _resolve_value(
        "port",
        json_mcp.get("port"),
        settings.value("Processing/MCP/port", 8000),
        8000,
        _int_parser(1, 65535),
        sources,
    )
    mount_path = _resolve_value(
        "mount_path",
        json_mcp.get("mount_path"),
        settings.value("Processing/MCP/mount_path", "/"),
        "/",
        _parse_string,
        sources,
    )
    sse_path = _resolve_value(
        "sse_path",
        json_mcp.get("sse_path"),
        settings.value("Processing/MCP/sse_path", "/sse"),
        "/sse",
        _parse_string,
        sources,
    )
    message_path = _resolve_value(
        "message_path",
        json_mcp.get("message_path"),
        settings.value("Processing/MCP/message_path", "/messages/"),
        "/messages/",
        _parse_string,
        sources,
    )
    streamable_http_path = _resolve_value(
        "streamable_http_path",
        json_mcp.get("streamable_http_path"),
        settings.value("Processing/MCP/streamable_http_path", "/mcp"),
        "/mcp",
        _parse_string,
        sources,
    )
    stateless_http = _resolve_value(
        "stateless_http",
        json_mcp.get("stateless_http"),
        settings.value("Processing/MCP/stateless_http", True),
        True,
        _parse_bool,
        sources,
    )
    json_response = _resolve_value(
        "json_response",
        json_mcp.get("json_response"),
        settings.value("Processing/MCP/json_response", True),
        True,
        _parse_bool,
        sources,
    )
    log_level = _resolve_value(
        "log_level",
        json_mcp.get("log_level"),
        settings.value("Processing/MCP/log_level", "INFO"),
        "INFO",
        _parse_log_level,
        sources,
    )
    cors_origins = _resolve_value(
        "cors_origins",
        json_mcp.get("cors_origins"),
        settings.value("Processing/MCP/cors_origins", ",".join(DEFAULT_CORS_ORIGINS)),
        list(DEFAULT_CORS_ORIGINS),
        _parse_string_list_or_none,
        sources,
    )
    cors_allow_headers = _resolve_value(
        "cors_allow_headers",
        json_mcp.get("cors_allow_headers"),
        settings.value(
            "Processing/MCP/cors_allow_headers", ",".join(DEFAULT_CORS_ALLOW_HEADERS)
        ),
        list(DEFAULT_CORS_ALLOW_HEADERS),
        _parse_string_list_or_none,
        sources,
    )
    enable_execute_code = _resolve_value(
        "enable_execute_code",
        json_mcp.get("enable_execute_code"),
        settings.value("Processing/MCP/enable_execute_code", False),
        False,
        _parse_bool,
        sources,
    )
    dependency_auto_install = _resolve_value(
        "dependencies.auto_install",
        json_dependencies.get("auto_install"),
        settings.value("Processing/MCP/dependencies/auto_install", True),
        True,
        _parse_bool,
        sources,
    )
    dependencies = ProcessingMCPServerDependencies(
        auto_install=dependency_auto_install,
    )

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
        enable_execute_code=enable_execute_code,
        dependencies=dependencies,
        config_sources=sources,
        config_file_path=str(json_path),
    )
