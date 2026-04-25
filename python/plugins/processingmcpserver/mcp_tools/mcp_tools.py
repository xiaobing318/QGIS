"""QGIS-backed MCP tool implementations and registration helpers."""

from __future__ import annotations

import inspect
from datetime import date, datetime, time as time_value, timezone
from pathlib import Path
from typing import Any, Callable

from qgis.PyQt.QtCore import (
    QDate,
    QDateTime,
    QTime,
    Qt,
)
from qgis.core import (
    QgsProject,
)

from processingmcpserver.config_types import (
    ProcessingMCPServerConfig,
)
from processingmcpserver.mcp_main_thread_runner import McpMainThreadRunner

from . import mcp_tools_common_get_qgis_info as _MOD_common_get_qgis_info
from . import mcp_tools_vector_add_layers as _MOD_vector_add_layers
from . import mcp_tools_raster_add_layers as _MOD_raster_add_layers
from . import mcp_tools_layer_list as _MOD_layer_list
from . import mcp_tools_layer_get_panel_tree as _MOD_layer_get_panel_tree
from . import mcp_tools_layer_get_details as _MOD_layer_get_details
from . import mcp_tools_layer_remove_batch as _MOD_layer_remove_batch
from . import mcp_tools_layer_resolve_references as _MOD_layer_resolve_references
from . import mcp_tools_vector_table_add_field as _MOD_vector_table_add_field
from . import mcp_tools_vector_table_drop_fields as _MOD_vector_table_drop_fields
from . import mcp_tools_vector_table_rename_field as _MOD_vector_table_rename_field
from . import mcp_tools_vector_table_calculate_field as _MOD_vector_table_calculate_field
from . import mcp_tools_vector_table_query_records as _MOD_vector_table_query_records
from . import mcp_tools_vector_table_insert_records as _MOD_vector_table_insert_records
from . import mcp_tools_vector_table_update_records as _MOD_vector_table_update_records
from . import mcp_tools_vector_table_delete_records as _MOD_vector_table_delete_records
from . import mcp_tools_vector_table_truncate as _MOD_vector_table_truncate
from . import mcp_tools_vector_stats_basic as _MOD_vector_stats_basic
from . import mcp_tools_vector_stats_by_categories as _MOD_vector_stats_by_categories
from . import mcp_tools_raster_stats_basic as _MOD_raster_stats_basic
from . import mcp_tools_raster_stats_zonal as _MOD_raster_stats_zonal
from . import mcp_tools_raster_stats_cell as _MOD_raster_stats_cell
from . import mcp_tools_dataset_list_files as _MOD_dataset_list_files
from . import mcp_tools_dataset_load_from_directory as _MOD_dataset_load_from_directory
from . import mcp_tools_dataset_inspect_shapefile_bundle as _MOD_dataset_inspect_shapefile_bundle
from . import mcp_tools_vector_check_validity_report as _MOD_vector_check_validity_report
from . import mcp_tools_vector_prepare_work_layer as _MOD_vector_prepare_work_layer
from . import mcp_tools_vector_export_shapefile as _MOD_vector_export_shapefile
from . import mcp_tools_project_cleanup_work_layers as _MOD_project_cleanup_work_layers
from . import mcp_tools_filesystem_query_list_entries as _MOD_filesystem_query_list_entries
from . import mcp_tools_filesystem_query_entry_info as _MOD_filesystem_query_entry_info
from . import mcp_tools_filesystem_query_read_text as _MOD_filesystem_query_read_text
from . import mcp_tools_filesystem_edit_write_text as _MOD_filesystem_edit_write_text
from . import mcp_tools_filesystem_edit_append_text as _MOD_filesystem_edit_append_text
from . import mcp_tools_filesystem_edit_copy_entry as _MOD_filesystem_edit_copy_entry
from . import mcp_tools_filesystem_edit_move_entry as _MOD_filesystem_edit_move_entry
from . import mcp_tools_filesystem_edit_delete_entry as _MOD_filesystem_edit_delete_entry
from . import mcp_tools_filesystem_stats_directory as _MOD_filesystem_stats_directory
from . import mcp_tools_processing_list_providers as _MOD_processing_list_providers
from . import mcp_tools_processing_get_algorithms as _MOD_processing_get_algorithms
from . import mcp_tools_processing_get_parameter_template as _MOD_processing_get_parameter_template
from . import mcp_tools_processing_execute_algorithm as _MOD_processing_execute_algorithm
from . import mcp_tools_processing_execute_on_layers as _MOD_processing_execute_on_layers
from . import mcp_tools_prefixed_highfreq as _MOD_prefixed_highfreq

