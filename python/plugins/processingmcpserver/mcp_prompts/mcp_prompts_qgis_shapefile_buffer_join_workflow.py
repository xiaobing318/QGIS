"""Processing MCP prompt module."""

from __future__ import annotations

from typing import Any, Callable

PROMPT_NAME = "qgis_shapefile_buffer_join_workflow"
PROMPT_DOC = (
    "用途：生成面向 shapefile 线缓冲区与点空间连接分析的执行提示，适用于设施服务范围与点位关联场景。 "
    "输入：task_name、line_shapefile、point_shapefile、output_shapefile、buffer_distance 为必填；target_crs、predicate_codes、point_category_field、deliverables 为可选。 "
    "前置条件：processingmcpserver 已注册 prompts/resources/tools，输入数据可被 QGIS 正常加载。 "
    "副作用：无写操作，只返回结构化 prompt 文本。 "
    "安全控制：导出步骤要求 confirm_write=true，覆盖输出时要求 confirm_destructive=true。 "
    "返回结果：返回“缓冲连接分析”工作流模板，供 llama-server WebUI 的 Use Prompt 直接注入聊天上下文。"
)


def register_prompt(
    prompt_factory: Callable[..., Any],
    workflow_template: dict[str, Any],
    _quality_profile: dict[str, Any],
) -> None:
    """
    作用：注册 `qgis_shapefile_buffer_join_workflow`，完成当前函数负责的处理步骤并产出结果。
    用途：注册 `qgis_shapefile_buffer_join_workflow`，完成当前函数负责的处理步骤并产出结果。
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
    def qgis_shapefile_buffer_join_workflow(
        task_name: str,
        line_shapefile: str,
        point_shapefile: str,
        output_shapefile: str,
        buffer_distance: float,
        target_crs: str = default_target_crs,
        predicate_codes: str = "0",
        point_category_field: str = "",
        deliverables: str = "标准交付：缓冲连接结果 shapefile、统计摘要、运行摘要。",
    ) -> str:
        """
        作用：处理 `qgis_shapefile_buffer_join_workflow` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `qgis_shapefile_buffer_join_workflow` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP prompt 注册与调用流程中被调用，用于返回提示词模板内容。
        参数与返回：
        - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
        - 参数 `line_shapefile`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `point_shapefile`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `output_shapefile`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `buffer_distance`（`float`）：数值控制参数，用于限制范围、数量或时限。
        - 参数 `target_crs`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `default_target_crs`。
        - 参数 `predicate_codes`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"0"`。
        - 参数 `point_category_field`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `""`。
        - 参数 `deliverables`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"标准交付：缓冲连接结果 shapefile、统计摘要、运行摘要。"`。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        task_text = task_name.strip() if isinstance(task_name, str) else ""
        line_text = line_shapefile.strip() if isinstance(line_shapefile, str) else ""
        point_text = point_shapefile.strip() if isinstance(point_shapefile, str) else ""
        output_text = (
            output_shapefile.strip() if isinstance(output_shapefile, str) else ""
        )
        target_crs_text = (
            target_crs.strip()
            if isinstance(target_crs, str) and target_crs.strip()
            else default_target_crs
        )
        predicate_codes_text = (
            predicate_codes.strip()
            if isinstance(predicate_codes, str) and predicate_codes.strip()
            else "0"
        )
        point_category_text = (
            point_category_field.strip() if isinstance(point_category_field, str) else ""
        )
        deliverables_text = (
            deliverables.strip()
            if isinstance(deliverables, str) and deliverables.strip()
            else "标准交付：缓冲连接结果 shapefile、统计摘要、运行摘要。"
        )
        missing_required: list[str] = []
        if not task_text:
            missing_required.append("task_name")
        if not line_text:
            missing_required.append("line_shapefile")
        if not point_text:
            missing_required.append("point_shapefile")
        if not output_text:
            missing_required.append("output_shapefile")
        predicate_list = [
            item.strip()
            for item in predicate_codes_text.replace(";", ",").split(",")
            if item.strip()
        ]
        predicate_display = ", ".join(predicate_list) if predicate_list else "0"
        stats_branch = (
            "point_category_field 非空时执行 mcp_tools_vector_stats_by_categories；"
            "为空时执行 mcp_tools_vector_table_query_records 抽样校验。"
        )
        return (
            "Structure: Parameters -> 8 Steps -> Branch Rules -> Deliverables\n"
            "Workflow: qgis_shapefile_buffer_join_workflow\n"
            "Task Parameters / 任务参数:\n"
            f"- task_name: {task_text or '[MISSING]'}\n"
            f"- line_shapefile: {line_text or '[MISSING]'}\n"
            f"- point_shapefile: {point_text or '[MISSING]'}\n"
            f"- output_shapefile: {output_text or '[MISSING]'}\n"
            f"- buffer_distance: {buffer_distance}\n"
            f"- target_crs: {target_crs_text}\n"
            f"- predicate_codes: {predicate_display}\n"
            f"- point_category_field: {point_category_text or '(none)'}\n"
            f"- deliverables: {deliverables_text}\n"
            f"- Missing required fields: {', '.join(missing_required) if missing_required else 'none'}\n"
            "1) 输入路径与 bundle 检查\n"
            "- Tools: mcp_tools_filesystem_query_entry_info, mcp_tools_dataset_inspect_vector_bundle\n"
            "2) 加载线/点图层\n"
            "- Tools: mcp_tools_dataset_load_layers\n"
            "3) 双图层标准化\n"
            "- Tools: mcp_tools_vector_prepare_work_layer\n"
            "- Required options: target_crs=<target_crs>，line/point 各执行一次。\n"
            "4) 线图层缓冲区计算\n"
            "- Tools: mcp_tools_vector_buffer\n"
            "- Required options: distance=<buffer_distance>\n"
            "5) 缓冲区与点图层空间连接\n"
            "- Tools: mcp_tools_vector_join_attributes_by_location\n"
            "- Required options: predicate=<predicate_codes>\n"
            "6) 结果验证与统计分支\n"
            "- Tools: mcp_tools_vector_stats_by_categories, mcp_tools_vector_table_query_records\n"
            f"- Branch rule: {stats_branch}\n"
            "7) 导出输出 shapefile\n"
            "- Tools: mcp_tools_vector_export_dataset\n"
            "- Required options: file_format='shp', output_path=<output_shapefile>, confirm_write=true\n"
            "8) 清理工作层\n"
            "- Tools: mcp_tools_project_cleanup_work_layers\n"
            "Branch Rules / 分支规则:\n"
            "- 统计分支：point_category_field 非空 -> 分类统计；为空 -> 抽样记录校验。\n"
            "- 谓词分支：predicate_codes 可传逗号分隔整数码，默认 0（intersects）。\n"
            "Final Deliverables / 最终交付:\n"
            f"- {deliverables_text}"
        )

    qgis_shapefile_buffer_join_workflow.__doc__ = PROMPT_DOC

