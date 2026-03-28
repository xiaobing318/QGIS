"""QGIS-backed MCP tool implementations and registration helpers."""

from __future__ import annotations

import inspect
from datetime import date, datetime, time as time_value, timezone
from pathlib import Path
from typing import Any

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
from . import mcp_tools_vector_add_layer as _MOD_vector_add_layer
from . import mcp_tools_vector_add_layers as _MOD_vector_add_layers
from . import mcp_tools_raster_add_layer as _MOD_raster_add_layer
from . import mcp_tools_raster_add_layers as _MOD_raster_add_layers
from . import mcp_tools_layer_list as _MOD_layer_list
from . import mcp_tools_layer_get_panel_tree as _MOD_layer_get_panel_tree
from . import mcp_tools_layer_get_details as _MOD_layer_get_details
from . import mcp_tools_layer_remove as _MOD_layer_remove
from . import mcp_tools_layer_remove_batch as _MOD_layer_remove_batch
from . import mcp_tools_layer_resolve_references as _MOD_layer_resolve_references
from . import mcp_tools_vector_get_layer_features as _MOD_vector_get_layer_features
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

REGISTERED_TOOL_NAMES: tuple[str, ...] = (
    'common_get_qgis_info',
    'vector_add_layer',
    'vector_add_layers',
    'raster_add_layer',
    'raster_add_layers',
    'layer_list',
    'layer_get_panel_tree',
    'layer_get_details',
    'layer_remove',
    'layer_remove_batch',
    'layer_resolve_references',
    'vector_get_layer_features',
    'vector_table_add_field',
    'vector_table_drop_fields',
    'vector_table_rename_field',
    'vector_table_calculate_field',
    'vector_table_query_records',
    'vector_table_insert_records',
    'vector_table_update_records',
    'vector_table_delete_records',
    'vector_table_truncate',
    'vector_stats_basic',
    'vector_stats_by_categories',
    'raster_stats_basic',
    'raster_stats_zonal',
    'raster_stats_cell',
    'dataset_list_files',
    'dataset_load_from_directory',
    'dataset_inspect_shapefile_bundle',
    'vector_check_validity_report',
    'vector_prepare_work_layer',
    'vector_export_shapefile',
    'project_cleanup_work_layers',
    'filesystem_query_list_entries',
    'filesystem_query_entry_info',
    'filesystem_query_read_text',
    'filesystem_edit_write_text',
    'filesystem_edit_append_text',
    'filesystem_edit_copy_entry',
    'filesystem_edit_move_entry',
    'filesystem_edit_delete_entry',
    'filesystem_stats_directory',
    'processing_list_providers',
    'processing_get_algorithms',
    'processing_get_parameter_template',
    'processing_execute_algorithm',
    'processing_execute_on_layers',
)
NON_TOOL_PUBLIC_HELPER_NAMES: tuple[str, ...] = (
    'get_project_snapshot',
    'get_layers_summary',
    'get_shapefile_workflow_template',
    'get_shapefile_quality_profile',
    'get_shapefile_run_summary',
)

