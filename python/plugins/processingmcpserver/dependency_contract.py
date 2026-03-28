from __future__ import annotations

import inspect


def _validate_mcp_runtime_contract() -> tuple[bool, str]:
    """
    作用：封装内部辅助步骤 `_validate_mcp_runtime_contract`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_validate_mcp_runtime_contract`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 processingmcpserver 插件运行流程中被调用，用于支撑当前模块的业务处理。
    参数与返回：
    - 参数：无。
    - 返回：返回 `tuple[bool, str]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[bool, str]` 类型结果，返回值语义遵循该函数实现约定。
    """
    try:
        from mcp.server.fastmcp import FastMCP
    except Exception as exc:
        return False, f"Failed to import FastMCP from mcp.server.fastmcp: {exc}"

    required_init_params = (
        "host",
        "port",
        "mount_path",
        "sse_path",
        "message_path",
        "streamable_http_path",
        "stateless_http",
        "json_response",
        "log_level",
    )

    try:
        init_signature = inspect.signature(FastMCP.__init__)
    except Exception as exc:
        return False, f"Failed to inspect FastMCP.__init__ signature: {exc}"

    parameters = init_signature.parameters
    supports_var_keyword = any(
        parameter.kind == inspect.Parameter.VAR_KEYWORD
        for parameter in parameters.values()
    )
    missing_init_params = [
        name
        for name in required_init_params
        if name not in parameters and not supports_var_keyword
    ]
    if missing_init_params:
        return (
            False,
            "FastMCP.__init__ does not support required parameters: "
            + ", ".join(missing_init_params),
        )

    missing_methods: list[str] = []
    for method_name in ("streamable_http_app", "sse_app"):
        method = getattr(FastMCP, method_name, None)
        if method is None or not callable(method):
            missing_methods.append(method_name)
    if missing_methods:
        return (
            False,
            "FastMCP is missing required callable members: "
            + ", ".join(missing_methods),
        )

    return True, ""
