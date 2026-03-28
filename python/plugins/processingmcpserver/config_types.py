from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

MCP_LOG_CATEGORY = "Processing MCP Server"
DEFAULT_CORS_ORIGINS = (
    "http://127.0.0.1:8080",
    "http://localhost:8080",
    "http://127.0.0.1:8282",
    "http://localhost:8282",
)
DEFAULT_CORS_ALLOW_HEADERS = (
    "mcp-session-id",
    "mcp-protocol-version",
    "last-event-id",
    "authorization",
)
SOURCE_JSON = "JSON"
SOURCE_SETTINGS = "Settings"
SOURCE_DEFAULT = "Default"


@dataclass(frozen=True)
class ProcessingMCPServerDependencies:
    """Describe dependency checking and auto-installation policy."""

    auto_install: bool = True


@dataclass(frozen=True)
class ProcessingMCPServerConfig:
    """Aggregate all configuration needed to start the Processing MCP server."""

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
    enable_execute_code: bool
    dependencies: ProcessingMCPServerDependencies = field(
        default_factory=ProcessingMCPServerDependencies
    )
    config_sources: dict[str, str] = field(default_factory=dict)
    config_file_path: str = ""
