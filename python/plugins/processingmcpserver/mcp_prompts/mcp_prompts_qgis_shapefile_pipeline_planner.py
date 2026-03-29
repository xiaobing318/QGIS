"""Processing MCP prompt module."""

from __future__ import annotations

from typing import Any, Callable

PROMPT_NAME = "qgis_shapefile_pipeline_planner"
PROMPT_DOC = (
    "用途：生成面向 shapefile 的六阶段执行计划提示，约束模型按固定阶段完成路径检查、筛选、预检、统计、标准化、处理、导出和清理。 "
    "输入：task_name、input_dir、output_dir 为必填参数；quality_rule_resource 指向质量规则资源；deliverables 描述交付物说明。 "
    "前置条件：processingmcpserver 已注册 prompts/resources/tools，且建议同时挂载 qgis://workflow/shapefile/template 与 qgis://workflow/shapefile/quality-profile/default。 "
    "副作用：无写操作，只返回结构化 prompt 文本。 "
    "安全控制：无。 "
    "返回结果：返回六阶段中文主导提示模板，供 llama-server WebUI 的 Use Prompt 直接注入聊天上下文。"
)


def register_prompt(
    prompt_factory: Callable[..., Any],
    workflow_template: dict[str, Any],
    quality_profile: dict[str, Any],
) -> None:
    """
    作用：注册 `qgis_shapefile_pipeline_planner`，完成当前函数负责的处理步骤并产出结果。
    用途：注册 `qgis_shapefile_pipeline_planner`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP prompt 注册流程中由聚合模块调用，用于挂载单个 prompt 实现。
    参数与返回：
    - 参数 `prompt_factory`（`Callable[..., Any]`）：注册器参数。
    - 参数 `workflow_template`（`dict[str, Any]`）：workflow 资源模板快照。
    - 参数 `quality_profile`（`dict[str, Any]`）：质量规则快照。
    - 返回：无返回值。
    返回结果：无返回值。
    """
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

    @prompt_factory(description=PROMPT_DOC)
    def qgis_shapefile_pipeline_planner(
        task_name: str,
        input_dir: str,
        output_dir: str,
        quality_rule_resource: str = "qgis://workflow/shapefile/quality-profile/default",
        deliverables: str = "标准交付：输出 shapefile bundle、质量结果、运行摘要。",
    ) -> str:
        """
        作用：处理 `qgis_shapefile_pipeline_planner` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `qgis_shapefile_pipeline_planner` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP prompt 注册与调用流程中被调用，用于返回提示词模板内容。
        参数与返回：
        - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
        - 参数 `input_dir`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `output_dir`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `quality_rule_resource`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"qgis://workflow/shapefile/quality-profile/default"`。
        - 参数 `deliverables`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"标准交付：输出 shapefile bundle、质量结果、运行摘要。"`。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
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

    qgis_shapefile_pipeline_planner.__doc__ = PROMPT_DOC

