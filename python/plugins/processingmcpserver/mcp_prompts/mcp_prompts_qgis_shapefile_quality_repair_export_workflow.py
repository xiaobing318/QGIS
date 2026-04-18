"""Processing MCP prompt module."""

from __future__ import annotations

from typing import Any, Callable

PROMPT_NAME = "qgis_shapefile_quality_repair_export_workflow"
PROMPT_DOC = (
    "用途：生成面向 shapefile 批量质检、修复、标准化并导出的执行提示，适用于目录级批处理。 "
    "输入：task_name、input_dir、output_dir 为必填；target_crs、geometry_type、name_glob、required_fields、deliverables 为可选。 "
    "前置条件：processingmcpserver 已注册 prompts/resources/tools，且建议同步读取 qgis://workflow/shapefile/quality-profile/default。 "
    "副作用：无写操作，只返回结构化 prompt 文本。 "
    "安全控制：导出/清理步骤要求显式确认 confirm_write，覆盖或删除时要求 confirm_destructive。 "
    "返回结果：返回“质检修复导出”工作流模板，供 llama-server WebUI 的 Use Prompt 直接注入聊天上下文。"
)


def register_prompt(
    prompt_factory: Callable[..., Any],
    workflow_template: dict[str, Any],
    quality_profile: dict[str, Any],
) -> None:
    """
    作用：注册 `qgis_shapefile_quality_repair_export_workflow`，完成当前函数负责的处理步骤并产出结果。
    用途：注册 `qgis_shapefile_quality_repair_export_workflow`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP prompt 注册流程中由聚合模块调用，用于挂载单个 prompt 实现。
    参数与返回：
    - 参数 `prompt_factory`（`Callable[..., Any]`）：注册器参数。
    - 参数 `workflow_template`（`dict[str, Any]`）：workflow 资源模板快照。
    - 参数 `quality_profile`（`dict[str, Any]`）：质量规则快照。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    workflow_defaults = workflow_template.get("defaults", {})
    default_target_crs = str(workflow_defaults.get("default_target_crs") or "EPSG:4490")
    quality_checks = quality_profile.get("quality_checks", [])

    @prompt_factory(description=PROMPT_DOC)
    def qgis_shapefile_quality_repair_export_workflow(
        task_name: str,
        input_dir: str,
        output_dir: str,
        target_crs: str = default_target_crs,
        geometry_type: str = "any",
        name_glob: str = "*.shp",
        required_fields: str = "",
        deliverables: str = "标准交付：修复后 shapefile、质检报告、运行摘要。",
    ) -> str:
        """
        作用：处理 `qgis_shapefile_quality_repair_export_workflow` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `qgis_shapefile_quality_repair_export_workflow` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP prompt 注册与调用流程中被调用，用于返回提示词模板内容。
        参数与返回：
        - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
        - 参数 `input_dir`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `output_dir`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `target_crs`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `default_target_crs`。
        - 参数 `geometry_type`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"any"`。
        - 参数 `name_glob`（`str`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `"*.shp"`。
        - 参数 `required_fields`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `""`。
        - 参数 `deliverables`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"标准交付：修复后 shapefile、质检报告、运行摘要。"`。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        task_text = task_name.strip() if isinstance(task_name, str) else ""
        input_text = input_dir.strip() if isinstance(input_dir, str) else ""
        output_text = output_dir.strip() if isinstance(output_dir, str) else ""
        target_crs_text = (
            target_crs.strip()
            if isinstance(target_crs, str) and target_crs.strip()
            else default_target_crs
        )
        geometry_type_text = (
            geometry_type.strip().lower()
            if isinstance(geometry_type, str) and geometry_type.strip()
            else "any"
        )
        name_glob_text = (
            name_glob.strip()
            if isinstance(name_glob, str) and name_glob.strip()
            else "*.shp"
        )
        required_fields_text = (
            required_fields.strip() if isinstance(required_fields, str) else ""
        )
        deliverables_text = (
            deliverables.strip()
            if isinstance(deliverables, str) and deliverables.strip()
            else "标准交付：修复后 shapefile、质检报告、运行摘要。"
        )
        normalized_required_fields = [
            item.strip()
            for item in required_fields_text.replace(";", ",").split(",")
            if item.strip()
        ]
        quality_checks_text = ", ".join(str(item) for item in quality_checks)
        if not quality_checks_text:
            quality_checks_text = (
                "bundle_integrity, crs_declared, geometry_valid, "
                "duplicates_checked, utf8_encoding_expected"
            )
        missing_required: list[str] = []
        if not task_text:
            missing_required.append("task_name")
        if not input_text:
            missing_required.append("input_dir")
        if not output_text:
            missing_required.append("output_dir")
        required_fields_display = (
            ", ".join(normalized_required_fields)
            if normalized_required_fields
            else "(none)"
        )
        return (
            "Structure: Parameters -> 9 Steps -> Quality Rules -> Deliverables\n"
            "Workflow: qgis_shapefile_quality_repair_export_workflow\n"
            "Task Parameters / 任务参数:\n"
            f"- task_name: {task_text or '[MISSING]'}\n"
            f"- input_dir: {input_text or '[MISSING]'}\n"
            f"- output_dir: {output_text or '[MISSING]'}\n"
            f"- target_crs: {target_crs_text}\n"
            f"- geometry_type: {geometry_type_text}\n"
            f"- name_glob: {name_glob_text}\n"
            f"- required_fields: {required_fields_display}\n"
            f"- deliverables: {deliverables_text}\n"
            f"- Missing required fields: {', '.join(missing_required) if missing_required else 'none'}\n"
            "1) 路径与目录前检\n"
            "- Tools: mcp_tools_filesystem_query_entry_info, mcp_tools_filesystem_stats_directory\n"
            "- Checkpoint: input_dir/output_dir 存在且可访问。\n"
            "2) 候选数据集扫描\n"
            "- Tools: mcp_tools_dataset_list_files\n"
            "- Checkpoint: geometry_type 与 name_glob 过滤后得到待处理 shapefile 列表。\n"
            "3) Shapefile bundle 完整性体检\n"
            "- Tools: mcp_tools_dataset_inspect_vector_bundle\n"
            "- Checkpoint: .shp/.shx/.dbf 必需成员齐全。\n"
            "4) 图层加载与解析\n"
            "- Tools: mcp_tools_dataset_load_layers\n"
            "- Checkpoint: skip_invalid=true，记录加载成功/失败清单。\n"
            "5) 几何与结构有效性检查\n"
            "- Tools: mcp_tools_vector_check_validity_report\n"
            "- Checkpoint: required_fields、CRS、一致性风险、导出安全性。\n"
            "6) 工作层标准化与修复\n"
            "- Tools: mcp_tools_vector_prepare_work_layer\n"
            "- Required options: normalize_field_names=true, multipart_policy=singleparts, target_crs=<target_crs>\n"
            "7) 导出前复核\n"
            "- Tools: mcp_tools_vector_check_validity_report\n"
            "- Checkpoint: 阻断级问题清零后再导出。\n"
            "8) 正式导出\n"
            "- Tools: mcp_tools_vector_export_dataset\n"
            "- Required options: file_format='shp'，如写盘需 confirm_write=true，覆盖需 confirm_destructive=true。\n"
            "9) 清理工作层与临时产物\n"
            "- Tools: mcp_tools_project_cleanup_work_layers\n"
            "- Checkpoint: 回收 task_name 对应工作层并记录 removed/deleted/missing。\n"
            "Quality Rules / 质量规则:\n"
            f"- quality_checks: {quality_checks_text}\n"
            "- blocking: invalid geometry、null/empty geometry、missing required fields、crs mismatch\n"
            "Final Deliverables / 最终交付:\n"
            f"- {deliverables_text}"
        )

    qgis_shapefile_quality_repair_export_workflow.__doc__ = PROMPT_DOC

