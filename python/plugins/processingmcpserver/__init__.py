"""Processing MCP server plugin entrypoint."""

from pathlib import Path


def classFactory(iface):
    """
    作用：处理 `classFactory` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `classFactory` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在插件加载与导出辅助接口时被调用，用于对外提供入口函数。
    参数与返回：
    - 参数 `iface`：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
    返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
    """
    from processingmcpserver.plugin import ProcessingMCPServerPlugin

    return ProcessingMCPServerPlugin(iface)


def write_mcp_capabilities_markdown(output_path: str | Path = "") -> Path:
    """
    作用：写入 `mcp_capabilities_markdown`，完成当前函数负责的处理步骤并产出结果。
    用途：写入 `mcp_capabilities_markdown`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在插件加载与导出辅助接口时被调用，用于对外提供入口函数。
    参数与返回：
    - 参数 `output_path`（`str | Path`）：路径类参数，用于定位输入或输出文件系统位置。 默认值为 `""`。
    - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    """
    from processingmcpserver.capabilities_markdown import (
        write_mcp_capabilities_markdown as _write_markdown,
    )

    return _write_markdown(output_path)


__all__ = ["classFactory", "write_mcp_capabilities_markdown"]
