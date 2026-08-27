"""Constants shared by QCopilots plugins and MCP services.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import json
import os
from collections.abc import Iterable
from pathlib import Path


MENU_NAME = "&QCopilots"
LOG_CHANNEL = "QCopilots"
MCP_PROTOCOL_VERSION = "2025-06-18"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_MCP_PATH = "/mcp"
DEFAULT_CORS_ORIGINS: tuple[str, ...] = ()

QCOPILOTS_HOME_ENV = "QCOPILOTS_HOME"
BRIDGE_URL_ENV = "QCOPILOTS_BRIDGE_URL"
MCP_AUTH_TOKEN_ENV = "QCOPILOTS_MCP_AUTH_TOKEN"
QGIS_BRIDGE_AUTH_TOKEN_ENV = "QCOPILOTS_QGIS_BRIDGE_AUTH_TOKEN"
CORS_ORIGINS_ENV = "QCOPILOTS_CORS_ORIGINS"
UV_EXECUTABLE_ENV = "QCOPILOTS_UV"
SERVICE_DESCRIPTION_ENV = "QCOPILOTS_SERVICE_DESCRIPTION"
SERVICE_ICON_ENV = "QCOPILOTS_SERVICE_ICON"
SERVICE_TITLE_ENV = "QCOPILOTS_SERVICE_TITLE"

DEFAULT_BRIDGE_PORT = 48200
DEFAULT_SERVICE_PORTS = {
    "qcopilots.mcp_server_builtin_tools": 48211,
    "qcopilots.mcp_server_skills": 48212,
    "qcopilots.mcp_server_interactive_tools": 48213,
    "qcopilots.mcp_server_processing_vector": 48214,
    "qcopilots.mcp_server_processing_raster": 48215,
    "qcopilots.mcp_server_qgis_binary": 48216,
    "qcopilots.mcp_server_processing_general": 48217,
}

MAX_FILE_READ_BYTES = 256 * 1024
MAX_HTTP_REQUEST_BODY_BYTES = 1024 * 1024
MAX_HTTP_ERROR_BODY_DRAIN_BYTES = 64 * 1024
HTTP_ERROR_BODY_DRAIN_TIMEOUT_SECONDS = 0.25
MAX_TOOL_TEXT_CHARS = 200 * 1024
MAX_GLOB_RESULTS = 200
MAX_GREP_RESULTS = 200
MAX_GREP_SCANNED_FILES = 5000
MAX_EXEC_TIMEOUT_SECONDS = 60
MAX_EXEC_OUTPUT_CHARS = 128 * 1024

DEFAULT_PROCESSING_ALGORITHM_RESULTS = 50
MAX_PROCESSING_ALGORITHM_RESULTS = 2000
PROCESSING_ALGORITHM_PAGE_SIZE = 50
MAX_PROCESSING_ALGORITHM_CURSOR_LENGTH = 2048
PROCESSING_ALGORITHM_CURSOR_ERROR_PREFIX = (
    "Processing algorithms pagination error:"
)


def encode_cors_origins(origins: Iterable[str]) -> str:
    """Encode CORS origins for transport through the process environment."""

    values = list(origins)
    if any(not isinstance(origin, str) for origin in values):
        raise TypeError("CORS origins must be strings")
    return json.dumps(values, ensure_ascii=False, separators=(",", ":"))


def decode_cors_origins(
    value: str | None,
    *,
    legacy_path_separator: str | None = None,
) -> list[str]:
    """Decode JSON CORS origins, with support for the legacy path separator."""

    if not isinstance(value, str) or not value.strip():
        return []

    text = value.strip()
    try:
        decoded = json.loads(text)
    except json.JSONDecodeError:
        decoded = None
    else:
        if not isinstance(decoded, list) or any(
            not isinstance(origin, str) for origin in decoded
        ):
            return []
        return [origin.strip() for origin in decoded if origin.strip()]

    if text.startswith("["):
        return []
    separator = os.pathsep if legacy_path_separator is None else legacy_path_separator
    if not separator:
        return [text]
    return [origin.strip() for origin in text.split(separator) if origin.strip()]


def qcopilots_home() -> Path:
    """Return the per-user QCopilots state directory."""

    configured = os.environ.get(QCOPILOTS_HOME_ENV)
    if configured:
        return Path(configured).expanduser().resolve()

    appdata = os.environ.get("APPDATA")
    if appdata:
        return Path(appdata) / "QGIS" / "QCopilots"

    return Path.home() / ".qcopilots"


def runtime_root() -> Path:
    return qcopilots_home() / "runtimes"


def logs_root() -> Path:
    return qcopilots_home() / "logs"


def services_state_root() -> Path:
    return qcopilots_home() / "services"
