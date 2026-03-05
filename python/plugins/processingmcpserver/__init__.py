"""
processing mcp server plugin entrypoint.
"""

def classFactory(iface):
    """
    作用：返回插件主类实例并交给 QGIS 插件系统管理。
    """
    from processingmcpserver.plugin import ProcessingMCPServerPlugin

    return ProcessingMCPServerPlugin(iface)
