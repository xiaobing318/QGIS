from __future__ import annotations

from pathlib import Path
from typing import Any

from qgis.core import QgsApplication

from processingmcpserver.config_types import (
    DEFAULT_CORS_ALLOW_HEADERS,
    DEFAULT_CORS_ORIGINS,
)


def processing_mcp_config_file_path() -> Path:
    """返回 Processing MCP 的 JSON 配置文件路径。"""
    return (
        Path(QgsApplication.qgisSettingsDirPath())
        / "processingmcpserver"
        / "config.json"
    )


def default_processing_mcp_json_document() -> dict[str, Any]:
    """生成默认配置文档，用于首次启动自动落盘。"""
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