_REGISTERED_TOOL_DOCSTRINGS: dict[str, str] = {
    'common_get_qgis_info': _MOD_common_get_qgis_info.TOOL_DOC,
    'vector_add_layer': _MOD_vector_add_layer.TOOL_DOC,
    'vector_add_layers': _MOD_vector_add_layers.TOOL_DOC,
    'raster_add_layer': _MOD_raster_add_layer.TOOL_DOC,
    'raster_add_layers': _MOD_raster_add_layers.TOOL_DOC,
    'layer_list': _MOD_layer_list.TOOL_DOC,
    'layer_get_panel_tree': _MOD_layer_get_panel_tree.TOOL_DOC,
    'layer_get_details': _MOD_layer_get_details.TOOL_DOC,
    'layer_remove': _MOD_layer_remove.TOOL_DOC,
    'layer_remove_batch': _MOD_layer_remove_batch.TOOL_DOC,
    'layer_resolve_references': _MOD_layer_resolve_references.TOOL_DOC,
    'vector_get_layer_features': _MOD_vector_get_layer_features.TOOL_DOC,
    'vector_table_add_field': _MOD_vector_table_add_field.TOOL_DOC,
    'vector_table_drop_fields': _MOD_vector_table_drop_fields.TOOL_DOC,
    'vector_table_rename_field': _MOD_vector_table_rename_field.TOOL_DOC,
    'vector_table_calculate_field': _MOD_vector_table_calculate_field.TOOL_DOC,
    'vector_table_query_records': _MOD_vector_table_query_records.TOOL_DOC,
    'vector_table_insert_records': _MOD_vector_table_insert_records.TOOL_DOC,
    'vector_table_update_records': _MOD_vector_table_update_records.TOOL_DOC,
    'vector_table_delete_records': _MOD_vector_table_delete_records.TOOL_DOC,
    'vector_table_truncate': _MOD_vector_table_truncate.TOOL_DOC,
    'vector_stats_basic': _MOD_vector_stats_basic.TOOL_DOC,
    'vector_stats_by_categories': _MOD_vector_stats_by_categories.TOOL_DOC,
    'raster_stats_basic': _MOD_raster_stats_basic.TOOL_DOC,
    'raster_stats_zonal': _MOD_raster_stats_zonal.TOOL_DOC,
    'raster_stats_cell': _MOD_raster_stats_cell.TOOL_DOC,
    'dataset_list_files': _MOD_dataset_list_files.TOOL_DOC,
    'dataset_load_from_directory': _MOD_dataset_load_from_directory.TOOL_DOC,
    'dataset_inspect_shapefile_bundle': _MOD_dataset_inspect_shapefile_bundle.TOOL_DOC,
    'vector_check_validity_report': _MOD_vector_check_validity_report.TOOL_DOC,
    'vector_prepare_work_layer': _MOD_vector_prepare_work_layer.TOOL_DOC,
    'vector_export_shapefile': _MOD_vector_export_shapefile.TOOL_DOC,
    'project_cleanup_work_layers': _MOD_project_cleanup_work_layers.TOOL_DOC,
    'filesystem_query_list_entries': _MOD_filesystem_query_list_entries.TOOL_DOC,
    'filesystem_query_entry_info': _MOD_filesystem_query_entry_info.TOOL_DOC,
    'filesystem_query_read_text': _MOD_filesystem_query_read_text.TOOL_DOC,
    'filesystem_edit_write_text': _MOD_filesystem_edit_write_text.TOOL_DOC,
    'filesystem_edit_append_text': _MOD_filesystem_edit_append_text.TOOL_DOC,
    'filesystem_edit_copy_entry': _MOD_filesystem_edit_copy_entry.TOOL_DOC,
    'filesystem_edit_move_entry': _MOD_filesystem_edit_move_entry.TOOL_DOC,
    'filesystem_edit_delete_entry': _MOD_filesystem_edit_delete_entry.TOOL_DOC,
    'filesystem_stats_directory': _MOD_filesystem_stats_directory.TOOL_DOC,
    'processing_list_providers': _MOD_processing_list_providers.TOOL_DOC,
    'processing_get_algorithms': _MOD_processing_get_algorithms.TOOL_DOC,
    'processing_get_parameter_template': _MOD_processing_get_parameter_template.TOOL_DOC,
    'processing_execute_algorithm': _MOD_processing_execute_algorithm.TOOL_DOC,
    'processing_execute_on_layers': _MOD_processing_execute_on_layers.TOOL_DOC,
}

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
        """Initialize the tool registry and cache workflow resources."""
        self._iface = iface
        self._runner = runner
        self._config = config
        self._shapefile_workflow_template = self._build_shapefile_workflow_template()
        self._shapefile_quality_profile = self._build_shapefile_quality_profile()
        self._shapefile_run_summary = self._empty_shapefile_run_summary()

    def get_project_snapshot(self) -> dict[str, Any]:
        """Return a snapshot of the current QGIS project."""
        return self._run(self._get_project_snapshot_impl)

    def get_layers_summary(self) -> dict[str, Any]:
        """Return a summary of the currently loaded layers."""
        return self._run(self._get_layers_summary_impl)

    def get_shapefile_workflow_template(self) -> dict[str, Any]:
        """Return the cached shapefile workflow template."""
        return self._run(self._get_shapefile_workflow_template_impl)

    def get_shapefile_quality_profile(self) -> dict[str, Any]:
        """Return the cached shapefile quality profile."""
        return self._run(self._get_shapefile_quality_profile_impl)

    def get_shapefile_run_summary(self) -> dict[str, Any]:
        """Return the cached shapefile run summary."""
        return self._run(self._get_shapefile_run_summary_impl)

    def _run(self, func, *args, **kwargs):
        """Run a callable on the main-thread runner and return its result."""
        return self._runner.run(lambda: func(*args, **kwargs))

    @staticmethod
    def _serialize_value(value: Any) -> Any:
        """Serialize values into JSON-friendly representations."""
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
        """Return the current UTC timestamp in ISO 8601 format."""
        return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")

    def _build_shapefile_workflow_template(self) -> dict[str, Any]:
        """Build the default shapefile workflow template."""
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
                "final_output_format": "shapefile",
                "default_target_crs": "EPSG:4490",
                "default_geometry_filter": "all",
                "cleanup_temp_layers": True,
                "cleanup_temp_files": True,
            },
            "stage_tool_map": {
                "path_check_and_geometry_filter": [
                    "common_get_qgis_info",
                    "filesystem_query_entry_info",
                    "filesystem_stats_directory",
                    "dataset_list_files",
                    "dataset_load_from_directory",
                ],
                "precheck_and_pre_statistics": [
                    "dataset_inspect_shapefile_bundle",
                    "vector_check_validity_report",
                    "filesystem_stats_directory",
                ],
                "standardize_crs": [
                    "vector_prepare_work_layer",
                ],
                "data_processing": [
                    "processing_get_parameter_template",
                    "processing_execute_algorithm",
                    "processing_execute_on_layers",
                    "vector_table_add_field",
                    "vector_table_drop_fields",
                    "vector_table_rename_field",
                    "vector_table_calculate_field",
                    "vector_table_query_records",
                    "vector_table_insert_records",
                    "vector_table_update_records",
                    "vector_table_delete_records",
                ],
                "export": [
                    "vector_check_validity_report",
                    "vector_export_shapefile",
                ],
                "cleanup": [
                    "project_cleanup_work_layers",
                    "layer_remove",
                ],
            },
            "phases": [
                {
                    "name": "path_check_and_geometry_filter",
                    "goal": "检查 input_dir 与 output_dir 是否显式给出，并按用户意图筛选点/线/面或全部类型。",
                    "recommended_tools": [
                        "common_get_qgis_info",
                        "filesystem_query_entry_info",
                        "filesystem_stats_directory",
                        "dataset_list_files",
                        "dataset_load_from_directory",
                    ],
                },
                {
                    "name": "precheck_and_pre_statistics",
                    "goal": "检查文件缺失、可打开性、无效几何、重复几何、UTF-8 编码风险，并统计文件数量与总大小。",
                    "recommended_tools": [
                        "dataset_inspect_shapefile_bundle",
                        "vector_check_validity_report",
                        "filesystem_stats_directory",
                    ],
                },
                {
                    "name": "standardize_crs",
                    "goal": "标准化坐标系统，未指定时默认转为 EPSG:4490。",
                    "recommended_tools": [
                        "vector_prepare_work_layer",
                    ],
                },
                {
                    "name": "data_processing",
                    "goal": "执行字段增删改查、缓冲区、面积字段等业务处理。",
                    "recommended_tools": [
                        "processing_get_parameter_template",
                        "processing_execute_algorithm",
                        "processing_execute_on_layers",
                        "vector_table_add_field",
                        "vector_table_drop_fields",
                        "vector_table_rename_field",
                        "vector_table_calculate_field",
                    ],
                },
                {
                    "name": "export",
                    "goal": "在确认导出路径和导出约束后，将结果正式导出到 output_dir。",
                    "recommended_tools": [
                        "vector_check_validity_report",
                        "vector_export_shapefile",
                    ],
                },
                {
                    "name": "cleanup",
                    "goal": "移除中间/临时图层与输入图层，按策略清理临时文件。",
                    "recommended_tools": [
                        "project_cleanup_work_layers",
                        "layer_remove",
                    ],
                },
            ],
        }

    def _build_shapefile_quality_profile(self) -> dict[str, Any]:
        """Build the default shapefile quality profile."""
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
        """Build an empty shapefile run summary record."""
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
        """Collect the current QGIS project snapshot."""
        project = QgsProject.instance()
        return {
            "file_name": project.fileName(),
            "title": project.title(),
            "crs": project.crs().authid() if project.crs().isValid() else "",
            "layer_count": len(project.mapLayers()),
        }

    def _get_layers_summary_impl(self) -> dict[str, Any]:
        """Collect a summary of the loaded layers."""
        layers = self.layer_list(layer_types="both")
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
        """Return the shapefile workflow template payload."""
        return self._serialize_value(self._shapefile_workflow_template)

    def _get_shapefile_quality_profile_impl(self) -> dict[str, Any]:
        """Return the shapefile quality profile payload."""
        return self._serialize_value(self._shapefile_quality_profile)

    def _get_shapefile_run_summary_impl(self) -> dict[str, Any]:
        """Return the shapefile run summary payload."""
        return self._serialize_value(self._shapefile_run_summary)

