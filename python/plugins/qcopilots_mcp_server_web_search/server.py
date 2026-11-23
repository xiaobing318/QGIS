# SPDX-License-Identifier: GPL-2.0-or-later
"""Standalone entry point for the QCopilots WebSearch MCP adapter."""
from __future__ import annotations
import os
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from qcopilots_common.constants import DEFAULT_SERVICE_PORTS, MCP_AUTH_TOKEN_ENV, CORS_ORIGINS_ENV, decode_cors_origins
from qcopilots_common.logging import configure_logger
from qcopilots_common.mcp_http import parse_server_args
from qcopilots_mcp_server_web_search.relay_server import WebSearchRelayServer

SERVICE_ID = "qcopilots.mcp_server_web_search"


def main() -> None:
    logger = configure_logger(SERVICE_ID, os.environ.get("QCOPILOTS_LOG_FILE"))
    args = parse_server_args(DEFAULT_SERVICE_PORTS[SERVICE_ID], "QCopilots WebSearch MCP relay")
    if args.log_file:
        logger = configure_logger(SERVICE_ID, args.log_file)
    server = WebSearchRelayServer(host=args.host, port=args.port, path=args.path,
        auth_token=os.environ.get(MCP_AUTH_TOKEN_ENV, ""),
        cors_origins=decode_cors_origins(os.environ.get(CORS_ORIGINS_ENV, "[]")), logger=logger)
    server.serve_forever()


if __name__ == "__main__":
    main()
