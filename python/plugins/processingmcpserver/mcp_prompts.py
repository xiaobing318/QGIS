from __future__ import annotations

from typing import Any

REGISTERED_PROMPT_NAMES: tuple[str, ...] = (
    "qgis_task_planner",
    "qgis_layer_health_check",
)


def register_prompts(mcp: Any, tools: Any) -> None:
    _ = tools
    prompt_factory = getattr(mcp, "prompt", None)
    if not callable(prompt_factory):
        return

    @prompt_factory()
    def qgis_task_planner(task: str = "", constraints: str = "") -> str:
        task_text = task.strip() if isinstance(task, str) and task.strip() else "N/A"
        constraints_text = constraints.strip() if isinstance(constraints, str) and constraints.strip() else "None"
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

    @prompt_factory()
    def qgis_layer_health_check(task: str = "", layer_ref: str = "") -> str:
        target = layer_ref.strip() if isinstance(layer_ref, str) and layer_ref.strip() else task.strip()
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
