"""Processing MCP prompt module."""

from __future__ import annotations

from typing import Any, Callable

PROMPT_NAME = "qgis_shapefile_overlay_clip_stats_workflow"
PROMPT_DOC = (
    "用途：生成面向 shapefile 叠加裁剪与面积统计的执行提示，适用于主体图层与覆盖图层二元分析。 "
    "输入：task_name、subject_shapefile、overlay_shapefile、output_shapefile 为必填；target_crs、area_field、group_field、deliverables 为可选。 "
    "前置条件：processingmcpserver 已注册 prompts/resources/tools，主体与覆盖 shapefile 可被 QGIS 正常加载。 "
    "副作用：无写操作，只返回结构化 prompt 文本。 "
    "安全控制：导出步骤要求 confirm_write=true，覆盖输出时要求 confirm_destructive=true。 "
    "返回结果：返回“叠加裁剪统计”工作流模板，供 llama-server WebUI 的 Use Prompt 直接注入聊天上下文。"
)


def register_prompt(
    prompt_factory: Callable[..., Any],
    workflow_template: dict[str, Any],
    _quality_profile: dict[str, Any],
) -> None:
    """
    作用：注册 `qgis_shapefile_overlay_clip_stats_workflow`，完成当前函数负责的处理步骤并产出结果。
    用途：注册 `qgis_shapefile_overlay_clip_stats_workflow`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP prompt 注册流程中由聚合模块调用，用于挂载单个 prompt 实现。
    参数与返回：
    - 参数 `prompt_factory`（`Callable[..., Any]`）：注册器参数。
    - 参数 `workflow_template`（`dict[str, Any]`）：workflow 资源模板快照。
    - 参数 `_quality_profile`（`dict[str, Any]`）：质量规则快照。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    workflow_defaults = workflow_template.get("defaults", {})
    default_target_crs = str(workflow_defaults.get("default_target_crs") or "EPSG:4490")

    @prompt_factory(description=PROMPT_DOC)
    def qgis_shapefile_overlay_clip_stats_workflow(
        task_name: str,
        subject_shapefile: str,
        overlay_shapefile: str,
        output_shapefile: str,
        target_crs: str = default_target_crs,
        area_field: str = "area_m2",
        group_field: str = "",
        deliverables: str = "标准交付：裁剪结果 shapefile、面积统计摘要、运行摘要。",
    ) -> str:
        """
        作用：处理 `qgis_shapefile_overlay_clip_stats_workflow` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `qgis_shapefile_overlay_clip_stats_workflow` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP prompt 注册与调用流程中被调用，用于返回提示词模板内容。
        参数与返回：
        - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
        - 参数 `subject_shapefile`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `overlay_shapefile`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `output_shapefile`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `target_crs`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `default_target_crs`。
        - 参数 `area_field`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"area_m2"`。
        - 参数 `group_field`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `""`。
        - 参数 `deliverables`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"标准交付：裁剪结果 shapefile、面积统计摘要、运行摘要。"`。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        task_text = task_name.strip() if isinstance(task_name, str) else ""
        subject_text = (
            subject_shapefile.strip()
            if isinstance(subject_shapefile, str)
            else ""
        )
        overlay_text = (
            overlay_shapefile.strip()
            if isinstance(overlay_shapefile, str)
            else ""
        )
        output_text = (
            output_shapefile.strip() if isinstance(output_shapefile, str) else ""
        )
        target_crs_text = (
            target_crs.strip()
            if isinstance(target_crs, str) and target_crs.strip()
            else default_target_crs
        )
        area_field_text = (
            area_field.strip()
            if isinstance(area_field, str) and area_field.strip()
            else "area_m2"
        )
        group_field_text = group_field.strip() if isinstance(group_field, str) else ""
        deliverables_text = (
            deliverables.strip()
            if isinstance(deliverables, str) and deliverables.strip()
            else "标准交付：裁剪结果 shapefile、面积统计摘要、运行摘要。"
        )
        missing_required: list[str] = []
        if not task_text:
            missing_required.append("task_name")
        if not subject_text:
            missing_required.append("subject_shapefile")
        if not overlay_text:
            missing_required.append("overlay_shapefile")
        if not output_text:
            missing_required.append("output_shapefile")
        optional_stats_hint = (
            "group_field 非空时启用 mcp_tools_vector_stats_by_categories；为空时跳过该步骤。"
        )
        return (
            "Structure: Parameters -> 8 Steps -> Branch Rules -> Deliverables\n"
            "Workflow: qgis_shapefile_overlay_clip_stats_workflow\n"
            "Task Parameters / 任务参数:\n"
            f"- task_name: {task_text or '[MISSING]'}\n"
            f"- subject_shapefile: {subject_text or '[MISSING]'}\n"
            f"- overlay_shapefile: {overlay_text or '[MISSING]'}\n"
            f"- output_shapefile: {output_text or '[MISSING]'}\n"
            f"- target_crs: {target_crs_text}\n"
            f"- area_field: {area_field_text}\n"
            f"- group_field: {group_field_text or '(none)'}\n"
            f"- deliverables: {deliverables_text}\n"
            f"- Missing required fields: {', '.join(missing_required) if missing_required else 'none'}\n"
            "1) 输入路径校验与 bundle 检查\n"
            "- Tools: mcp_tools_filesystem_query_entry_info, mcp_tools_dataset_inspect_vector_bundle\n"
            "2) 加载主体与覆盖图层\n"
            "- Tools: mcp_tools_dataset_load_layers\n"
            "3) 双图层标准化到统一 CRS\n"
            "- Tools: mcp_tools_vector_prepare_work_layer\n"
            "- Required options: target_crs=<target_crs>，对 subject/overlay 各执行一次。\n"
            "4) 执行裁剪\n"
            "- Tools: mcp_tools_vector_clip\n"
            "- Inputs: input_layer_ref=<subject_work_layer>, overlay_layer_ref=<overlay_work_layer>\n"
            "5) 计算面积字段\n"
            "- Tools: mcp_tools_vector_table_calculate_field\n"
            "- Required options: field_name=<area_field>, expression=$area\n"
            "6) 可选分组统计\n"
            "- Tools: mcp_tools_vector_stats_by_categories\n"
            f"- Branch rule: {optional_stats_hint}\n"
            "7) 导出输出 shapefile\n"
            "- Tools: mcp_tools_vector_export_dataset\n"
            "- Required options: file_format='shp', output_path=<output_shapefile>, confirm_write=true\n"
            "8) 清理工作层\n"
            "- Tools: mcp_tools_project_cleanup_work_layers\n"
            "Branch Rules / 分支规则:\n"
            "- 当 group_field 为空：只保留面积字段结果，不做分组聚合。\n"
            "- 当 group_field 非空：输出分组统计结果并附在运行摘要。\n"
            "Final Deliverables / 最终交付:\n"
            f"- {deliverables_text}"
        )

    qgis_shapefile_overlay_clip_stats_workflow.__doc__ = PROMPT_DOC

