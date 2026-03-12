from __future__ import annotations

from typing import Any

REGISTERED_PROMPT_NAMES: tuple[str, ...] = ("qgis_shapefile_pipeline_planner",)


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
    "qgis_shapefile_pipeline_planner": _prompt_doc(
        "生成面向 shapefile 的六阶段执行计划提示，约束模型按固定阶段完成路径检查、筛选、预检、统计、标准化、处理、导出和清理。",
        "task_name、input_dir、output_dir 为必填参数；quality_rule_resource 指向质量规则资源；deliverables 描述交付物说明。",
        "processingmcpserver 已注册 prompts/resources/tools，且建议同时挂载 qgis://workflow/shapefile/template 与 qgis://workflow/shapefile/quality-profile/default。",
        "无写操作，只返回结构化 prompt 文本。",
        "无。",
        "返回六阶段中文主导提示模板，供 llama-server WebUI 的 Use Prompt 直接注入聊天上下文。",
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
    prompt_factory = getattr(mcp, "prompt", None)
    if not callable(prompt_factory):
        return

    workflow_template: dict[str, Any] = {}
    quality_profile: dict[str, Any] = {}
    if hasattr(tools, "get_shapefile_workflow_template") and callable(
        tools.get_shapefile_workflow_template
    ):
        try:
            workflow_template = tools.get_shapefile_workflow_template()
        except Exception:
            workflow_template = {}
    if hasattr(tools, "get_shapefile_quality_profile") and callable(
        tools.get_shapefile_quality_profile
    ):
        try:
            quality_profile = tools.get_shapefile_quality_profile()
        except Exception:
            quality_profile = {}

    qgis_shapefile_pipeline_planner_description = _REGISTERED_PROMPT_DOCSTRINGS[
        "qgis_shapefile_pipeline_planner"
    ]
    workflow_stages = workflow_template.get(
        "workflow_stages",
        [
            "path_check_and_geometry_filter",
            "precheck_and_pre_statistics",
            "standardize_crs",
            "data_processing",
            "export",
            "cleanup",
        ],
    )
    workflow_defaults = workflow_template.get("defaults", {})
    default_target_crs = str(workflow_defaults.get("default_target_crs") or "EPSG:4490")
    default_geometry_filter = str(
        workflow_defaults.get("default_geometry_filter") or "all"
    )
    quality_checks = quality_profile.get("quality_checks", [])

    @prompt_factory(description=qgis_shapefile_pipeline_planner_description)
    def qgis_shapefile_pipeline_planner(
        task_name: str,
        input_dir: str,
        output_dir: str,
        quality_rule_resource: str = "qgis://workflow/shapefile/quality-profile/default",
        deliverables: str = "标准交付：输出 shapefile bundle、质量结果、运行摘要。",
    ) -> str:
        """执行 QGIS shapefile pipeline planner 相关逻辑。"""
        task_text = task_name.strip() if isinstance(task_name, str) else ""
        input_text = input_dir.strip() if isinstance(input_dir, str) else ""
        output_text = output_dir.strip() if isinstance(output_dir, str) else ""
        quality_rule_text = (
            quality_rule_resource.strip()
            if isinstance(quality_rule_resource, str) and quality_rule_resource.strip()
            else "qgis://workflow/shapefile/quality-profile/default"
        )
        deliverables_text = (
            deliverables.strip()
            if isinstance(deliverables, str) and deliverables.strip()
            else "标准交付：输出 shapefile bundle、质量结果、运行摘要。"
        )
        missing_required: list[str] = []
        if not task_text:
            missing_required.append("task_name")
        if not input_text:
            missing_required.append("input_dir")
        if not output_text:
            missing_required.append("output_dir")
        quality_checks_text = ", ".join(str(item) for item in quality_checks)
        if not quality_checks_text:
            quality_checks_text = "CRS、invalid geometry、duplicate geometry、UTF-8 编码、schema 约束"
        return (
            "Structure: Parameters -> 6 Stages -> Quality Rules -> Deliverables\n"
            f"Task Parameters / 任务参数:\n"
            f"- task_name: {task_text or '[MISSING]'}\n"
            f"- input_dir: {input_text or '[MISSING]'}\n"
            f"- output_dir: {output_text or '[MISSING]'}\n"
            f"- quality_rule_resource: {quality_rule_text}\n"
            f"- deliverables: {deliverables_text}\n"
            f"- Ordered stages: {', '.join(str(item) for item in workflow_stages)}\n"
            f"- Missing required fields: {', '.join(missing_required) if missing_required else 'none'}\n"
            "1) 路径检查与数据范围确认阶段 / Path Check & Geometry Filter\n"
            "- Goal: 明确 input_dir/output_dir，并确认本次任务处理点、线、面还是全部数据。\n"
            f"- Checks: 校验路径存在性与有效性；若无明确筛选条件，默认 geometry_filter={default_geometry_filter}。\n"
            "- Blocking: 任一必填路径缺失或无效时阻断；筛选结果为空时给出 warning。\n"
            "- Tools: filesystem_query_entry_info, filesystem_stats_directory, dataset_list_files, dataset_load_from_directory\n"
            "- Output: path_and_scope_result。\n"
            "2) 预检与基线统计阶段 / Precheck & Baseline Stats\n"
            "- Goal: 完成文件完整性、可打开性、几何质量、编码风险检查，并建立基础统计基线。\n"
            "- Checks: shapefile bundle 完整性、可打开性、invalid geometry、duplicate geometry、UTF-8 风险、文件大小与数量。\n"
            "- Blocking: 缺失必要 sidecar、无法打开、存在阻断级几何问题时阻断。\n"
            "- Tools: dataset_inspect_shapefile_bundle, vector_check_validity_report, filesystem_stats_directory\n"
            "- Output: precheck_baseline_report。\n"
            "3) 数据标准化阶段 / CRS Standardization\n"
            "- Goal: 统一坐标系统并准备标准化工作层。\n"
            f"- Checks: 若用户未指定目标 CRS，默认转换为 {default_target_crs}。\n"
            "- Blocking: 目标 CRS 非法或转换失败时阻断。\n"
            "- Tools: vector_prepare_work_layer\n"
            "- Output: standardized_work_layers。\n"
            "4) 数据处理阶段 / Data Processing\n"
            "- Goal: 执行字段增删改查、缓冲区、面积字段等业务处理。\n"
            "- Checks: 参数模板合法、算法输入输出匹配、字段变更符合用户意图。\n"
            "- Blocking: 算法执行失败且无法修复时阻断。\n"
            "- Tools: processing_get_parameter_template, processing_execute_algorithm, processing_execute_on_layers, vector_table_*\n"
            "- Output: processed_work_layers。\n"
            "5) 数据处理后导出阶段 / Export\n"
            "- Goal: 将处理结果正式导出到 output_dir。\n"
            "- Checks: output_dir 明确、文件名策略明确、覆盖策略明确，并结合质量规则资源确认导出约束。\n"
            "- Blocking: output_dir 缺失、写盘确认不足、覆盖确认不足时阻断。\n"
            "- Tools: vector_check_validity_report, vector_export_shapefile\n"
            "- Output: exported_shapefile_bundle。\n"
            "6) 数据处理后清理阶段 / Cleanup\n"
            "- Goal: 清理中间层、临时层和加载的输入层。\n"
            "- Checks: 清理清单完整，必要时记录移除结果与剩余风险。\n"
            "- Blocking: 清理失败时记录 warning，不回滚已完成导出。\n"
            "- Tools: project_cleanup_work_layers, layer_remove\n"
            "- Output: cleanup_report。\n"
            "Quality Rules / 质量规则:\n"
            f"- quality_rule_resource: {quality_rule_text}\n"
            f"- quality_checks: {quality_checks_text}\n"
            "Final Deliverables / 最终交付:\n"
            f"- {deliverables_text}"
        )

    qgis_shapefile_pipeline_planner.__doc__ = qgis_shapefile_pipeline_planner_description
