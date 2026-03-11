"""
processing mcp server plugin entrypoint.
"""

from pathlib import Path


def classFactory(iface):
    """
    作用：返回插件主类实例并交给 QGIS 插件系统管理。
    """
    from processingmcpserver.plugin import ProcessingMCPServerPlugin

    return ProcessingMCPServerPlugin(iface)


def write_mcp_capabilities_markdown(output_path: str | Path = "") -> Path:
    """
    作用：导出当前 processingmcpserver 的 tools/prompts/resources 能力文档。
    """
    from processingmcpserver.capabilities_markdown import (
        write_mcp_capabilities_markdown as _write_markdown,
    )

    return _write_markdown(output_path)


__all__ = ["classFactory", "write_mcp_capabilities_markdown"]
