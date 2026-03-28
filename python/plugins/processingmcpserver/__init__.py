"""Processing MCP server plugin entrypoint."""

from pathlib import Path


def classFactory(iface):
    """Return the plugin instance managed by the QGIS plugin system."""
    from processingmcpserver.plugin import ProcessingMCPServerPlugin

    return ProcessingMCPServerPlugin(iface)


def write_mcp_capabilities_markdown(output_path: str | Path = "") -> Path:
    """Export the current processingmcpserver tools, prompts, and resources docs."""
    from processingmcpserver.capabilities_markdown import (
        write_mcp_capabilities_markdown as _write_markdown,
    )

    return _write_markdown(output_path)


__all__ = ["classFactory", "write_mcp_capabilities_markdown"]