REGISTERED_TOOL_NAMES: tuple[str, ...] = (
    "mcp_tools_common_get_qgis_info",
    "mcp_tools_layer_list",
    "mcp_tools_layer_get_details",
    "mcp_tools_layer_remove_batch",
    "mcp_tools_layer_resolve_references",
    "mcp_tools_dataset_list_files",
    "mcp_tools_dataset_load_layers",
    "mcp_tools_dataset_inspect_vector_bundle",
    "mcp_tools_project_cleanup_work_layers",
    "mcp_tools_filesystem_query_list_entries",
    "mcp_tools_filesystem_query_entry_info",
    "mcp_tools_filesystem_query_read_text",
    "mcp_tools_filesystem_edit_write_text",
    "mcp_tools_filesystem_edit_append_text",
    "mcp_tools_filesystem_edit_copy_entry",
    "mcp_tools_filesystem_edit_move_entry",
    "mcp_tools_filesystem_edit_delete_entry",
    "mcp_tools_filesystem_stats_directory",
    "mcp_tools_vector_table_add_field",
    "mcp_tools_vector_table_drop_fields",
    "mcp_tools_vector_table_rename_field",
    "mcp_tools_vector_table_calculate_field",
    "mcp_tools_vector_table_query_records",
    "mcp_tools_vector_table_insert_records",
    "mcp_tools_vector_table_update_records",
    "mcp_tools_vector_table_delete_records",
    "mcp_tools_vector_table_truncate",
    "mcp_tools_vector_stats_basic",
    "mcp_tools_vector_stats_by_categories",
    "mcp_tools_vector_check_validity_report",
    "mcp_tools_vector_prepare_work_layer",
    "mcp_tools_vector_export_dataset",
    "mcp_tools_vector_buffer",
    "mcp_tools_vector_dissolve",
    "mcp_tools_vector_clip",
    "mcp_tools_vector_intersection",
    "mcp_tools_vector_union",
    "mcp_tools_vector_difference",
    "mcp_tools_vector_fix_geometries",
    "mcp_tools_vector_reproject_layer",
    "mcp_tools_vector_join_attributes_by_location",
    "mcp_tools_raster_stats_basic",
    "mcp_tools_raster_stats_zonal",
    "mcp_tools_raster_stats_cell",
    "mcp_tools_raster_clip_by_mask",
    "mcp_tools_raster_warp_reproject",
    "mcp_tools_raster_calculator",
    "mcp_tools_raster_merge",
    "mcp_tools_raster_reclassify_by_table",
    "mcp_tools_processing_list_providers",
    "mcp_tools_processing_get_algorithms",
    "mcp_tools_processing_get_parameter_template",
    "mcp_tools_processing_execute_algorithm",
)
NON_TOOL_PUBLIC_HELPER_NAMES: tuple[str, ...] = (
    'get_project_snapshot',
    'get_layers_summary',
    'get_shapefile_workflow_template',
    'get_shapefile_quality_profile',
    'get_shapefile_run_summary',
)

