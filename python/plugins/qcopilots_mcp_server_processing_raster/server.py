"""Command line entry point for QCopilots raster Processing MCP service.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from qcopilots_common.constants import DEFAULT_SERVICE_PORTS
from qcopilots_common.logging import configure_logger
from qcopilots_common.mcp_http import run_mcp_server
from qcopilots_common.processing_tools import build_processing_tools


SERVICE_ID = "qcopilots.mcp_server_processing_raster"


def main() -> None:
    logger = configure_logger(SERVICE_ID, os.environ.get("QCOPILOTS_LOG_FILE"))
    run_mcp_server(
        name="QCopilots MCP Server Processing Raster",
        version="0.1.0",
        tools=build_processing_tools("raster"),
        default_port=DEFAULT_SERVICE_PORTS[SERVICE_ID],
        description="QCopilots raster Processing MCP service.",
        logger=logger,
    )


if __name__ == "__main__":
    main()
