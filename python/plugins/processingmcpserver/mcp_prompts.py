from __future__ import annotations

from typing import Any

REGISTERED_PROMPT_NAMES: tuple[str, ...] = (
    "qgis_task_planner",
    "qgis_layer_health_check",
)


def _prompt_doc(
    purpose: str,
    inputs: str,
    preconditions: str,
    effects: str,
    safety: str,
    returns: str,
) -> str:
    """执行 prompt doc 相关逻辑。"""
    return (
        f"用途：{purpose} 输入语义：{inputs} 前置条件：{preconditions} "
        f"主要副作用：{effects} 安全开关：{safety} 返回结果：{returns}"
    )


_REGISTERED_PROMPT_DOCSTRINGS: dict[str, str] = {
    "qgis_task_planner": _prompt_doc(
        "生成适合 llama-server WebUI 使用的 QGIS 任务规划提示模板，帮助模型先拆解任务再调用 tools。",
        "task 描述任务目标，constraints 描述输出格式、限制条件或交付要求。",
        "processingmcpserver 已注册 prompts 能力，且建议同时挂载 qgis://project/info 与 qgis://project/layers/summary。",
        "无写操作，只返回结构化 prompt 文本。",
        "无。",
        "返回 Goal/Inputs/Tool Chain/Key Params/Validation/Deliverables 六段式双语提示模板。",
    ),
    "qgis_layer_health_check": _prompt_doc(
        "生成图层健康检查提示模板，引导模型按固定步骤检查图层状态、字段和样本记录。",
        "task 可作为兜底目标描述，layer_ref 用于指定要检查的图层引用。",
        "processingmcpserver 已注册 prompts 能力，且目标图层最好已加载到当前工程。",
        "无写操作，只返回结构化 prompt 文本。",
        "无。",
        "返回面向图层体检的双语提示模板，覆盖 layer tree、details、features 和 records 查询链路。",
    ),
}


def _validate_registered_prompt_docstrings() -> None:
    """校验 registered prompt docstrings。"""
    missing: list[str] = []
    invalid: list[str] = []
    extra = sorted(set(_REGISTERED_PROMPT_DOCSTRINGS) - set(REGISTERED_PROMPT_NAMES))
    for prompt_name in REGISTERED_PROMPT_NAMES:
        description = _REGISTERED_PROMPT_DOCSTRINGS.get(prompt_name)
        if not isinstance(description, str):
            missing.append(prompt_name)
            continue
        if not description.strip():
            invalid.append(prompt_name)
    if missing or invalid or extra:
        raise RuntimeError(
            "Failed to initialize registered MCP prompt docstrings: "
            f"missing={missing}, invalid={invalid}, extra={extra}"
        )


_validate_registered_prompt_docstrings()


def register_prompts(mcp: Any, tools: Any) -> None:
    """注册 prompts 能力。"""
    _ = tools
    prompt_factory = getattr(mcp, "prompt", None)
    if not callable(prompt_factory):
        return

    qgis_task_planner_description = _REGISTERED_PROMPT_DOCSTRINGS["qgis_task_planner"]

    @prompt_factory(description=qgis_task_planner_description)
    def qgis_task_planner(task: str = "", constraints: str = "") -> str:
        """执行 QGIS task planner 相关逻辑。"""
        task_text = task.strip() if isinstance(task, str) and task.strip() else "N/A"
        constraints_text = (
            constraints.strip() if isinstance(constraints, str) and constraints.strip() else "None"
        )
        return (
            "Structure: Goal -> Inputs -> Tool Chain -> Key Params -> Validation -> Deliverables\n"
            "Goal / 目标:\n"
            f"- Task: {task_text}\n"
            "Inputs / 输入:\n"
            "- Required layers, CRS, and destination format.\n"
            "- Current project state from qgis://project/info and qgis://project/layers/summary.\n"
            "Tool Chain / 工具链:\n"
            "- common_get_qgis_info\n"
            "- layer_list / layer_get_details / layer_resolve_references\n"
            "- processing_get_parameter_template / processing_execute_algorithm / processing_execute_on_layers\n"
            "Key Params / 关键参数:\n"
            f"- Constraints: {constraints_text}\n"
            "- Explicitly state OUTPUT destination policy and IN_PLACE policy.\n"
            "Validation / 验证:\n"
            "- Check counts, schema, and key metrics after each step.\n"
            "Deliverables / 交付物:\n"
            "- Executable steps, final outputs, and verification checklist."
        )

    qgis_task_planner.__doc__ = qgis_task_planner_description

    qgis_layer_health_check_description = _REGISTERED_PROMPT_DOCSTRINGS[
        "qgis_layer_health_check"
    ]

    @prompt_factory(description=qgis_layer_health_check_description)
    def qgis_layer_health_check(task: str = "", layer_ref: str = "") -> str:
        """执行 QGIS layer health check 相关逻辑。"""
        target = (
            layer_ref.strip() if isinstance(layer_ref, str) and layer_ref.strip() else task.strip()
        )
        target_text = target or "current project layers"
        return (
            "Structure: Goal -> Inputs -> Tool Chain -> Key Params -> Validation -> Deliverables\n"
            f"Goal / 目标: Inspect layer health for {target_text}\n"
            "Inputs / 输入:\n"
            "- Layer reference or project-wide inspection.\n"
            "Tool Chain / 工具链:\n"
            "- layer_get_panel_tree\n"
            "- layer_get_details\n"
            "- vector_get_layer_features\n"
            "- vector_table_query_records\n"
            "Key Params / 关键参数:\n"
            "- Verify CRS, geometry type, field schema, and sampled records.\n"
            "Validation / 验证:\n"
            "- Confirm no missing source, invalid geometry, or empty critical fields.\n"
            "Deliverables / 交付物:\n"
            "- Risk list, recommended fixes, and follow-up tool calls."
        )

    qgis_layer_health_check.__doc__ = qgis_layer_health_check_description
