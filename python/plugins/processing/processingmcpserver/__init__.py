"""
1. 总结：当前文件是 processingmcpserver 包的入口点，该模块负责导入和暴露 Processing MCP Server 的主要类和功能。
2. 详细说明：
   - 从 processing.processingmcpserver.server 模块中导入了 ProcessingMCPServer 类。
   - 最后，使用 __all__ 列表声明了该包对外暴露的 API，明确指出 ProcessingMCPServer 是该包的主要导出对象。
"""

# 从指定文件模块中导入 ProcessingMCPServer 类。
from processing.processingmcpserver.server import ProcessingMCPServer
# 通过 __all__ 列表声明该包对外暴露的 API。
__all__ = ["ProcessingMCPServer"]