TOOL_ALIAS_MAP: dict[str, str] = {
    "mcp_tools_common_get_qgis_info": "common_get_qgis_info",
    "mcp_tools_layer_list": "layer_list",
    "mcp_tools_layer_get_details": "layer_get_details",
    "mcp_tools_layer_remove_batch": "layer_remove_batch",
    "mcp_tools_layer_resolve_references": "layer_resolve_references",
    "mcp_tools_dataset_list_files": "dataset_list_files",
    "mcp_tools_project_cleanup_work_layers": "project_cleanup_work_layers",
    "mcp_tools_filesystem_query_list_entries": "filesystem_query_list_entries",
    "mcp_tools_filesystem_query_entry_info": "filesystem_query_entry_info",
    "mcp_tools_filesystem_query_read_text": "filesystem_query_read_text",
    "mcp_tools_filesystem_edit_write_text": "filesystem_edit_write_text",
    "mcp_tools_filesystem_edit_append_text": "filesystem_edit_append_text",
    "mcp_tools_filesystem_edit_copy_entry": "filesystem_edit_copy_entry",
    "mcp_tools_filesystem_edit_move_entry": "filesystem_edit_move_entry",
    "mcp_tools_filesystem_edit_delete_entry": "filesystem_edit_delete_entry",
    "mcp_tools_filesystem_stats_directory": "filesystem_stats_directory",
    "mcp_tools_vector_table_add_field": "vector_table_add_field",
    "mcp_tools_vector_table_drop_fields": "vector_table_drop_fields",
    "mcp_tools_vector_table_rename_field": "vector_table_rename_field",
    "mcp_tools_vector_table_calculate_field": "vector_table_calculate_field",
    "mcp_tools_vector_table_query_records": "vector_table_query_records",
    "mcp_tools_vector_table_insert_records": "vector_table_insert_records",
    "mcp_tools_vector_table_update_records": "vector_table_update_records",
    "mcp_tools_vector_table_delete_records": "vector_table_delete_records",
    "mcp_tools_vector_table_truncate": "vector_table_truncate",
    "mcp_tools_vector_stats_basic": "vector_stats_basic",
    "mcp_tools_vector_stats_by_categories": "vector_stats_by_categories",
    "mcp_tools_vector_check_validity_report": "vector_check_validity_report",
    "mcp_tools_vector_prepare_work_layer": "vector_prepare_work_layer",
    "mcp_tools_raster_stats_basic": "raster_stats_basic",
    "mcp_tools_raster_stats_zonal": "raster_stats_zonal",
    "mcp_tools_raster_stats_cell": "raster_stats_cell",
    "mcp_tools_processing_list_providers": "processing_list_providers",
    "mcp_tools_processing_get_algorithms": "processing_get_algorithms",
    "mcp_tools_processing_get_parameter_template": "processing_get_parameter_template",
    "mcp_tools_processing_execute_algorithm": "processing_execute_algorithm",
}

_LEGACY_TOOL_DOCS: dict[str, str] = {
    "common_get_qgis_info": _MOD_common_get_qgis_info.TOOL_DOC,
    "layer_list": _MOD_layer_list.TOOL_DOC,
    "layer_get_details": _MOD_layer_get_details.TOOL_DOC,
    "layer_remove_batch": _MOD_layer_remove_batch.TOOL_DOC,
    "layer_resolve_references": _MOD_layer_resolve_references.TOOL_DOC,
    "dataset_list_files": _MOD_dataset_list_files.TOOL_DOC,
    "project_cleanup_work_layers": _MOD_project_cleanup_work_layers.TOOL_DOC,
    "filesystem_query_list_entries": _MOD_filesystem_query_list_entries.TOOL_DOC,
    "filesystem_query_entry_info": _MOD_filesystem_query_entry_info.TOOL_DOC,
    "filesystem_query_read_text": _MOD_filesystem_query_read_text.TOOL_DOC,
    "filesystem_edit_write_text": _MOD_filesystem_edit_write_text.TOOL_DOC,
    "filesystem_edit_append_text": _MOD_filesystem_edit_append_text.TOOL_DOC,
    "filesystem_edit_copy_entry": _MOD_filesystem_edit_copy_entry.TOOL_DOC,
    "filesystem_edit_move_entry": _MOD_filesystem_edit_move_entry.TOOL_DOC,
    "filesystem_edit_delete_entry": _MOD_filesystem_edit_delete_entry.TOOL_DOC,
    "filesystem_stats_directory": _MOD_filesystem_stats_directory.TOOL_DOC,
    "vector_table_add_field": _MOD_vector_table_add_field.TOOL_DOC,
    "vector_table_drop_fields": _MOD_vector_table_drop_fields.TOOL_DOC,
    "vector_table_rename_field": _MOD_vector_table_rename_field.TOOL_DOC,
    "vector_table_calculate_field": _MOD_vector_table_calculate_field.TOOL_DOC,
    "vector_table_query_records": _MOD_vector_table_query_records.TOOL_DOC,
    "vector_table_insert_records": _MOD_vector_table_insert_records.TOOL_DOC,
    "vector_table_update_records": _MOD_vector_table_update_records.TOOL_DOC,
    "vector_table_delete_records": _MOD_vector_table_delete_records.TOOL_DOC,
    "vector_table_truncate": _MOD_vector_table_truncate.TOOL_DOC,
    "vector_stats_basic": _MOD_vector_stats_basic.TOOL_DOC,
    "vector_stats_by_categories": _MOD_vector_stats_by_categories.TOOL_DOC,
    "vector_check_validity_report": _MOD_vector_check_validity_report.TOOL_DOC,
    "vector_prepare_work_layer": _MOD_vector_prepare_work_layer.TOOL_DOC,
    "raster_stats_basic": _MOD_raster_stats_basic.TOOL_DOC,
    "raster_stats_zonal": _MOD_raster_stats_zonal.TOOL_DOC,
    "raster_stats_cell": _MOD_raster_stats_cell.TOOL_DOC,
    "processing_list_providers": _MOD_processing_list_providers.TOOL_DOC,
    "processing_get_algorithms": _MOD_processing_get_algorithms.TOOL_DOC,
    "processing_get_parameter_template": _MOD_processing_get_parameter_template.TOOL_DOC,
    "processing_execute_algorithm": _MOD_processing_execute_algorithm.TOOL_DOC,
}

