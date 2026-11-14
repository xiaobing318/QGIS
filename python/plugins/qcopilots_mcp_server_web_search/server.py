# SPDX-License-Identifier: GPL-2.0-or-later
"""Standalone entry point for the QCopilots WebSearch MCP adapter."""
from __future__ import annotations
import os
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from qcopilots_common.constants import DEFAULT_SERVICE_PORTS
from qcopilots_common.logging import configure_logger
from qcopilots_common.mcp_http import run_mcp_server
from qcopilots_mcp_server_web_search.backend_client import BackendClient
from qcopilots_mcp_server_web_search.tools import build_web_search_tools

SERVICE_ID = "qcopilots.mcp_server_web_search"


def main() -> None:
    logger = configure_logger(SERVICE_ID, os.environ.get("QCOPILOTS_LOG_FILE"))
    client = BackendClient()
    try:
        run_mcp_server(name="QCopilots MCP Server Web Search", version="0.1.0",
            tools=build_web_search_tools(client), default_port=DEFAULT_SERVICE_PORTS[SERVICE_ID],
            description="Public web search, reading and find through an independent native backend.", logger=logger)
    finally:
        client.close()


if __name__ == "__main__":
    main()