_TOOL_MODULES = [
    _MOD_common_get_qgis_info,
    _MOD_vector_add_layer,
    _MOD_vector_add_layers,
    _MOD_raster_add_layer,
    _MOD_raster_add_layers,
    _MOD_layer_list,
    _MOD_layer_get_panel_tree,
    _MOD_layer_get_details,
    _MOD_layer_remove,
    _MOD_layer_remove_batch,
    _MOD_layer_resolve_references,
    _MOD_vector_get_layer_features,
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
]

for _tool_module in _TOOL_MODULES:
    for _name, _callable in _tool_module.TOOL_METHODS.items():
        setattr(ProcessingMCPTools, _name, _callable)

def _validate_tool_entry_contracts() -> None:
    """Validate tool entry wrappers, impl mappings, and docstring registry integrity."""
    missing_tool_methods: list[str] = []
    missing_tool_impls: list[str] = []
    invalid_tool_wrappers: list[str] = []
    missing_docs: list[str] = []
    invalid_docs: list[str] = []
    extra_docs = sorted(set(_REGISTERED_TOOL_DOCSTRINGS) - set(REGISTERED_TOOL_NAMES))

    public_methods = {
        name
        for name, member in inspect.getmembers(ProcessingMCPTools, predicate=callable)
        if not name.startswith("_")
    }
    expected_public_methods = set(REGISTERED_TOOL_NAMES) | set(NON_TOOL_PUBLIC_HELPER_NAMES)
    missing_public_helpers = sorted(set(NON_TOOL_PUBLIC_HELPER_NAMES) - public_methods)
    unexpected_public_methods = sorted(public_methods - expected_public_methods)

    for tool_name in REGISTERED_TOOL_NAMES:
        method = getattr(ProcessingMCPTools, tool_name, None)
        if not callable(method):
            missing_tool_methods.append(tool_name)
            continue

        impl_name = f"_{tool_name}_impl"
        if not callable(getattr(ProcessingMCPTools, impl_name, None)):
            missing_tool_impls.append(impl_name)

        code = getattr(method, "__code__", None)
        co_names = tuple(getattr(code, "co_names", ()))
        if "_run" not in co_names or impl_name not in co_names:
            invalid_tool_wrappers.append(tool_name)

        description = _REGISTERED_TOOL_DOCSTRINGS.get(tool_name)
        if not isinstance(description, str):
            missing_docs.append(tool_name)
            continue
        if not description.strip():
            invalid_docs.append(tool_name)

    if (
        missing_tool_methods
        or missing_tool_impls
        or invalid_tool_wrappers
        or missing_public_helpers
        or unexpected_public_methods
        or missing_docs
        or invalid_docs
        or extra_docs
    ):
        raise RuntimeError(
            "Failed to validate MCP tool entry contracts: "
            f"missing_tool_methods={missing_tool_methods}, "
            f"missing_tool_impls={missing_tool_impls}, "
            f"invalid_tool_wrappers={invalid_tool_wrappers}, "
            f"missing_public_helpers={missing_public_helpers}, "
            f"unexpected_public_methods={unexpected_public_methods}, "
            f"missing_docs={missing_docs}, invalid_docs={invalid_docs}, extra_docs={extra_docs}"
        )

def _apply_registered_tool_docstrings() -> None:
    """Apply validated tool docstrings onto public tool entry methods."""
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
    """Register all processing tools with the MCP server."""
    _ = enable_execute_code
    tool_factory = getattr(mcp, "tool", None)
    if not callable(tool_factory):
        return
    for tool_name in REGISTERED_TOOL_NAMES:
        method = getattr(tools, tool_name, None)
        if callable(method) and getattr(method, "__name__", "") != "<lambda>":
            tool_factory()(method)
            continue

        def _wrapper(*args, _tool_name=tool_name, **kwargs):
            """Wrap a tool method so it can be registered dynamically."""
            return getattr(tools, _tool_name)(*args, **kwargs)

        _wrapper.__name__ = tool_name
        _wrapper.__doc__ = f"MCP tool wrapper for {tool_name}."
        tool_factory()(_wrapper)

_validate_tool_entry_contracts()
_apply_registered_tool_docstrings()