_REGISTERED_TOOL_DOCSTRINGS: dict[str, str] = {
    new_name: _LEGACY_TOOL_DOCS[old_name]
    for new_name, old_name in TOOL_ALIAS_MAP.items()
}
_REGISTERED_TOOL_DOCSTRINGS.update(_MOD_prefixed_highfreq.TOOL_DOCS)

class ProcessingMCPTools:
    REGISTERED_TOOL_NAMES = REGISTERED_TOOL_NAMES
    NON_TOOL_PUBLIC_HELPER_NAMES = NON_TOOL_PUBLIC_HELPER_NAMES

    DEFAULT_FEATURE_LIMIT = 10
    MAX_FEATURE_LIMIT = 100
    DEFAULT_DATASET_LIMIT = 50
    MAX_DATASET_LIMIT = 100
    DEFAULT_FILESYSTEM_LIST_LIMIT = 100
    MAX_FILESYSTEM_LIST_LIMIT = 200
    DEFAULT_ALGORITHM_LIST_LIMIT = 30
    MAX_ALGORITHM_LIST_LIMIT = 60

    def __init__(
        self,
        iface,
        runner: McpMainThreadRunner,
        config: ProcessingMCPServerConfig | None = None,
    ):
        """
        作用：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `iface`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `runner`（`McpMainThreadRunner`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `config`（`ProcessingMCPServerConfig | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self._iface = iface
        self._runner = runner
        self._config = config
        self._shapefile_workflow_template = self._build_shapefile_workflow_template()
        self._shapefile_quality_profile = self._build_shapefile_quality_profile()
        self._shapefile_run_summary = self._empty_shapefile_run_summary()

    def get_project_snapshot(self) -> dict[str, Any]:
        """
        作用：实现 `get_project_snapshot` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `get_project_snapshot` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return self._run(self._get_project_snapshot_impl)

    def get_layers_summary(self) -> dict[str, Any]:
        """
        作用：实现 `get_layers_summary` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `get_layers_summary` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return self._run(self._get_layers_summary_impl)

    def get_shapefile_workflow_template(self) -> dict[str, Any]:
        """
        作用：实现 `get_shapefile_workflow_template` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `get_shapefile_workflow_template` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return self._run(self._get_shapefile_workflow_template_impl)

    def get_shapefile_quality_profile(self) -> dict[str, Any]:
        """
        作用：实现 `get_shapefile_quality_profile` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `get_shapefile_quality_profile` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return self._run(self._get_shapefile_quality_profile_impl)

    def get_shapefile_run_summary(self) -> dict[str, Any]:
        """
        作用：实现 `get_shapefile_run_summary` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `get_shapefile_run_summary` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return self._run(self._get_shapefile_run_summary_impl)

    def _run(self, func, *args, **kwargs):
        """
        作用：封装内部辅助步骤 `_run`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_run`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `func`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `*args`：可变位置参数，按调用时顺序传入补充数据。
        - 参数 `**kwargs`：可变关键字参数，用于扩展命名输入。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return self._runner.run(lambda: func(*args, **kwargs))

    @staticmethod
    def _serialize_value(value: Any) -> Any:
        """
        作用：封装内部辅助步骤 `_serialize_value`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_serialize_value`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `value`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
        """
        if value is None:
            return None
        if isinstance(value, (bool, int, float, str)):
            return value
        if isinstance(value, (date, datetime, time_value)):
            return value.isoformat()
        if isinstance(value, QDateTime):
            return value.toString(Qt.DateFormat.ISODateWithMs)
        if isinstance(value, QDate):
            return value.toString(Qt.DateFormat.ISODate)
        if isinstance(value, QTime):
            return value.toString(Qt.DateFormat.ISODateWithMs)
        if isinstance(value, Path):
            return str(value)
        if isinstance(value, (list, tuple, set)):
            return [ProcessingMCPTools._serialize_value(item) for item in value]
        if isinstance(value, dict):
            return {str(k): ProcessingMCPTools._serialize_value(v) for k, v in value.items()}
        if hasattr(value, "id") and callable(value.id):
            try:
                return value.id()
            except Exception:
                pass
        if hasattr(value, "toString") and callable(value.toString):
            try:
                return value.toString()
            except Exception:
                pass
        return str(value)

    @staticmethod
    def _utc_now_iso() -> str:
        """
        作用：封装内部辅助步骤 `_utc_now_iso`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_utc_now_iso`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数：无。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")

    def _build_shapefile_workflow_template(self) -> dict[str, Any]:
        """
        作用：构建 `_build_shapefile_workflow_template` 相关对象或配置数据，供后续流程直接复用。
        用途：构建 `_build_shapefile_workflow_template` 相关对象或配置数据，供后续流程直接复用。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return {
            "schema_version": "1.0.0",
            "name": "shapefile_standard_workflow",
            "summary": (
                "用于 QCopilots/processingmcpserver 的 shapefile 自动化处理模板。"
                "固定六阶段执行：路径检查与筛选、预检与统计、标准化、处理、导出、清理。"
            ),
            "workflow_stages": [
                "path_check_and_geometry_filter",
                "precheck_and_pre_statistics",
                "standardize_crs",
                "data_processing",
                "export",
                "cleanup",
            ],
            "required_inputs": [
                "task_name",
                "input_dir",
                "output_dir",
                "quality_rule_resource",
                "deliverables",
            ],
            "defaults": {
                "execution_mode": "plan_then_confirm",
                "intermediate_storage": "temporary_layers",
                "final_output_format": "gpkg",
                "default_target_crs": "EPSG:4490",
                "default_geometry_filter": "all",
                "cleanup_temp_layers": True,
                "cleanup_temp_files": True,
            },
            "stage_tool_map": {
                "path_check_and_geometry_filter": [
                    "mcp_tools_common_get_qgis_info",
                    "mcp_tools_filesystem_query_entry_info",
                    "mcp_tools_filesystem_stats_directory",
                    "mcp_tools_dataset_list_files",
                    "mcp_tools_dataset_load_layers",
                ],
                "precheck_and_pre_statistics": [
                    "mcp_tools_dataset_inspect_vector_bundle",
                    "mcp_tools_vector_check_validity_report",
                    "mcp_tools_filesystem_stats_directory",
                ],
                "standardize_crs": [
                    "mcp_tools_vector_prepare_work_layer",
                ],
                "data_processing": [
                    "mcp_tools_processing_get_parameter_template",
                    "mcp_tools_processing_execute_algorithm",
                    "mcp_tools_vector_table_add_field",
                    "mcp_tools_vector_table_drop_fields",
                    "mcp_tools_vector_table_rename_field",
                    "mcp_tools_vector_table_calculate_field",
                    "mcp_tools_vector_table_query_records",
                    "mcp_tools_vector_table_insert_records",
                    "mcp_tools_vector_table_update_records",
                    "mcp_tools_vector_table_delete_records",
                ],
                "export": [
                    "mcp_tools_vector_check_validity_report",
                    "mcp_tools_vector_export_dataset",
                ],
                "cleanup": [
                    "mcp_tools_project_cleanup_work_layers",
                    "mcp_tools_layer_remove_batch",
                ],
            },
            "phases": [
                {
                    "name": "path_check_and_geometry_filter",
                    "goal": "检查 input_dir 与 output_dir 是否显式给出，并按用户意图筛选点/线/面或全部类型。",
                    "recommended_tools": [
                        "mcp_tools_common_get_qgis_info",
                        "mcp_tools_filesystem_query_entry_info",
                        "mcp_tools_filesystem_stats_directory",
                        "mcp_tools_dataset_list_files",
                        "mcp_tools_dataset_load_layers",
                    ],
                },
                {
                    "name": "precheck_and_pre_statistics",
                    "goal": "检查文件缺失、可打开性、无效几何、重复几何、UTF-8 编码风险，并统计文件数量与总大小。",
                    "recommended_tools": [
                        "mcp_tools_dataset_inspect_vector_bundle",
                        "mcp_tools_vector_check_validity_report",
                        "mcp_tools_filesystem_stats_directory",
                    ],
                },
                {
                    "name": "standardize_crs",
                    "goal": "标准化坐标系统，未指定时默认转为 EPSG:4490。",
                    "recommended_tools": [
                        "mcp_tools_vector_prepare_work_layer",
                    ],
                },
                {
                    "name": "data_processing",
                    "goal": "执行字段增删改查、缓冲区、面积字段等业务处理。",
                    "recommended_tools": [
                        "mcp_tools_processing_get_parameter_template",
                        "mcp_tools_processing_execute_algorithm",
                        "mcp_tools_vector_table_add_field",
                        "mcp_tools_vector_table_drop_fields",
                        "mcp_tools_vector_table_rename_field",
                        "mcp_tools_vector_table_calculate_field",
                    ],
                },
                {
                    "name": "export",
                    "goal": "在确认导出路径和导出约束后，将结果正式导出到 output_dir。",
                    "recommended_tools": [
                        "mcp_tools_vector_check_validity_report",
                        "mcp_tools_vector_export_dataset",
                    ],
                },
                {
                    "name": "cleanup",
                    "goal": "移除中间/临时图层与输入图层，按策略清理临时文件。",
                    "recommended_tools": [
                        "mcp_tools_project_cleanup_work_layers",
                        "mcp_tools_layer_remove_batch",
                    ],
                },
            ],
        }

    def _build_shapefile_quality_profile(self) -> dict[str, Any]:
        """
        作用：构建 `_build_shapefile_quality_profile` 相关对象或配置数据，供后续流程直接复用。
        用途：构建 `_build_shapefile_quality_profile` 相关对象或配置数据，供后续流程直接复用。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return {
            "schema_version": "1.0.0",
            "name": "default",
            "scope": "vector_shapefile",
            "quality_checks": [
                "bundle_integrity",
                "crs_declared",
                "geometry_valid",
                "duplicates_checked",
                "utf8_encoding_expected",
            ],
            "blocking_rules": [
                "missing_required_bundle_files",
                "missing_required_fields",
                "invalid_geometry_count > 0",
                "null_or_empty_geometry_count > 0",
                "crs_mismatch",
            ],
            "warning_rules": [
                "missing_prj_file",
                "missing_cpg_file",
                "duplicate_geometry_count > 0",
                "duplicate_record_count > 0",
                "multipart_feature_count > 0",
                "field_name_length_gt_10",
                "null_attribute_count > 0",
            ],
            "export_constraints": {
                "driver": "ESRI Shapefile",
                "field_name_max_length": 10,
                "encoding": "UTF-8",
                "write_cpg": True,
            },
            "confirmation_policy": {
                "require_confirm_write": True,
                "require_confirm_destructive_for_overwrite": True,
            },
        }

    def _empty_shapefile_run_summary(self) -> dict[str, Any]:
        """
        作用：封装内部辅助步骤 `_empty_shapefile_run_summary`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_empty_shapefile_run_summary`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return {
            "schema_version": "1.0.0",
            "generated_at": self._utc_now_iso(),
            "task_name": "",
            "status": "idle",
            "inputs": {},
            "steps": [],
            "warnings": [],
            "outputs": {
                "work_layer_ids": [],
                "final_shapefiles": [],
                "removed_layer_ids": [],
                "removed_temp_files": [],
            },
        }

    def _get_project_snapshot_impl(self) -> dict[str, Any]:
        """
        作用：实现 `_get_project_snapshot_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
        用途：实现 `_get_project_snapshot_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        project = QgsProject.instance()
        return {
            "file_name": project.fileName(),
            "title": project.title(),
            "crs": project.crs().authid() if project.crs().isValid() else "",
            "layer_count": len(project.mapLayers()),
        }

    def _get_layers_summary_impl(self) -> dict[str, Any]:
        """
        作用：实现 `_get_layers_summary_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
        用途：实现 `_get_layers_summary_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        layers = self.mcp_tools_layer_list(layer_types="both")
        return {
            "count": len(layers),
            "counts": {
                "vector": len([x for x in layers if str(x["type"]).startswith("vector_")]),
                "raster": len([x for x in layers if x["type"] == "raster"]),
                "other": len([x for x in layers if not str(x["type"]).startswith("vector_") and x["type"] != "raster"]),
            },
            "layers": layers,
        }

    def _get_shapefile_workflow_template_impl(self) -> dict[str, Any]:
        """
        作用：实现 `_get_shapefile_workflow_template_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
        用途：实现 `_get_shapefile_workflow_template_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return self._serialize_value(self._shapefile_workflow_template)

    def _get_shapefile_quality_profile_impl(self) -> dict[str, Any]:
        """
        作用：实现 `_get_shapefile_quality_profile_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
        用途：实现 `_get_shapefile_quality_profile_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return self._serialize_value(self._shapefile_quality_profile)

    def _get_shapefile_run_summary_impl(self) -> dict[str, Any]:
        """
        作用：实现 `_get_shapefile_run_summary_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
        用途：实现 `_get_shapefile_run_summary_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
        使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return self._serialize_value(self._shapefile_run_summary)

