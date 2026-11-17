# SPDX-License-Identifier: GPL-2.0-or-later
"""MCP bindings for the versioned native WebSearch contract."""
from __future__ import annotations
import json
from pathlib import Path
from qcopilots_common.mcp_http import McpTool
from .backend_client import BackendClient, BackendError

_OPERATIONS = {"web_search": "search", "web_open": "open", "web_find": "find", "web_search_status": "status"}


def _result(value, *, error=False):
    return {"content": [{"type": "text", "text": json.dumps(value, ensure_ascii=False, allow_nan=False)}],
            "structuredContent": value, "isError": error}


def _handler(client, name, operation):
    def invoke(arguments):
        # The contract schema has no top-level oneOf, which several model APIs
        # reject, so the url/page_id choice is checked here before any request.
        if name == "web_open" and isinstance(arguments, dict) and ("url" in arguments) == ("page_id" in arguments):
            return _result({"error": {"code": "invalid_argument", "message": "Supply exactly one of url or page_id.",
                                      "retryable": False}}, error=True)
        try:
            return _result(client.call(operation, arguments))
        except BackendError as error:
            return _result({"error": {"code": error.code, "message": error.safe_message,
                                      "retryable": error.retryable}}, error=True)
    return invoke


def build_web_search_tools(client=None, contracts_root=None):
    client = client if client is not None else BackendClient()
    root = Path(contracts_root) if contracts_root is not None else Path(__file__).resolve().parent
    # tools.json and the schemas are identical copies of the native contracts/v1 files.
    catalog = json.loads((root / "tools.json").read_text(encoding="utf-8"))
    tools = []
    for entry in catalog["tools"]:
        name = entry["name"]
        tools.append(McpTool(name=name, description=entry["description"],
            input_schema=json.loads((root / f"{name}.schema.json").read_text(encoding="utf-8")),
            handler=_handler(client, name, _OPERATIONS[name])))
    return tools
