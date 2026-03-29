"""Processing MCP resources package entrypoint."""

from .mcp_resources import (
    REGISTERED_RESOURCE_URIS,
    RESOURCE_SCHEMA_VERSION,
    _REGISTERED_RESOURCE_DOCSTRINGS,
    register_resources,
)

__all__ = [
    "REGISTERED_RESOURCE_URIS",
    "RESOURCE_SCHEMA_VERSION",
    "_REGISTERED_RESOURCE_DOCSTRINGS",
    "register_resources",
]