_TOOL_MODULES = [
    _MOD_common_get_qgis_info,
    _MOD_vector_add_layers,
    _MOD_raster_add_layers,
    _MOD_layer_list,
    _MOD_layer_get_panel_tree,
    _MOD_layer_get_details,
    _MOD_layer_remove_batch,
    _MOD_layer_resolve_references,
    _MOD_vector_table_add_field,
    _MOD_vector_table_drop_fields,
    _MOD_vector_table_rename_field,
    _MOD_vector_table_calculate_field,
    _MOD_vector_table_query_records,
    _MOD_vector_table_insert_records,
    _MOD_vector_table_update_records,
    _MOD_vector_table_delete_records,
    _MOD_vector_table_truncate,
    _MOD_vector_stats_basic,
    _MOD_vector_stats_by_categories,
    _MOD_raster_stats_basic,
    _MOD_raster_stats_zonal,
    _MOD_raster_stats_cell,
    _MOD_dataset_list_files,
    _MOD_dataset_load_from_directory,
    _MOD_dataset_inspect_shapefile_bundle,
    _MOD_vector_check_validity_report,
    _MOD_vector_prepare_work_layer,
    _MOD_vector_export_shapefile,
    _MOD_project_cleanup_work_layers,
    _MOD_filesystem_query_list_entries,
    _MOD_filesystem_query_entry_info,
    _MOD_filesystem_query_read_text,
    _MOD_filesystem_edit_write_text,
    _MOD_filesystem_edit_append_text,
    _MOD_filesystem_edit_copy_entry,
    _MOD_filesystem_edit_move_entry,
    _MOD_filesystem_edit_delete_entry,
    _MOD_filesystem_stats_directory,
    _MOD_processing_list_providers,
    _MOD_processing_get_algorithms,
    _MOD_processing_get_parameter_template,
    _MOD_processing_execute_algorithm,
    _MOD_processing_execute_on_layers,
    _MOD_prefixed_highfreq,
]

