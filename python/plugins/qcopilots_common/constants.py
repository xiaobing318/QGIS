"""Constants shared by QCopilots plugins and MCP services.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import os
from pathlib import Path


MENU_NAME = "&QCopilots"
LOG_CHANNEL = "QCopilots"
MCP_PROTOCOL_VERSION = "2025-06-18"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_MCP_PATH = "/mcp"
DEFAULT_CORS_ORIGINS = ("http://127.0.0.1:8282", "http://localhost:8282")

QCOPILOTS_HOME_ENV = "QCOPILOTS_HOME"
BRIDGE_URL_ENV = "QCOPILOTS_BRIDGE_URL"
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
