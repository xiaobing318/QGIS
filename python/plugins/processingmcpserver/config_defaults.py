from __future__ import annotations

from pathlib import Path
from typing import Any

from qgis.core import QgsApplication

from processingmcpserver.config_types import (
    DEFAULT_CORS_ALLOW_HEADERS,
    DEFAULT_CORS_ORIGINS,
)


def processing_mcp_config_file_path() -> Path:
    """
    作用：处理 `processing_mcp_config_file_path` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `processing_mcp_config_file_path` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 processingmcpserver 插件运行流程中被调用，用于支撑当前模块的业务处理。
    参数与返回：
    - 参数：无。
    - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    """
    return (
        Path(QgsApplication.qgisSettingsDirPath())
        / "processingmcpserver"
        / "config.json"
    )


def default_processing_mcp_json_document() -> dict[str, Any]:
    """
    作用：处理 `default_processing_mcp_json_document` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `default_processing_mcp_json_document` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 processingmcpserver 插件运行流程中被调用，用于支撑当前模块的业务处理。
    参数与返回：
    - 参数：无。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return {
        "version": 1,
        "processing_mcp": {
            "enabled": True,
            "transport": "streamable-http",
            "host": "127.0.0.1",
            "port": 8000,
            "mount_path": "/",
            "sse_path": "/sse",
            "message_path": "/messages/",
            "streamable_http_path": "/mcp",
            "stateless_http": True,
            "json_response": True,
            "log_level": "INFO",
            "cors_origins": list(DEFAULT_CORS_ORIGINS),
            "cors_allow_headers": list(DEFAULT_CORS_ALLOW_HEADERS),
            "enable_execute_code": False,
            "dependencies": {
                "auto_install": True,
            },
        },
    }
