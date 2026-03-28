"""Processing MCP server test package."""


def run_from_qgis_console(*args, **kwargs):
    """
    作用：运行 `from_qgis_console`，完成当前函数负责的处理步骤并产出结果。
    用途：运行 `from_qgis_console`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
    参数与返回：
    - 参数 `*args`：可变位置参数，按调用时顺序传入补充数据。
    - 参数 `**kwargs`：可变关键字参数，用于扩展命名输入。
    - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
    返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
    """
    from .suite_runner import run_from_qgis_console as _runner

    return _runner(*args, **kwargs)


__all__ = ["run_from_qgis_console"]
