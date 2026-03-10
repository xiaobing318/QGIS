from __future__ import annotations

"""Public config facade for processingmcpserver.

This module intentionally re-exports the stable config API so other modules
and tests can import from a single location without depending on the loader
module layout.
"""

from processingmcpserver.config_defaults import (
    default_processing_mcp_json_document,
    processing_mcp_config_file_path,
)
from processingmcpserver.config_loader import load_processing_mcp_server_config
from processingmcpserver.config_types import (
    DEFAULT_CORS_ALLOW_HEADERS,
    DEFAULT_CORS_ORIGINS,
    MCP_LOG_CATEGORY,
    SOURCE_DEFAULT,
    SOURCE_JSON,
    SOURCE_SETTINGS,
    ProcessingMCPServerConfig,
    ProcessingMCPServerDependencies,
    ProcessingMCPFilesystemConfig,
)

__all__ = [
    "MCP_LOG_CATEGORY",
    "DEFAULT_CORS_ORIGINS",
    "DEFAULT_CORS_ALLOW_HEADERS",
    "SOURCE_JSON",
    "SOURCE_SETTINGS",
    "SOURCE_DEFAULT",
    "ProcessingMCPServerDependencies",
    "ProcessingMCPFilesystemConfig",
    "ProcessingMCPServerConfig",
    "processing_mcp_config_file_path",
    "default_processing_mcp_json_document",
    "load_processing_mcp_server_config",
]
