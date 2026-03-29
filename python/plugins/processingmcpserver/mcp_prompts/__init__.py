"""Processing MCP prompts package entrypoint."""

from .mcp_prompts import (
    REGISTERED_PROMPT_NAMES,
    _REGISTERED_PROMPT_DOCSTRINGS,
    register_prompts,
)

__all__ = [
    "REGISTERED_PROMPT_NAMES",
    "_REGISTERED_PROMPT_DOCSTRINGS",
    "register_prompts",
]