for _tool_module in _TOOL_MODULES:
    for _name, _callable in _tool_module.TOOL_METHODS.items():
        setattr(ProcessingMCPTools, _name, _callable)


def _copy_callable_signature(source: Callable[..., Any], target: Callable[..., Any]) -> None:
    """
    作用：复制 `source` 的函数签名到 `target`，避免注册后暴露通用可变参数名称。
    用途：复制 `source` 的函数签名到 `target`，避免注册后暴露通用可变参数名称。
    使用场景：在 MCP 工具包装器创建阶段调用，确保 FastMCP 基于业务参数生成入参模式。
    参数与返回：
    - 参数 `source`：签名来源函数。
    - 参数 `target`：签名接收函数。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    try:
        target.__signature__ = inspect.signature(source)
    except (TypeError, ValueError):
        return


def _build_alias_tool_method(new_name: str, legacy_method: Callable[..., Any]) -> Callable[..., Any]:
    def _alias(self, *args, **kwargs):
        payload = legacy_method(self, *args, **kwargs)
        if isinstance(payload, dict):
            normalized = dict(payload)
            normalized["tool"] = new_name
            return normalized
        return payload

    _alias.__name__ = new_name
    _alias.__doc__ = getattr(legacy_method, "__doc__", None)
    _copy_callable_signature(legacy_method, _alias)
    return _alias


def _build_registered_tool_wrapper(method: Callable[..., Any], tool_name: str) -> Callable[..., Any]:
    def wrapper(*args, **kwargs):
        payload = method(*args, **kwargs)
        if isinstance(payload, dict):
            normalized = dict(payload)
            normalized["tool"] = tool_name
            return normalized
        return payload

    wrapper.__name__ = tool_name
    wrapper.__doc__ = getattr(method, "__doc__", None) or f"MCP tool wrapper for {tool_name}."
    _copy_callable_signature(method, wrapper)
    return wrapper


for _new_name, _legacy_name in TOOL_ALIAS_MAP.items():
    _legacy_method = getattr(ProcessingMCPTools, _legacy_name, None)
    if callable(_legacy_method):
        setattr(ProcessingMCPTools, _new_name, _build_alias_tool_method(_new_name, _legacy_method))

def _validate_tool_entry_contracts() -> None:
    """
    作用：封装内部辅助步骤 `_validate_tool_entry_contracts`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_validate_tool_entry_contracts`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数：无。
    - 返回：无返回值。
    返回结果：无返回值。
    异常：可能显式抛出 `RuntimeError`。
    """
    missing_tool_methods: list[str] = []
    missing_docs: list[str] = []
    invalid_docs: list[str] = []
    extra_docs = sorted(set(_REGISTERED_TOOL_DOCSTRINGS) - set(REGISTERED_TOOL_NAMES))
    public_methods = {
        name for name, member in inspect.getmembers(ProcessingMCPTools, predicate=callable) if not name.startswith("_")
    }
    missing_public_helpers = sorted(set(NON_TOOL_PUBLIC_HELPER_NAMES) - public_methods)

    for tool_name in REGISTERED_TOOL_NAMES:
        method = getattr(ProcessingMCPTools, tool_name, None)
        if not callable(method):
            missing_tool_methods.append(tool_name)
            continue

        description = _REGISTERED_TOOL_DOCSTRINGS.get(tool_name)
        if not isinstance(description, str):
            missing_docs.append(tool_name)
            continue
        if not description.strip():
            invalid_docs.append(tool_name)

    if (
        missing_tool_methods
        or missing_public_helpers
        or missing_docs
        or invalid_docs
        or extra_docs
    ):
        raise RuntimeError(
            "Failed to validate MCP tool entry contracts: "
            f"missing_tool_methods={missing_tool_methods}, "
            f"missing_public_helpers={missing_public_helpers}, "
            f"missing_docs={missing_docs}, invalid_docs={invalid_docs}, extra_docs={extra_docs}"
        )

def _apply_registered_tool_docstrings() -> None:
    """
    作用：封装内部辅助步骤 `_apply_registered_tool_docstrings`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_apply_registered_tool_docstrings`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数：无。
    - 返回：无返回值。
    返回结果：无返回值。
    异常：可能显式抛出 `RuntimeError`。
    """
    for tool_name in REGISTERED_TOOL_NAMES:
        method = getattr(ProcessingMCPTools, tool_name, None)
        if not callable(method):
            raise RuntimeError(f"Cannot set tool docstring, missing method: {tool_name}")
        doc = _REGISTERED_TOOL_DOCSTRINGS[tool_name].strip()
        if "用途：" not in doc:
            doc = f"用途：{doc}"
        if "返回结果：" not in doc:
            doc = f"{doc} 返回结果：见工具输出。"
        method.__doc__ = doc

def register_tools(mcp, tools: ProcessingMCPTools, enable_execute_code: bool = True) -> None:
    """
    作用：注册 `tools`，完成当前函数负责的处理步骤并产出结果。
    用途：注册 `tools`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `mcp`：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `tools`（`ProcessingMCPTools`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `enable_execute_code`（`bool`）：布尔开关参数，用于控制是否启用特定行为。 默认值为 `True`。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    _ = enable_execute_code
    tool_factory = getattr(mcp, "tool", None)
    if not callable(tool_factory):
        return
    for tool_name in REGISTERED_TOOL_NAMES:
        method = getattr(tools, tool_name, None)
        if not callable(method):
            continue
        if getattr(method, "__name__", "") == tool_name:
            tool_factory()(method)
            continue
        tool_factory()(_build_registered_tool_wrapper(method, tool_name))

_validate_tool_entry_contracts()
_apply_registered_tool_docstrings()
