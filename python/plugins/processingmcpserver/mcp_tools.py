"""QGIS-backed MCP tool implementations and registration helpers."""

from __future__ import annotations

import inspect
import json
import platform
import re
import shutil
import statistics
import sys
from datetime import date, datetime, time as time_value, timezone
from fnmatch import fnmatch
from pathlib import Path
from typing import Any

import processing
from processing.core.Processing import Processing
from qgis.PyQt.QtCore import (
    QDate,
    QDateTime,
    QTime,
    Qt,
    QVariant)
from qgis.core import (
    Qgis,
    QgsApplication,
    QgsExpression,
    QgsExpressionContext,
    QgsExpressionContextUtils,
    QgsFeature,
    QgsFeatureRequest,
    QgsField,
    QgsGeometry,
    QgsLayerTreeGroup,
    QgsLayerTreeLayer,
    QgsMapLayer,
    QgsCoordinateReferenceSystem,
    QgsCoordinateTransform,
    QgsProcessingParameterDefinition,
    QgsProcessingParameterEnum,
    QgsProject,
    QgsRasterLayer,
    QgsVectorLayer,
    QgsVectorFileWriter,
    QgsWkbTypes,
)
from qgis.utils import active_plugins

from processingmcpserver.config_types import (
    ProcessingMCPServerConfig,
)
from processingmcpserver.mcp_main_thread_runner import McpMainThreadRunner

# Module state & contracts.
_COPY_VECTOR_SAFETY = (
    "默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。"
)
_DESTRUCTIVE_VECTOR_SAFETY = (
    "默认 in_place=false；删除类操作还要求 confirm_destructive=true，避免误删原图层或记录。"
)
_FILESYSTEM_WRITE_SAFETY = (
    "所有 filesystem_edit_* 调用都要求 confirm_write=true；删除或覆盖时还必须显式设置 confirm_destructive=true。"
)
_PROCESSING_SAFETY = (
    "默认禁止磁盘写出和原位编辑；只有在明确需要时才把 allow_disk_write 或 allow_in_place_edit 设为 true，并应复核返回里的 safety_policy、warnings 与 effective_parameters。"
)
# Guard for one-time Processing.initialize() within current Python runtime.
_PROCESSING_INITIALIZED = False

REGISTERED_TOOL_NAMES: tuple[str, ...] = (
    "common_get_qgis_info",
    "vector_add_layer",
    "vector_add_layers",
    "raster_add_layer",
    "raster_add_layers",
    "layer_list",
    "layer_get_panel_tree",
    "layer_get_details",
    "layer_remove",
    "layer_remove_batch",
    "layer_resolve_references",
    "vector_get_layer_features",
    "vector_table_add_field",
    "vector_table_drop_fields",
    "vector_table_rename_field",
    "vector_table_calculate_field",
    "vector_table_query_records",
    "vector_table_insert_records",
    "vector_table_update_records",
    "vector_table_delete_records",
    "vector_table_truncate",
    "vector_stats_basic",
    "vector_stats_by_categories",
    "raster_stats_basic",
    "raster_stats_zonal",
    "raster_stats_cell",
    "dataset_list_files",
    "dataset_load_from_directory",
    "dataset_inspect_shapefile_bundle",
    "vector_check_validity_report",
    "vector_prepare_work_layer",
    "vector_export_shapefile",
    "project_cleanup_work_layers",
    "filesystem_query_list_entries",
    "filesystem_query_entry_info",
    "filesystem_query_read_text",
    "filesystem_edit_write_text",
    "filesystem_edit_append_text",
    "filesystem_edit_copy_entry",
    "filesystem_edit_move_entry",
    "filesystem_edit_delete_entry",
    "filesystem_stats_directory",
    "processing_list_providers",
    "processing_get_algorithms",
    "processing_get_parameter_template",
    "processing_execute_algorithm",
    "processing_execute_on_layers",
)
NON_TOOL_PUBLIC_HELPER_NAMES: tuple[str, ...] = (
    "get_project_snapshot",
    "get_layers_summary",
    "get_shapefile_workflow_template",
    "get_shapefile_quality_profile",
    "get_shapefile_run_summary",
)

def _ensure_processing_initialized() -> None:
    """确保 processing initialized 已就绪。"""
    global _PROCESSING_INITIALIZED
    if _PROCESSING_INITIALIZED:
        return
    Processing.initialize()
    _PROCESSING_INITIALIZED = True

def _tool_doc(
    purpose: str,
    inputs: str,
    preconditions: str,
    effects: str,
    safety: str,
    returns: str,
) -> str:
    """执行 tool doc 相关逻辑。"""
    return (
        f"用途：{purpose} 输入语义：{inputs} 前置条件：{preconditions} "
        f"主要副作用：{effects} 安全开关：{safety} 返回结果：{returns}"
    )

def _apply_registered_tool_docstrings() -> None:
    """Apply validated tool docstrings onto public tool entry methods."""
    for tool_name in REGISTERED_TOOL_NAMES:
        method = getattr(ProcessingMCPTools, tool_name, None)
        if not callable(method):
            raise RuntimeError(f"Cannot set tool docstring, missing method: {tool_name}")
        method.__doc__ = _REGISTERED_TOOL_DOCSTRINGS[tool_name].strip()

class ProcessingMCPTools:
    DEFAULT_FEATURE_LIMIT = 10
    MAX_FEATURE_LIMIT = 100
    DEFAULT_DATASET_LIMIT = 50
    MAX_DATASET_LIMIT = 100
    DEFAULT_FILESYSTEM_LIST_LIMIT = 100
    MAX_FILESYSTEM_LIST_LIMIT = 200
    DEFAULT_ALGORITHM_LIST_LIMIT = 30
    MAX_ALGORITHM_LIST_LIMIT = 60

    # Limits and initialization.
    def __init__(
        self,
        iface,
        runner: McpMainThreadRunner,
        config: ProcessingMCPServerConfig | None = None,
    ):
        """初始化 ProcessingMCPTools 实例状态。"""
        self._iface = iface
        self._runner = runner
        self._config = config
        self._shapefile_workflow_template = self._build_shapefile_workflow_template()
        self._shapefile_quality_profile = self._build_shapefile_quality_profile()
        self._shapefile_run_summary = self._empty_shapefile_run_summary()

    # Public MCP tool entry wrappers.
    def common_get_qgis_info(self) -> dict[str, Any]:
        """返回 QGIS info 信息。"""
        return self._run(self._common_get_qgis_info_impl)

    def vector_add_layer(self, path: str, provider: str = "ogr", name: str | None = None) -> dict[str, Any]:
        """执行矢量相关的 add layer 逻辑。"""
        return self._run(self._vector_add_layer_impl, path, provider, name)

    def vector_add_layers(self, paths: list[str], provider: str = "ogr", skip_invalid: bool = True) -> dict[str, Any]:
        """执行矢量相关的 add layers 逻辑。"""
        return self._run(self._vector_add_layers_impl, paths, provider, skip_invalid)

    def raster_add_layer(self, path: str, provider: str = "gdal", name: str | None = None) -> dict[str, Any]:
        """执行栅格相关的 add layer 逻辑。"""
        return self._run(self._raster_add_layer_impl, path, provider, name)

    def raster_add_layers(self, paths: list[str], provider: str = "gdal", skip_invalid: bool = True) -> dict[str, Any]:
        """执行栅格相关的 add layers 逻辑。"""
        return self._run(self._raster_add_layers_impl, paths, provider, skip_invalid)

    def layer_list(self, layer_types: str = "both", include_hidden: bool = True, name_glob: str = "*") -> list[dict[str, Any]]:
        """执行图层相关的 list 逻辑。"""
        return self._run(self._layer_list_impl, layer_types, include_hidden, name_glob)

    def layer_get_panel_tree(self, include_hidden: bool = True) -> dict[str, Any]:
        """执行图层相关的 get panel tree 逻辑。"""
        return self._run(self._layer_get_panel_tree_impl, include_hidden)

    def layer_get_details(self, layer_ref: str) -> dict[str, Any]:
        """执行图层相关的 get details 逻辑。"""
        return self._run(self._layer_get_details_impl, layer_ref)

    def layer_remove(self, layer_id: str) -> dict[str, str]:
        """执行图层相关的 remove 逻辑。"""
        return self._run(self._layer_remove_impl, layer_id)

    def layer_remove_batch(self, layer_ids: list[str]) -> dict[str, list[str]]:
        """执行图层相关的 remove batch 逻辑。"""
        return self._run(self._layer_remove_batch_impl, layer_ids)

    def layer_resolve_references(self, refs: list[str], strict: bool = False) -> dict[str, Any]:
        """执行图层相关的 resolve references 逻辑。"""
        return self._run(self._layer_resolve_references_impl, refs, strict)

    def vector_get_layer_features(self, layer_ref: str, limit: int = DEFAULT_FEATURE_LIMIT) -> dict[str, Any]:
        """执行矢量相关的 get layer features 逻辑。"""
        return self._run(self._vector_get_layer_features_impl, layer_ref, limit)

    def vector_table_add_field(self, layer_ref: str, field_name: str, field_type: str = "string", field_length: int = 0, field_precision: int = 0, in_place: bool = False) -> dict[str, Any]:
        """执行矢量相关的 table add field 逻辑。"""
        return self._run(self._vector_table_add_field_impl, layer_ref, field_name, field_type, field_length, field_precision, in_place)

    def vector_table_drop_fields(self, layer_ref: str, fields: list[str], in_place: bool = False) -> dict[str, Any]:
        """执行矢量相关的 table drop fields 逻辑。"""
        return self._run(self._vector_table_drop_fields_impl, layer_ref, fields, in_place)

    def vector_table_rename_field(self, layer_ref: str, field_name: str, new_field_name: str, in_place: bool = False) -> dict[str, Any]:
        """执行矢量相关的 table rename field 逻辑。"""
        return self._run(self._vector_table_rename_field_impl, layer_ref, field_name, new_field_name, in_place)

    def vector_table_calculate_field(self, layer_ref: str, field_name: str, expression: str, field_type: str = "string", where: str | None = None, in_place: bool = False) -> dict[str, Any]:
        """执行矢量相关的 table calculate field 逻辑。"""
        return self._run(self._vector_table_calculate_field_impl, layer_ref, field_name, expression, field_type, where, in_place)

    def vector_table_query_records(self, layer_ref: str, where: str | None = None, fields: list[str] | None = None, limit: int = DEFAULT_FEATURE_LIMIT, offset: int = 0, order_by: str | None = None, include_geometry: bool = False) -> dict[str, Any]:
        """执行矢量相关的 table query records 逻辑。"""
        return self._run(self._vector_table_query_records_impl, layer_ref, where, fields, limit, offset, order_by, include_geometry)

    def vector_table_insert_records(self, layer_ref: str, records: list[dict[str, Any]], in_place: bool = False) -> dict[str, Any]:
        """执行矢量相关的 table insert records 逻辑。"""
        return self._run(self._vector_table_insert_records_impl, layer_ref, records, in_place)

    def vector_table_update_records(self, layer_ref: str, where: str | None = None, set_literals: dict[str, Any] | None = None, set_expressions: dict[str, str] | None = None, in_place: bool = False) -> dict[str, Any]:
        """执行矢量相关的 table update records 逻辑。"""
        return self._run(self._vector_table_update_records_impl, layer_ref, where, set_literals, set_expressions, in_place)

    def vector_table_delete_records(self, layer_ref: str, where: str | None = None, in_place: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
        """执行矢量相关的 table delete records 逻辑。"""
        return self._run(self._vector_table_delete_records_impl, layer_ref, where, in_place, confirm_destructive)

    def vector_table_truncate(self, layer_ref: str, in_place: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
        """执行矢量相关的 table truncate 逻辑。"""
        return self._run(self._vector_table_truncate_impl, layer_ref, in_place, confirm_destructive)

    def vector_stats_basic(self, layer_ref: str, field_name: str) -> dict[str, Any]:
        """执行矢量相关的 stats basic 逻辑。"""
        return self._run(self._vector_stats_basic_impl, layer_ref, field_name)

    def vector_stats_by_categories(self, layer_ref: str, category_fields: list[str], values_field: str | None = None) -> dict[str, Any]:
        """执行矢量相关的 stats by categories 逻辑。"""
        return self._run(self._vector_stats_by_categories_impl, layer_ref, category_fields, values_field)

    def raster_stats_basic(self, layer_ref: str, band: int = 1) -> dict[str, Any]:
        """执行栅格相关的 stats basic 逻辑。"""
        return self._run(self._raster_stats_basic_impl, layer_ref, band)

    def raster_stats_zonal(self, vector_layer_ref: str, raster_layer_ref: str, raster_band: int = 1, column_prefix: str = "z_", in_place: bool = False) -> dict[str, Any]:
        """执行栅格相关的 stats zonal 逻辑。"""
        return self._run(self._raster_stats_zonal_impl, vector_layer_ref, raster_layer_ref, raster_band, column_prefix, in_place)

    def raster_stats_cell(self, raster_layer_refs: list[str], statistic: int = 0, ignore_nodata: bool = True) -> dict[str, Any]:
        """执行栅格相关的 stats cell 逻辑。"""
        return self._run(self._raster_stats_cell_impl, raster_layer_refs, statistic, ignore_nodata)

    def dataset_list_files(self, directory: str, recursive: bool = False, dataset_kind: str = "both", geometry_type: str = "any", name_glob: str = "*", limit: int = DEFAULT_DATASET_LIMIT) -> dict[str, Any]:
        """执行数据集相关的 list files 逻辑。"""
        return self._run(self._dataset_list_files_impl, directory, recursive, dataset_kind, geometry_type, name_glob, limit)

    def dataset_load_from_directory(self, directory: str, recursive: bool = False, dataset_kind: str = "both", geometry_type: str = "any", name_glob: str = "*", limit: int = DEFAULT_DATASET_LIMIT, skip_invalid: bool = True) -> dict[str, Any]:
        """执行数据集相关的 load from directory 逻辑。"""
        return self._run(self._dataset_load_from_directory_impl, directory, recursive, dataset_kind, geometry_type, name_glob, limit, skip_invalid)

    def dataset_inspect_shapefile_bundle(self, path: str, recursive: bool = False, name_glob: str = "*.shp", limit: int = DEFAULT_DATASET_LIMIT, task_name: str = "") -> dict[str, Any]:
        """执行数据集相关的 inspect shapefile bundle 逻辑。"""
        return self._run(self._dataset_inspect_shapefile_bundle_impl, path, recursive, name_glob, limit, task_name)

    def vector_check_validity_report(self, layer_ref: str = "", path: str = "", required_fields: list[str] | None = None, expected_crs: str | None = None, task_name: str = "") -> dict[str, Any]:
        """执行矢量相关的 check validity report 逻辑。"""
        return self._run(self._vector_check_validity_report_impl, layer_ref, path, required_fields, expected_crs, task_name)

    def vector_prepare_work_layer(self, layer_ref: str = "", path: str = "", task_name: str = "", target_crs: str | None = None, normalize_field_names: bool = False, multipart_policy: str = "keep") -> dict[str, Any]:
        """执行矢量相关的 prepare work layer 逻辑。"""
        return self._run(self._vector_prepare_work_layer_impl, layer_ref, path, task_name, target_crs, normalize_field_names, multipart_policy)

    def vector_export_shapefile(self, layer_ref: str, output_directory: str, file_name: str = "", task_name: str = "", overwrite: bool = False, auto_truncate_field_names: bool = True, confirm_write: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
        """执行矢量相关的 export shapefile 逻辑。"""
        return self._run(self._vector_export_shapefile_impl, layer_ref, output_directory, file_name, task_name, overwrite, auto_truncate_field_names, confirm_write, confirm_destructive)

    def project_cleanup_work_layers(self, task_name: str = "", layer_ids: list[str] | None = None, temp_paths: list[str] | None = None, delete_temp_files: bool = True, confirm_write: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
        """执行项目相关的 cleanup work layers 逻辑。"""
        return self._run(self._project_cleanup_work_layers_impl, task_name, layer_ids, temp_paths, delete_temp_files, confirm_write, confirm_destructive)

    def filesystem_query_list_entries(self, directory: str, recursive: bool = False, include_files: bool = True, include_directories: bool = True, name_glob: str = "*", limit: int = DEFAULT_FILESYSTEM_LIST_LIMIT) -> dict[str, Any]:
        """执行文件系统相关的 query list entries 逻辑。"""
        return self._run(self._filesystem_query_list_entries_impl, directory, recursive, include_files, include_directories, name_glob, limit)

    def filesystem_query_entry_info(self, path: str) -> dict[str, Any]:
        """执行文件系统相关的 query entry info 逻辑。"""
        return self._run(self._filesystem_query_entry_info_impl, path)

    def filesystem_query_read_text(self, path: str, max_chars: int | None = None) -> dict[str, Any]:
        """执行文件系统相关的 query read text 逻辑。"""
        return self._run(self._filesystem_query_read_text_impl, path, max_chars)

    def filesystem_edit_write_text(self, path: str, content: str, overwrite: bool = False, confirm_destructive: bool = False, create_parents: bool = True, confirm_write: bool = False) -> dict[str, Any]:
        """执行文件系统相关的 edit write text 逻辑。"""
        return self._run(self._filesystem_edit_write_text_impl, path, content, overwrite, confirm_destructive, create_parents, confirm_write)

    def filesystem_edit_append_text(self, path: str, content: str, create_parents: bool = True, confirm_write: bool = False) -> dict[str, Any]:
        """执行文件系统相关的 edit append text 逻辑。"""
        return self._run(self._filesystem_edit_append_text_impl, path, content, create_parents, confirm_write)

    def filesystem_edit_copy_entry(self, source_path: str, target_path: str, overwrite: bool = False, confirm_destructive: bool = False, confirm_write: bool = False) -> dict[str, Any]:
        """执行文件系统相关的 edit copy entry 逻辑。"""
        return self._run(self._filesystem_edit_copy_entry_impl, source_path, target_path, overwrite, confirm_destructive, confirm_write)

    def filesystem_edit_move_entry(self, source_path: str, target_path: str, overwrite: bool = False, confirm_destructive: bool = False, confirm_write: bool = False) -> dict[str, Any]:
        """执行文件系统相关的 edit move entry 逻辑。"""
        return self._run(self._filesystem_edit_move_entry_impl, source_path, target_path, overwrite, confirm_destructive, confirm_write)

    def filesystem_edit_delete_entry(self, path: str, confirm_destructive: bool = False, confirm_write: bool = False) -> dict[str, Any]:
        """执行文件系统相关的 edit delete entry 逻辑。"""
        return self._run(self._filesystem_edit_delete_entry_impl, path, confirm_destructive, confirm_write)

    def filesystem_stats_directory(self, directory: str, recursive: bool = False) -> dict[str, Any]:
        """执行文件系统相关的 stats directory 逻辑。"""
        return self._run(self._filesystem_stats_directory_impl, directory, recursive)

    def processing_list_providers(self) -> dict[str, Any]:
        """执行 Processing 相关的 list providers 逻辑。"""
        return self._run(self._processing_list_providers_impl)

    def processing_get_algorithms(self, algorithm_id: str | None = None, provider_id: str | None = None, include_parameters: bool = False, include_outputs: bool = False, limit: int | None = None) -> dict[str, Any]:
        """执行 Processing 相关的 get algorithms 逻辑。"""
        return self._run(self._processing_get_algorithms_impl, algorithm_id, provider_id, include_parameters, include_outputs, limit)

    def processing_get_parameter_template(self, algorithm_id: str) -> dict[str, Any]:
        """执行 Processing 相关的 get parameter template 逻辑。"""
        return self._run(self._processing_get_parameter_template_impl, algorithm_id)

    def processing_execute_algorithm(self, algorithm: str, parameters: dict[str, Any], load_results: bool = True, allow_disk_write: bool = False, allow_in_place_edit: bool = False) -> dict[str, Any]:
        """执行 Processing 相关的 execute algorithm 逻辑。"""
        return self._run(self._processing_execute_algorithm_impl, algorithm, parameters, load_results, allow_disk_write, allow_in_place_edit)

    def processing_execute_on_layers(self, algorithm: str, layer_bindings: dict[str, Any], parameters: dict[str, Any], load_results: bool = True, batch_mode: bool = False, allow_disk_write: bool = False, allow_in_place_edit: bool = False) -> dict[str, Any]:
        """执行 Processing 相关的 execute on layers 逻辑。"""
        return self._run(self._processing_execute_on_layers_impl, algorithm, layer_bindings, parameters, load_results, batch_mode, allow_disk_write, allow_in_place_edit)

    # Public non-tool helpers for resources.
    def get_project_snapshot(self) -> dict[str, Any]:
        """返回 project snapshot。"""
        return self._run(self._get_project_snapshot_impl)

    def get_layers_summary(self) -> dict[str, Any]:
        """返回 layers summary。"""
        return self._run(self._get_layers_summary_impl)

    def get_shapefile_workflow_template(self) -> dict[str, Any]:
        """返回 shapefile workflow template。"""
        return self._run(self._get_shapefile_workflow_template_impl)

    def get_shapefile_quality_profile(self) -> dict[str, Any]:
        """返回 shapefile quality profile。"""
        return self._run(self._get_shapefile_quality_profile_impl)

    def get_shapefile_run_summary(self) -> dict[str, Any]:
        """返回 shapefile run summary。"""
        return self._run(self._get_shapefile_run_summary_impl)

    # Internal domain helpers.
    def _run(self, func, *args, **kwargs):
        """执行 run 相关逻辑。"""
        return self._runner.run(lambda: func(*args, **kwargs))

    @staticmethod
    def _safe_int(value: Any, default: int) -> int:
        """执行 safe int 相关逻辑。"""
        try:
            return int(value)
        except (TypeError, ValueError):
            return default

    def _normalize_feature_limit(self, limit: Any) -> tuple[int, int, bool]:
        """归一化 feature limit。"""
        requested = self._safe_int(limit, self.DEFAULT_FEATURE_LIMIT)
        normalized = max(0, requested)
        applied = min(normalized, self.MAX_FEATURE_LIMIT)
        return requested, applied, applied != normalized

    def _normalize_algorithm_list_limit(self, limit: Any | None) -> tuple[int | None, int, bool]:
        """归一化 algorithm list limit。"""
        requested = None if limit is None else self._safe_int(limit, self.DEFAULT_ALGORITHM_LIST_LIMIT)
        normalized = self.DEFAULT_ALGORITHM_LIST_LIMIT if requested is None else max(0, requested)
        applied = min(normalized, self.MAX_ALGORITHM_LIST_LIMIT)
        capped = requested is not None and normalized != applied
        return requested, applied, capped

    def _normalize_dataset_limit(self, limit: Any | None) -> tuple[int, int, bool]:
        """归一化 dataset limit。"""
        requested = self._safe_int(limit, self.DEFAULT_DATASET_LIMIT)
        normalized = max(0, requested)
        applied = min(normalized, self.MAX_DATASET_LIMIT)
        return requested, applied, applied != normalized

    def _normalize_filesystem_limit(self, limit: Any | None) -> tuple[int, int, bool]:
        """归一化 filesystem limit。"""
        requested = self._safe_int(limit, self.DEFAULT_FILESYSTEM_LIST_LIMIT)
        normalized = max(0, requested)
        applied = min(normalized, self.MAX_FILESYSTEM_LIST_LIMIT)
        return requested, applied, applied != normalized

    # Execution/runtime bridge.
    def _ensure_processing_runtime(self) -> None:
        """确保 processing runtime 已就绪。"""
        _ensure_processing_initialized()

    def _execute_processing_call(
        self, algorithm: str, parameters: dict[str, Any], load_results: bool
    ) -> dict[str, Any]:
        """执行 execute processing call 相关逻辑。"""
        self._ensure_processing_runtime()
        result = (
            processing.runAndLoadResults(algorithm, parameters)
            if load_results
            else processing.run(algorithm, parameters)
        )
        return result if isinstance(result, dict) else {"result": result}

    # Filesystem policy helpers.
    @staticmethod
    def _normalize_filesystem_path(path: str | Path) -> Path:
        """归一化 filesystem path。"""
        candidate = path if isinstance(path, Path) else Path(str(path).strip()).expanduser()
        if not candidate.is_absolute():
            candidate = Path.cwd() / candidate
        return candidate.resolve(strict=False)

    def _resolve_filesystem_query_path(self, path: str | Path) -> Path:
        """解析 filesystem query path。"""
        return self._normalize_filesystem_path(path)

    def _resolve_filesystem_write_path(self, path: str | Path) -> Path:
        """解析 filesystem write path。"""
        return self._resolve_filesystem_query_path(path)

    @staticmethod
    def _ensure_filesystem_write_confirmed(confirm_write: bool) -> None:
        """确保写操作已显式确认。"""
        if not confirm_write:
            raise Exception(
                "confirm_write must be true for filesystem_edit_* operations"
            )

    # Normalizers and serializers.
    @staticmethod
    def _normalize_dataset_kind(dataset_kind: str | None) -> str:
        """归一化 dataset kind。"""
        value = (dataset_kind or "both").strip().lower()
        if value in {"both", "all", "any"}:
            return "both"
        if value in {"vector", "raster"}:
            return value
        raise Exception(f"Invalid dataset_kind: {dataset_kind}")

    @staticmethod
    def _normalize_geometry_type_filter(geometry_type: str | None) -> str:
        """归一化 geometry type filter。"""
        value = (geometry_type or "any").strip().lower()
        if value in {"any", "point", "line", "polygon", "unknown"}:
            return value
        raise Exception(f"Invalid geometry_type: {geometry_type}")

    @staticmethod
    def _normalize_layer_types_filter(layer_types: str | None) -> str:
        """归一化 layer types filter。"""
        value = (layer_types or "both").strip().lower()
        if value in {"both", "all", "any"}:
            return "both"
        if value in {"vector", "raster"}:
            return value
        raise Exception(f"Invalid layer_types: {layer_types}")

    @staticmethod
    def _normalize_name_glob(name_glob: str | None) -> str:
        """归一化 name glob。"""
        value = (name_glob or "*").strip()
        return value or "*"

    @staticmethod
    def _resource_json(payload: Any) -> str:
        """执行 resource JSON 相关逻辑。"""
        return json.dumps(payload, ensure_ascii=False, indent=2)

    @staticmethod
    def _serialize_value(value: Any) -> Any:
        """序列化 value。"""
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
    def _safe_call(obj: object, name: str, default: Any = None) -> Any:
        """执行 safe call 相关逻辑。"""
        attr = getattr(obj, name, None)
        if callable(attr):
            try:
                return attr()
            except Exception:
                return default
        return attr if attr is not None else default

    def _serialize_parameter(self, param) -> dict[str, Any]:
        """序列化 parameter。"""
        flags = self._safe_call(param, "flags", 0) or 0
        result = {
            "name": self._safe_call(param, "name"),
            "description": self._safe_call(param, "description"),
            "type": self._safe_call(param, "type"),
            "type_name": self._safe_call(param, "typeName"),
            "default": self._serialize_value(self._safe_call(param, "defaultValue")),
            "optional": bool(flags & QgsProcessingParameterDefinition.Flag.FlagOptional),
            "hidden": bool(flags & QgsProcessingParameterDefinition.Flag.FlagHidden),
        }
        if isinstance(param, QgsProcessingParameterEnum):
            result["options"] = list(param.options())
        if hasattr(param, "allowMultiple"):
            result["allow_multiple"] = bool(self._safe_call(param, "allowMultiple", False))
        return result

    def _serialize_output(self, output_def) -> dict[str, Any]:
        """序列化 output。"""
        return {
            "name": self._safe_call(output_def, "name"),
            "description": self._safe_call(output_def, "description"),
            "type": self._safe_call(output_def, "type"),
            "type_name": self._safe_call(output_def, "typeName"),
        }

    def _serialize_algorithm(self, alg, include_parameters: bool, include_outputs: bool) -> dict[str, Any]:
        """序列化 algorithm。"""
        provider = alg.provider() if hasattr(alg, "provider") else None
        result = {
            "id": alg.id(),
            "name": alg.displayName(),
            "group": alg.group(),
            "group_id": alg.groupId(),
            "provider": provider.name() if provider else None,
            "provider_id": provider.id() if provider else None,
            "parameters_count": len(alg.parameterDefinitions()),
            "outputs_count": len(alg.outputDefinitions()),
        }
        if include_parameters:
            result["parameters"] = [self._serialize_parameter(p) for p in alg.parameterDefinitions()]
        if include_outputs:
            result["outputs"] = [self._serialize_output(o) for o in alg.outputDefinitions()]
        return result

    @staticmethod
    def _is_disk_output_key(key: str) -> bool:
        """判断 disk output key 是否成立。"""
        return "OUTPUT" in key.upper()

    @staticmethod
    def _is_disk_output_value(value: Any) -> bool:
        """判断 disk output value 是否成立。"""
        if value is None:
            return False
        if isinstance(value, Path):
            return True
        if not isinstance(value, str):
            return False
        text = value.strip()
        if not text:
            return False
        if text.upper() in {"TEMPORARY_OUTPUT", "TEMPORARY", "MEMORY:", "MEMORY"}:
            return False
        if text.startswith("memory:"):
            return False
        return True

    def _sanitize_processing_parameters(self, parameters: dict[str, Any], allow_disk_write: bool, allow_in_place_edit: bool) -> tuple[dict[str, Any], list[str]]:
        """执行 sanitize processing parameters 相关逻辑。"""
        sanitized = dict(parameters)
        warnings: list[str] = []
        for key in list(sanitized.keys()):
            upper = str(key).upper()
            value = sanitized[key]
            if upper == "IN_PLACE" and bool(value) and not allow_in_place_edit:
                sanitized[key] = False
                warnings.append("IN_PLACE was forced to false by safety policy.")
            if self._is_disk_output_key(str(key)) and not allow_disk_write and self._is_disk_output_value(value):
                sanitized[key] = "TEMPORARY_OUTPUT"
                warnings.append(f"{key} was rewritten to TEMPORARY_OUTPUT by safety policy.")
        return sanitized, warnings

    def _normalize_layer_bindings(self, layer_bindings: dict[str, Any]) -> dict[str, list[str]]:
        """归一化 layer bindings。"""
        if not isinstance(layer_bindings, dict):
            raise Exception("layer_bindings is required")
        normalized: dict[str, list[str]] = {}
        for key, raw_value in layer_bindings.items():
            name = str(key).strip()
            if not name:
                continue
            if isinstance(raw_value, (list, tuple)):
                refs = [str(item).strip() for item in raw_value if str(item).strip()]
            else:
                text = str(raw_value).strip()
                refs = [text] if text else []
            if refs:
                normalized[name] = refs
        if not normalized:
            raise Exception("layer_bindings is required")
        return normalized

    @staticmethod
    def _coerce_output_identifier(value: Any) -> str:
        """执行 coerce output identifier 相关逻辑。"""
        if value is None:
            return ""
        if isinstance(value, str):
            return value
        if hasattr(value, "id") and callable(value.id):
            try:
                return str(value.id())
            except Exception:
                pass
        return str(value)

    @staticmethod
    def _to_field_type(field_type: str) -> QVariant.Type:
        """执行 to field type 相关逻辑。"""
        value = (field_type or "string").strip().lower()
        mapping = {
            "string": QVariant.String,
            "str": QVariant.String,
            "int": QVariant.Int,
            "integer": QVariant.Int,
            "double": QVariant.Double,
            "float": QVariant.Double,
            "real": QVariant.Double,
            "bool": QVariant.Bool,
            "boolean": QVariant.Bool,
        }
        if value not in mapping:
            raise Exception(f"Unsupported field_type: {field_type}")
        return mapping[value]

    @staticmethod
    def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
        """执行 ok result 相关逻辑。"""
        payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
        if warnings is not None:
            payload["warnings"] = warnings
        payload.update(extra)
        return payload

    @staticmethod
    def _path_info(path: Path) -> dict[str, Any]:
        """执行 path info 相关逻辑。"""
        stat = path.stat()
        return {
            "path": str(path),
            "name": path.name,
            "is_file": path.is_file(),
            "is_directory": path.is_dir(),
            "size_bytes": stat.st_size if path.is_file() else 0,
            "extension": path.suffix.lower(),
            "modified_at": datetime.fromtimestamp(stat.st_mtime, tz=timezone.utc).isoformat(),
        }

    @staticmethod
    def _utc_now_iso() -> str:
        """返回 UTC ISO 时间。"""
        return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")

    @staticmethod
    def _display_geometry_type(layer: QgsVectorLayer) -> str:
        """返回矢量图层几何类型标识。"""
        return {
            Qgis.GeometryType.Point: "point",
            Qgis.GeometryType.Line: "line",
            Qgis.GeometryType.Polygon: "polygon",
            Qgis.GeometryType.Null: "null",
            Qgis.GeometryType.Unknown: "unknown",
        }.get(layer.geometryType(), "unknown")

    def _build_shapefile_workflow_template(self) -> dict[str, Any]:
        """构建 shapefile workflow template。"""
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
        """构建 shapefile quality profile。"""
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
        """创建空的 shapefile run summary。"""
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

    def _reset_shapefile_run_summary(
        self,
        task_name: str,
        status: str,
        inputs: dict[str, Any] | None = None,
    ) -> str:
        """重置 shapefile run summary。"""
        normalized = (task_name or "").strip()
        self._shapefile_run_summary = self._empty_shapefile_run_summary()
        self._shapefile_run_summary["task_name"] = normalized
        self._shapefile_run_summary["status"] = status
        if inputs:
            self._shapefile_run_summary["inputs"] = self._serialize_value(inputs)
        return normalized

    def _ensure_shapefile_run_summary(
        self,
        task_name: str = "",
        status: str | None = None,
        inputs: dict[str, Any] | None = None,
    ) -> str:
        """确保 shapefile run summary 已初始化。"""
        normalized = (task_name or self._shapefile_run_summary.get("task_name") or "").strip()
        if (
            not self._shapefile_run_summary.get("task_name")
            or (
                normalized
                and normalized != self._shapefile_run_summary.get("task_name")
            )
        ):
            return self._reset_shapefile_run_summary(
                normalized,
                status or "initialized",
                inputs=inputs,
            )

        if status:
            self._shapefile_run_summary["status"] = status
        if inputs:
            current = self._shapefile_run_summary.setdefault("inputs", {})
            current.update(self._serialize_value(inputs))
        self._shapefile_run_summary["generated_at"] = self._utc_now_iso()
        return normalized

    def _append_shapefile_run_step(
        self,
        step: str,
        summary: dict[str, Any] | None = None,
        outputs: dict[str, Any] | None = None,
        warnings: list[str] | None = None,
        status: str | None = None,
    ) -> None:
        """追加 shapefile run summary step。"""
        payload = {
            "step": step,
            "generated_at": self._utc_now_iso(),
            "summary": self._serialize_value(summary or {}),
            "outputs": self._serialize_value(outputs or {}),
        }
        if warnings:
            payload["warnings"] = list(warnings)
            existing = self._shapefile_run_summary.setdefault("warnings", [])
            existing.extend(warnings)
        self._shapefile_run_summary.setdefault("steps", []).append(payload)
        if status:
            self._shapefile_run_summary["status"] = status

    @staticmethod
    def _normalize_task_name(task_name: str | None, fallback: str = "") -> str:
        """归一化 task_name。"""
        value = (task_name or "").strip()
        if value:
            return value
        return fallback.strip() or "shapefile-task"

    @staticmethod
    def _normalize_output_stem(text: str) -> str:
        """归一化输出文件名。"""
        value = re.sub(r"[^0-9A-Za-z_]+", "_", text or "")
        value = value.strip("_")
        return value or "output"

    @staticmethod
    def _normalize_bundle_stem(text: str) -> str:
        """归一化 bundle stem。"""
        return ProcessingMCPTools._normalize_output_stem(text).lower()

    @staticmethod
    def _normalize_shapefile_field_name(
        field_name: str,
        used_names: set[str],
    ) -> str:
        """归一化 shapefile 字段名。"""
        clean = re.sub(r"[^0-9A-Za-z_]+", "_", field_name or "")
        clean = clean.strip("_") or "field"
        candidate = clean[:10]
        counter = 1
        while candidate.lower() in used_names:
            suffix = str(counter)
            candidate = f"{clean[: max(1, 10 - len(suffix))]}{suffix}"[:10]
            counter += 1
        used_names.add(candidate.lower())
        return candidate

    @staticmethod
    def _layer_custom_property(layer: QgsMapLayer, key: str, default: Any = None) -> Any:
        """读取图层 custom property。"""
        if hasattr(layer, "customProperty"):
            try:
                return layer.customProperty(key, default)
            except Exception:
                return default
        return default

    def _tag_work_layer(
        self,
        layer: QgsVectorLayer,
        task_name: str,
        source_path: str = "",
        temp_paths: list[str] | None = None,
    ) -> None:
        """为工作层打标签。"""
        layer.setCustomProperty(
            "processingmcpserver/workflow/task_name",
            self._normalize_task_name(task_name, layer.name()),
        )
        layer.setCustomProperty("processingmcpserver/workflow/is_work_layer", True)
        layer.setCustomProperty(
            "processingmcpserver/workflow/source_path",
            source_path or layer.source(),
        )
        layer.setCustomProperty(
            "processingmcpserver/workflow/temp_paths",
            json.dumps(temp_paths or [], ensure_ascii=False),
        )

    def _get_layer_temp_paths(self, layer: QgsMapLayer) -> list[str]:
        """读取图层挂载的临时路径。"""
        raw = self._layer_custom_property(
            layer,
            "processingmcpserver/workflow/temp_paths",
            "[]",
        )
        if isinstance(raw, str):
            try:
                parsed = json.loads(raw)
            except Exception:
                return []
            if isinstance(parsed, list):
                return [str(item) for item in parsed if str(item).strip()]
        if isinstance(raw, list):
            return [str(item) for item in raw if str(item).strip()]
        return []

    def _record_run_output(self, output_key: str, value: str) -> None:
        """记录运行输出列表。"""
        outputs = self._shapefile_run_summary.setdefault("outputs", {})
        bucket = outputs.setdefault(output_key, [])
        if value not in bucket:
            bucket.append(value)

    def _resolve_vector_source(
        self,
        layer_ref: str = "",
        path: str = "",
    ) -> tuple[QgsVectorLayer, str]:
        """解析矢量来源。"""
        if str(layer_ref).strip():
            layer = self._resolve_vector_layer_ref(layer_ref)
            return layer, layer.source()
        normalized_path = str(path).strip()
        if not normalized_path:
            raise Exception("layer_ref or path is required")
        source_path = self._resolve_filesystem_query_path(normalized_path)
        if not source_path.exists() or not source_path.is_file():
            raise Exception(f"Vector dataset not found: {path}")
        layer = QgsVectorLayer(str(source_path), source_path.stem, "ogr")
        if not layer.isValid():
            raise Exception(f"Vector dataset is not valid: {path}")
        return layer, str(source_path)

    @staticmethod
    def _bundle_member_path(shp_path: Path, suffix: str) -> Path:
        """返回 bundle 成员路径。"""
        return shp_path.with_suffix(suffix)

    def _inspect_shapefile_bundle_entry(self, shp_path: Path) -> dict[str, Any]:
        """检查单个 shapefile bundle。"""
        required_suffixes = [".shp", ".shx", ".dbf"]
        optional_suffixes = [".prj", ".cpg"]
        members: dict[str, dict[str, Any]] = {}
        missing_required: list[str] = []
        missing_optional: list[str] = []
        total_size_bytes = 0

        for suffix in required_suffixes + optional_suffixes:
            member_path = self._bundle_member_path(shp_path, suffix)
            exists = member_path.exists()
            members[suffix[1:]] = {
                "path": str(member_path),
                "exists": exists,
                "size_bytes": member_path.stat().st_size if exists else 0,
            }
            if exists:
                total_size_bytes += member_path.stat().st_size
            elif suffix in required_suffixes:
                missing_required.append(suffix[1:])
            else:
                missing_optional.append(suffix[1:])

        layer = QgsVectorLayer(str(shp_path), "__shapefile_probe__", "ogr")
        geometry_type = "unknown"
        crs_authid = ""
        feature_count = 0
        field_names: list[str] = []
        encoding = ""
        if layer.isValid():
            geometry_type = self._display_geometry_type(layer)
            crs_authid = layer.crs().authid() if layer.crs().isValid() else ""
            feature_count = layer.featureCount()
            field_names = [field.name() for field in layer.fields()]
            provider = layer.dataProvider()
            if provider is not None and hasattr(provider, "encoding"):
                try:
                    encoding = str(provider.encoding() or "")
                except Exception:
                    encoding = ""

        warnings: list[str] = []
        if "prj" in missing_optional:
            warnings.append("Missing .prj file; CRS exchange may be unreliable.")
        if "cpg" in missing_optional:
            warnings.append("Missing .cpg file; encoding may be ambiguous.")

        return {
            "stem": shp_path.stem,
            "bundle_path": str(shp_path),
            "complete_bundle": not missing_required,
            "missing_required": missing_required,
            "missing_optional": missing_optional,
            "members": members,
            "size_bytes": total_size_bytes,
            "geometry_type": geometry_type,
            "crs": crs_authid,
            "feature_count": feature_count,
            "field_names": field_names,
            "field_name_length_gt_10": [
                name for name in field_names if len(name) > 10
            ],
            "encoding": encoding,
            "warnings": warnings,
        }

    def _parse_target_crs(
        self, target_crs: str | None
    ) -> QgsCoordinateReferenceSystem | None:
        """解析目标 CRS。"""
        value = (target_crs or "").strip()
        if not value:
            return None
        crs = QgsCoordinateReferenceSystem()
        if not crs.createFromUserInput(value) or not crs.isValid():
            raise Exception(f"Invalid target_crs: {target_crs}")
        return crs

    def _long_field_names(self, layer: QgsVectorLayer) -> list[str]:
        """返回超出 shapefile 约束的字段名。"""
        return [field.name() for field in layer.fields() if len(field.name()) > 10]

    def _rename_layer_fields_for_shapefile(
        self, layer: QgsVectorLayer
    ) -> dict[str, str]:
        """将字段名收敛到 shapefile 约束。"""
        mapping: dict[str, str] = {}
        used_names: set[str] = set()
        for field in layer.fields():
            field_name = field.name()
            if len(field_name) <= 10:
                used_names.add(field_name.lower())
                continue
            mapping[field_name] = self._normalize_shapefile_field_name(
                field_name,
                used_names,
            )
        if not mapping:
            return {}

        def op() -> int:
            renamed = 0
            for old_name, new_name in mapping.items():
                idx = self._field_index(layer, old_name)
                if idx < 0:
                    continue
                if layer.renameAttribute(idx, new_name):
                    renamed += 1
            layer.updateFields()
            return renamed

        self._apply_vector_edit(layer, op)
        return mapping

    def _collect_null_attribute_fields(self, layer: QgsVectorLayer) -> list[str]:
        """收集存在空值的字段。"""
        flagged: list[str] = []
        field_names = [field.name() for field in layer.fields()]
        if not field_names:
            return flagged
        for field_name in field_names:
            for feature in layer.getFeatures():
                if feature.attribute(field_name) is None:
                    flagged.append(field_name)
                    break
        return flagged

    def _inspect_vector_layer_validity(
        self,
        layer: QgsVectorLayer,
        required_fields: list[str] | None = None,
        expected_crs: str | None = None,
    ) -> dict[str, Any]:
        """执行矢量图层体检。"""
        required = [str(item).strip() for item in (required_fields or []) if str(item).strip()]
        expected = self._parse_target_crs(expected_crs) if expected_crs else None
        feature_count = layer.featureCount()
        null_geometry_count = 0
        empty_geometry_count = 0
        invalid_geometry_count = 0
        duplicate_geometry_count = 0
        duplicate_record_count = 0
        multipart_feature_count = 0
        seen_geometries: set[str] = set()
        seen_records: set[tuple[Any, ...]] = set()
        field_names = [field.name() for field in layer.fields()]

        for feature in layer.getFeatures():
            record_key = tuple(
                self._serialize_value(feature.attribute(field_name))
                for field_name in field_names
            )
            if record_key in seen_records:
                duplicate_record_count += 1
            else:
                seen_records.add(record_key)

            if not feature.hasGeometry():
                null_geometry_count += 1
                continue

            geometry = feature.geometry()
            if geometry is None or geometry.isNull():
                null_geometry_count += 1
                continue
            if geometry.isEmpty():
                empty_geometry_count += 1
                continue
            try:
                if not geometry.isGeosValid():
                    invalid_geometry_count += 1
            except Exception:
                invalid_geometry_count += 1
            if geometry.isMultipart():
                multipart_feature_count += 1
            geometry_key = geometry.asWkt(precision=12)
            if geometry_key in seen_geometries:
                duplicate_geometry_count += 1
            else:
                seen_geometries.add(geometry_key)

        missing_required_fields = [
            field_name
            for field_name in required
            if self._field_index(layer, field_name) < 0
        ]
        long_field_names = self._long_field_names(layer)
        null_attribute_fields = self._collect_null_attribute_fields(layer)
        crs_authid = layer.crs().authid() if layer.crs().isValid() else ""
        crs_mismatch = bool(
            expected is not None
            and expected.isValid()
            and crs_authid
            and crs_authid != expected.authid()
        )

        issues: list[dict[str, Any]] = []
        if null_geometry_count or empty_geometry_count:
            issues.append(
                {
                    "code": "null_or_empty_geometry",
                    "severity": "error",
                    "count": null_geometry_count + empty_geometry_count,
                }
            )
        if invalid_geometry_count:
            issues.append(
                {
                    "code": "invalid_geometry",
                    "severity": "error",
                    "count": invalid_geometry_count,
                }
            )
        if duplicate_geometry_count:
            issues.append(
                {
                    "code": "duplicate_geometry",
                    "severity": "warning",
                    "count": duplicate_geometry_count,
                }
            )
        if duplicate_record_count:
            issues.append(
                {
                    "code": "duplicate_record",
                    "severity": "warning",
                    "count": duplicate_record_count,
                }
            )
        if multipart_feature_count:
            issues.append(
                {
                    "code": "multipart_feature",
                    "severity": "warning",
                    "count": multipart_feature_count,
                }
            )
        if missing_required_fields:
            issues.append(
                {
                    "code": "missing_required_fields",
                    "severity": "error",
                    "fields": missing_required_fields,
                }
            )
        if long_field_names:
            issues.append(
                {
                    "code": "field_name_length_gt_10",
                    "severity": "warning",
                    "fields": long_field_names,
                }
            )
        if crs_mismatch:
            issues.append(
                {
                    "code": "crs_mismatch",
                    "severity": "error",
                    "expected_crs": expected.authid() if expected else "",
                    "actual_crs": crs_authid,
                }
            )

        return {
            "layer_id": layer.id(),
            "name": layer.name(),
            "source": layer.source(),
            "feature_count": feature_count,
            "geometry_type": self._display_geometry_type(layer),
            "crs": crs_authid,
            "field_names": field_names,
            "null_geometry_count": null_geometry_count,
            "empty_geometry_count": empty_geometry_count,
            "invalid_geometry_count": invalid_geometry_count,
            "duplicate_geometry_count": duplicate_geometry_count,
            "duplicate_record_count": duplicate_record_count,
            "multipart_feature_count": multipart_feature_count,
            "missing_required_fields": missing_required_fields,
            "field_name_length_gt_10": long_field_names,
            "null_attribute_fields": null_attribute_fields,
            "crs_mismatch": crs_mismatch,
            "issues": issues,
            "safe_for_export": not long_field_names and not missing_required_fields,
        }

    def _remove_empty_geometries(self, layer: QgsVectorLayer) -> int:
        """删除空几何要素。"""
        def op() -> int:
            fids: list[int] = []
            for feature in layer.getFeatures():
                if not feature.hasGeometry():
                    fids.append(int(feature.id()))
                    continue
                geometry = feature.geometry()
                if geometry is None or geometry.isNull() or geometry.isEmpty():
                    fids.append(int(feature.id()))
            if not fids:
                return 0
            if not layer.deleteFeatures(fids):
                raise Exception("Failed to delete null or empty geometries")
            return len(fids)

        return int(self._apply_vector_edit(layer, op))

    def _make_layer_geometries_valid(self, layer: QgsVectorLayer) -> int:
        """修复无效几何。"""
        def op() -> int:
            fixed_count = 0
            for feature in layer.getFeatures():
                if not feature.hasGeometry():
                    continue
                geometry = feature.geometry()
                if geometry is None or geometry.isNull() or geometry.isEmpty():
                    continue
                try:
                    if geometry.isGeosValid():
                        continue
                except Exception:
                    pass
                fixed = geometry.makeValid()
                if fixed is None or fixed.isNull() or fixed.isEmpty():
                    continue
                if not layer.changeGeometry(feature.id(), fixed):
                    raise Exception(f"Failed to update geometry for feature {feature.id()}")
                fixed_count += 1
            return fixed_count

        return int(self._apply_vector_edit(layer, op))

    def _remove_duplicate_geometries(self, layer: QgsVectorLayer) -> int:
        """删除重复几何。"""
        def op() -> int:
            seen_geometries: set[str] = set()
            fids: list[int] = []
            for feature in layer.getFeatures():
                if not feature.hasGeometry():
                    continue
                geometry = feature.geometry()
                if geometry is None or geometry.isNull() or geometry.isEmpty():
                    continue
                key = geometry.asWkt(precision=12)
                if key in seen_geometries:
                    fids.append(int(feature.id()))
                else:
                    seen_geometries.add(key)
            if not fids:
                return 0
            if not layer.deleteFeatures(fids):
                raise Exception("Failed to delete duplicate geometries")
            return len(fids)

        return int(self._apply_vector_edit(layer, op))

    def _reproject_layer_in_place(
        self,
        layer: QgsVectorLayer,
        target_crs: QgsCoordinateReferenceSystem,
    ) -> int:
        """原位重投影当前图层。"""
        if not target_crs.isValid():
            return 0
        source_crs = layer.crs()
        if not source_crs.isValid() or source_crs == target_crs:
            return 0
        transform = QgsCoordinateTransform(
            source_crs,
            target_crs,
            QgsProject.instance(),
        )

        def op() -> int:
            changed = 0
            for feature in layer.getFeatures():
                if not feature.hasGeometry():
                    continue
                geometry = QgsGeometry(feature.geometry())
                if geometry.isNull() or geometry.isEmpty():
                    continue
                try:
                    geometry.transform(transform)
                except Exception as exc:
                    raise Exception(
                        f"Failed to transform feature {feature.id()}: {exc}"
                    ) from exc
                if not layer.changeGeometry(feature.id(), geometry):
                    raise Exception(f"Failed to apply transformed geometry for feature {feature.id()}")
                changed += 1
            return changed

        changed = int(self._apply_vector_edit(layer, op))
        layer.setCrs(target_crs)
        return changed

    def _coerce_vector_output_layer(
        self,
        output_value: Any,
        layer_name: str,
    ) -> QgsVectorLayer:
        """将 processing 输出解析为矢量图层。"""
        output_layer: QgsVectorLayer | None = None
        if isinstance(output_value, QgsVectorLayer):
            output_layer = output_value
        elif isinstance(output_value, QgsMapLayer) and output_value.type() == QgsMapLayer.VectorLayer:
            output_layer = output_value
        elif isinstance(output_value, str):
            text = output_value.strip()
            if text in QgsProject.instance().mapLayers():
                candidate = QgsProject.instance().mapLayer(text)
                if isinstance(candidate, QgsVectorLayer):
                    output_layer = candidate
            elif text:
                candidate = QgsVectorLayer(text, layer_name, "ogr")
                if candidate.isValid():
                    output_layer = candidate
        if output_layer is None or not output_layer.isValid():
            raise Exception("Failed to resolve vector output layer from processing result")
        if QgsProject.instance().mapLayer(output_layer.id()) is None:
            QgsProject.instance().addMapLayer(output_layer)
        output_layer.setName(layer_name)
        return output_layer

    def _explode_multipart_layer(
        self, layer: QgsVectorLayer, task_name: str
    ) -> tuple[QgsVectorLayer, int]:
        """拆解 multipart 图层。"""
        self._ensure_processing_runtime()
        result = processing.run(
            "native:multiparttosingleparts",
            {
                "INPUT": layer,
                "OUTPUT": "memory:",
            },
        )
        exploded_layer = self._coerce_vector_output_layer(
            result.get("OUTPUT"),
            self._copy_layer_name(layer, "singleparts"),
        )
        self._tag_work_layer(
            exploded_layer,
            task_name,
            source_path=self._layer_custom_property(
                layer,
                "processingmcpserver/workflow/source_path",
                layer.source(),
            ),
            temp_paths=self._get_layer_temp_paths(layer),
        )
        feature_delta = exploded_layer.featureCount() - layer.featureCount()
        QgsProject.instance().removeMapLayer(layer.id())
        return exploded_layer, feature_delta

    def _collect_cleanup_target_layers(
        self,
        task_name: str,
        layer_ids: list[str] | None,
    ) -> list[QgsMapLayer]:
        """收集待清理图层。"""
        requested_ids = {
            str(layer_id).strip()
            for layer_id in (layer_ids or [])
            if str(layer_id).strip()
        }
        normalized_task = (task_name or "").strip()
        layers: list[QgsMapLayer] = []
        for layer in QgsProject.instance().mapLayers().values():
            if requested_ids and layer.id() in requested_ids:
                layers.append(layer)
                continue
            if not normalized_task:
                continue
            if (
                self._layer_custom_property(
                    layer,
                    "processingmcpserver/workflow/task_name",
                    "",
                )
                == normalized_task
            ):
                layers.append(layer)
        unique: dict[str, QgsMapLayer] = {layer.id(): layer for layer in layers}
        return list(unique.values())

    # Layer resolution and vector edit helpers.
    @staticmethod
    def _layer_type_token(layer: QgsMapLayer) -> str:
        """执行图层相关的 type token 逻辑。"""
        if layer.type() == QgsMapLayer.VectorLayer:
            return f"vector_{int(layer.geometryType())}"
        if layer.type() == QgsMapLayer.RasterLayer:
            return "raster"
        return str(int(layer.type()))

    @staticmethod
    def _is_layer_visible(project: QgsProject, layer_id: str) -> bool:
        """判断 layer visible 是否成立。"""
        node = project.layerTreeRoot().findLayer(layer_id) if project.layerTreeRoot() else None
        if node is None:
            return False
        try:
            return bool(node.isVisible())
        except Exception:
            return False

    @staticmethod
    def _field_index(layer: QgsVectorLayer, field_name: str) -> int:
        """执行 field index 相关逻辑。"""
        return layer.fields().indexFromName(field_name)

    def _resolve_layer_ref(self, layer_ref: Any) -> QgsMapLayer:
        """解析 layer ref。"""
        if isinstance(layer_ref, QgsMapLayer):
            return layer_ref
        text = str(layer_ref).strip() if layer_ref is not None else ""
        if not text:
            raise Exception("Layer reference is required")
        layers = QgsProject.instance().mapLayers()
        if text in layers:
            return layers[text]
        matches = [layer for layer in layers.values() if layer.name() == text]
        if len(matches) == 1:
            return matches[0]
        if matches:
            raise Exception(
                "Ambiguous layer reference: "
                f"{text}. Matches: {[layer.id() for layer in matches]}"
            )
        raise Exception(f"Layer not found: {text}")

    def _resolve_vector_layer_ref(self, layer_ref: Any) -> QgsVectorLayer:
        """解析 vector layer ref。"""
        layer = self._resolve_layer_ref(layer_ref)
        if layer.type() != QgsMapLayer.VectorLayer:
            raise Exception(f"Layer is not a vector layer: {layer_ref}")
        return layer

    def _resolve_raster_layer_ref(self, layer_ref: Any) -> QgsRasterLayer:
        """解析 raster layer ref。"""
        layer = self._resolve_layer_ref(layer_ref)
        if layer.type() != QgsMapLayer.RasterLayer:
            raise Exception(f"Layer is not a raster layer: {layer_ref}")
        return layer

    @staticmethod
    def _begin_vector_edit(layer: QgsVectorLayer) -> bool:
        """执行 begin vector edit 相关逻辑。"""
        if layer.isEditable():
            return False
        if not layer.startEditing():
            raise Exception("Failed to start editing vector layer")
        return True

    @staticmethod
    def _finish_vector_edit(layer: QgsVectorLayer, started_here: bool) -> None:
        """执行 finish vector edit 相关逻辑。"""
        if not started_here:
            return
        if layer.commitChanges():
            return
        errors = "; ".join(layer.commitErrors()) if hasattr(layer, "commitErrors") else ""
        layer.rollBack()
        raise Exception(f"Failed to commit vector layer changes: {errors}")

    def _apply_vector_edit(self, layer: QgsVectorLayer, operation) -> Any:
        """执行 apply vector edit 相关逻辑。"""
        started_here = self._begin_vector_edit(layer)
        try:
            result = operation()
        except Exception:
            if started_here:
                layer.rollBack()
            raise
        self._finish_vector_edit(layer, started_here)
        return result

    @staticmethod
    def _copy_layer_name(layer: QgsMapLayer, suffix: str = "copy") -> str:
        """复制 layer name。"""
        base_name = (layer.name() or "layer").strip() or "layer"
        clean_suffix = suffix.strip().replace(" ", "_") if suffix else "copy"
        return f"{base_name}_{clean_suffix}"

    def _materialize_vector_layer(
        self, layer: QgsVectorLayer, suffix: str = "copy"
    ) -> QgsVectorLayer:
        """执行 materialize vector layer 相关逻辑。"""
        cloned_layer = layer.materialize(QgsFeatureRequest())
        if cloned_layer is None or not cloned_layer.isValid():
            raise Exception(f"Failed to materialize vector layer copy: {layer.id()}")
        cloned_layer.setName(self._copy_layer_name(layer, suffix))
        QgsProject.instance().addMapLayer(cloned_layer)
        return cloned_layer

    def _prepare_vector_target_layer(
        self, layer: QgsVectorLayer, in_place: bool, suffix: str
    ) -> tuple[QgsVectorLayer, str]:
        """执行 prepare vector target layer 相关逻辑。"""
        if in_place:
            return layer, "in_place"
        return self._materialize_vector_layer(layer, suffix), "copy"

    # Project snapshot helpers.
    def _get_project_snapshot_impl(self) -> dict[str, Any]:
        """执行 get project snapshot impl 相关逻辑。"""
        project = QgsProject.instance()
        return {
            "file_name": project.fileName(),
            "title": project.title(),
            "crs": project.crs().authid() if project.crs().isValid() else "",
            "layer_count": len(project.mapLayers()),
        }

    def _get_layers_summary_impl(self) -> dict[str, Any]:
        """执行 get layers summary impl 相关逻辑。"""
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
        """返回 shapefile workflow template。"""
        return self._serialize_value(self._shapefile_workflow_template)

    def _get_shapefile_quality_profile_impl(self) -> dict[str, Any]:
        """返回 shapefile quality profile。"""
        return self._serialize_value(self._shapefile_quality_profile)

    def _get_shapefile_run_summary_impl(self) -> dict[str, Any]:
        """返回 shapefile run summary。"""
        return self._serialize_value(self._shapefile_run_summary)

    # Tool implementation bodies (_xxx_impl).
    # Common/project.
    def _common_get_qgis_info_impl(self) -> dict[str, Any]:
        """执行 common get QGIS info impl 相关逻辑。"""
        project = QgsProject.instance()
        available = False
        try:
            self._ensure_processing_runtime()
            available = True
        except Exception:
            available = False
        active_project = {
            "file_name": project.fileName(),
            "title": project.title(),
            "crs": project.crs().authid() if project.crs().isValid() else "",
            "layer_count": len(project.mapLayers()),
        }
        plugin_names = sorted(active_plugins)
        return {
            "qgis": {
                "version": Qgis.version(),
                "prefix_path": QgsApplication.prefixPath(),
                "settings_dir": QgsApplication.qgisSettingsDirPath(),
            },
            "platform": {
                "system": platform.system(),
                "release": platform.release(),
                "machine": platform.machine(),
            },
            "python": {
                "version": platform.python_version(),
                "executable": sys.executable,
            },
            "project": active_project,
            "active_project": active_project,
            "active_plugins": plugin_names,
            "processing_mcp": {
                "available": available,
                "registered_tools_count": len(REGISTERED_TOOL_NAMES),
                "active_plugins_count": len(plugin_names),
                "filesystem": {
                    "write_policy": {
                        "require_confirm_write": True,
                        "require_confirm_destructive_for_overwrite": True,
                        "require_confirm_destructive_for_delete": True,
                    },
                },
            },
        }

    # Layer add/list/details/remove/resolve.
    def _vector_add_layer_impl(self, path: str, provider: str = "ogr", name: str | None = None) -> dict[str, Any]:
        """执行矢量相关的 add layer impl 逻辑。"""
        layer = QgsVectorLayer(path, name or Path(path).stem, provider)
        if not layer.isValid():
            raise Exception(f"Layer is not valid: {path}")
        QgsProject.instance().addMapLayer(layer)
        return {"id": layer.id(), "name": layer.name(), "type": self._layer_type_token(layer), "feature_count": layer.featureCount()}

    def _vector_add_layers_impl(self, paths: list[str], provider: str, skip_invalid: bool) -> dict[str, Any]:
        """执行矢量相关的 add layers impl 逻辑。"""
        loaded: list[dict[str, Any]] = []
        failed: list[dict[str, Any]] = []
        for path in paths or []:
            try:
                loaded.append(self._vector_add_layer_impl(path, provider=provider))
            except Exception as exc:
                if not skip_invalid:
                    raise Exception(f"Failed to load vector dataset: {path}. {exc}") from exc
                failed.append({"path": path, "error": str(exc)})
        return {"requested_count": len(paths or []), "loaded_count": len(loaded), "failed_count": len(failed), "loaded": loaded, "failed": failed}

    def _raster_add_layer_impl(self, path: str, provider: str = "gdal", name: str | None = None) -> dict[str, Any]:
        """执行栅格相关的 add layer impl 逻辑。"""
        layer = QgsRasterLayer(path, name or Path(path).stem, provider)
        if not layer.isValid():
            raise Exception(f"Layer is not valid: {path}")
        QgsProject.instance().addMapLayer(layer)
        return {"id": layer.id(), "name": layer.name(), "type": "raster", "width": layer.width(), "height": layer.height(), "band_count": layer.bandCount()}

    def _raster_add_layers_impl(self, paths: list[str], provider: str, skip_invalid: bool) -> dict[str, Any]:
        """执行栅格相关的 add layers impl 逻辑。"""
        loaded: list[dict[str, Any]] = []
        failed: list[dict[str, Any]] = []
        for path in paths or []:
            try:
                loaded.append(self._raster_add_layer_impl(path, provider=provider))
            except Exception as exc:
                if not skip_invalid:
                    raise Exception(f"Failed to load raster dataset: {path}. {exc}") from exc
                failed.append({"path": path, "error": str(exc)})
        return {"requested_count": len(paths or []), "loaded_count": len(loaded), "failed_count": len(failed), "loaded": loaded, "failed": failed}

    def _layer_list_impl(self, layer_types: str, include_hidden: bool, name_glob: str) -> list[dict[str, Any]]:
        """执行图层相关的 list impl 逻辑。"""
        normalized_types = self._normalize_layer_types_filter(layer_types)
        pattern = self._normalize_name_glob(name_glob)
        project = QgsProject.instance()
        entries: list[dict[str, Any]] = []
        for layer_id, layer in project.mapLayers().items():
            is_vector = layer.type() == QgsMapLayer.VectorLayer
            is_raster = layer.type() == QgsMapLayer.RasterLayer
            if normalized_types == "vector" and not is_vector:
                continue
            if normalized_types == "raster" and not is_raster:
                continue
            if not fnmatch(layer.name(), pattern):
                continue
            visible = self._is_layer_visible(project, layer_id)
            if not include_hidden and not visible:
                continue
            item: dict[str, Any] = {
                "id": layer_id,
                "name": layer.name(),
                "type": self._layer_type_token(layer),
                "visible": visible,
                "provider": layer.providerType() if hasattr(layer, "providerType") else "",
            }
            if is_vector:
                item["feature_count"] = layer.featureCount()
                item["fields"] = [f.name() for f in layer.fields()]
            elif is_raster:
                item["width"] = layer.width()
                item["height"] = layer.height()
                item["band_count"] = layer.bandCount()
            entries.append(item)
        return entries

    def _layer_get_panel_tree_impl(self, include_hidden: bool) -> dict[str, Any]:
        """执行图层相关的 get panel tree impl 逻辑。"""
        root = QgsProject.instance().layerTreeRoot()
        groups: list[dict[str, Any]] = []
        layers: list[dict[str, Any]] = []
        project = QgsProject.instance()

        def layer_payload(layer: QgsMapLayer) -> dict[str, Any]:
            """执行图层相关的 payload 逻辑。"""
            item: dict[str, Any] = {
                "id": layer.id(),
                "name": layer.name(),
                "type": self._layer_type_token(layer),
                "visible": self._is_layer_visible(project, layer.id()),
                "provider": layer.providerType() if hasattr(layer, "providerType") else "",
            }
            if layer.type() == QgsMapLayer.VectorLayer:
                item["feature_count"] = layer.featureCount()
                item["fields"] = [field.name() for field in layer.fields()]
            elif layer.type() == QgsMapLayer.RasterLayer:
                item["width"] = layer.width()
                item["height"] = layer.height()
                item["band_count"] = layer.bandCount()
            return item

        def walk(group: QgsLayerTreeGroup, prefix: str) -> list[dict[str, Any]]:
            """遍历当前节点及其子节点。"""
            children_payload: list[dict[str, Any]] = []
            for child in group.children():
                visible = bool(child.isVisible()) if hasattr(child, "isVisible") else True
                if not include_hidden and not visible:
                    continue
                if isinstance(child, QgsLayerTreeGroup):
                    path = f"{prefix}/{child.name()}" if prefix else child.name()
                    groups.append(
                        {
                            "name": child.name(),
                            "path": path,
                            "visible": visible,
                        }
                    )
                    group_item = {
                        "node_type": "group",
                        "name": child.name(),
                        "path": path,
                        "visible": visible,
                        "children": walk(child, path),
                    }
                    children_payload.append(group_item)
                    continue
                if not isinstance(child, QgsLayerTreeLayer):
                    continue
                layer = child.layer()
                if layer is None:
                    continue
                item = layer_payload(layer)
                layers.append(item)
                children_payload.append({"node_type": "layer", **item})
            return children_payload

        tree = {
            "node_type": "group",
            "name": "",
            "path": "",
            "visible": True,
            "children": walk(root, "") if root is not None else [],
        }
        return {"tree": tree, "groups": groups, "layers": layers}

    def _layer_get_details_impl(self, layer_ref: str) -> dict[str, Any]:
        """执行图层相关的 get details impl 逻辑。"""
        layer = self._resolve_layer_ref(layer_ref)
        result: dict[str, Any] = {
            "id": layer.id(),
            "name": layer.name(),
            "type": self._layer_type_token(layer),
            "provider": layer.providerType() if hasattr(layer, "providerType") else "",
            "source": layer.source() if hasattr(layer, "source") else "",
            "crs": layer.crs().authid() if hasattr(layer, "crs") and layer.crs().isValid() else "",
        }
        if layer.type() == QgsMapLayer.VectorLayer:
            result["feature_count"] = layer.featureCount()
            result["fields"] = [f.name() for f in layer.fields()]
        elif layer.type() == QgsMapLayer.RasterLayer:
            result["width"] = layer.width()
            result["height"] = layer.height()
            result["band_count"] = layer.bandCount()
        return result

    def _layer_remove_impl(self, layer_id: str) -> dict[str, str]:
        """执行图层相关的 remove impl 逻辑。"""
        project = QgsProject.instance()
        if layer_id not in project.mapLayers():
            raise Exception(f"Layer not found: {layer_id}")
        project.removeMapLayer(layer_id)
        return {"removed": layer_id}

    def _layer_remove_batch_impl(self, layer_ids: list[str]) -> dict[str, list[str]]:
        """执行图层相关的 remove batch impl 逻辑。"""
        project = QgsProject.instance()
        removed: list[str] = []
        missing: list[str] = []
        for layer_id in layer_ids or []:
            value = str(layer_id).strip()
            if not value:
                continue
            if value in project.mapLayers():
                project.removeMapLayer(value)
                removed.append(value)
            else:
                missing.append(value)
        return {"removed": removed, "missing": missing}

    def _layer_resolve_references_impl(self, refs: list[str], strict: bool) -> dict[str, Any]:
        """执行图层相关的 resolve references impl 逻辑。"""
        resolved: dict[str, str] = {}
        missing: list[str] = []
        ambiguous: dict[str, list[str]] = {}
        layers = QgsProject.instance().mapLayers()
        for ref in refs or []:
            text = str(ref).strip()
            if not text:
                continue
            if text in layers:
                resolved[text] = text
                continue

            matches = [layer for layer in layers.values() if layer.name() == text]
            if not matches:
                missing.append(text)
                continue
            if len(matches) > 1:
                ambiguous[text] = [layer.id() for layer in matches]
                continue
            resolved[text] = matches[0].id()
        if strict and (missing or ambiguous):
            details: list[str] = []
            if missing:
                details.append(f"missing={missing}")
            if ambiguous:
                details.append(f"ambiguous={ambiguous}")
            raise Exception("Layer reference resolve failed: " + "; ".join(details))
        return {"resolved": resolved, "missing": missing, "ambiguous": ambiguous}

    # Vector query/table/stats.
    def _vector_get_layer_features_impl(self, layer_ref: str, limit: int) -> dict[str, Any]:
        """执行矢量相关的 get layer features impl 逻辑。"""
        layer = self._resolve_vector_layer_ref(layer_ref)
        requested, applied, capped = self._normalize_feature_limit(limit)
        features: list[dict[str, Any]] = []
        for index, feature in enumerate(layer.getFeatures()):
            if index >= applied:
                break
            attrs = {
                field.name(): self._serialize_value(feature.attribute(field.name()))
                for field in layer.fields()
            }
            features.append(
                {
                    "id": feature.id(),
                    "attributes": attrs,
                    "geometry_wkt": feature.geometry().asWkt(precision=6) if feature.hasGeometry() else None,
                }
            )
        return {
            "layer_id": layer.id(),
            "feature_count": layer.featureCount(),
            "fields": [f.name() for f in layer.fields()],
            "features": features,
            "requested_limit": requested,
            "applied_limit": applied,
            "limit_capped": capped,
            "max_feature_limit": self.MAX_FEATURE_LIMIT,
        }

    def _vector_table_add_field_impl(self, layer_ref: str, field_name: str, field_type: str, field_length: int, field_precision: int, in_place: bool) -> dict[str, Any]:
        """执行矢量相关的 table add field impl 逻辑。"""
        source_layer = self._resolve_vector_layer_ref(layer_ref)
        layer, mode = self._prepare_vector_target_layer(
            source_layer, in_place, "add_field_copy"
        )
        name = field_name.strip()
        if not name:
            raise Exception("field_name is required")
        if self._field_index(layer, name) >= 0:
            raise Exception(f"Field already exists: {name}")
        field = QgsField(name, self._to_field_type(field_type), len=max(0, self._safe_int(field_length, 0)), prec=max(0, self._safe_int(field_precision, 0)))

        def op() -> int:
            """执行当前局部编辑操作。"""
            if not layer.addAttribute(field):
                raise Exception(f"Failed to add field: {name}")
            layer.updateFields()
            return 1

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result(
            "vector_table_add_field",
            summary={
                "mode": mode,
                "affected_count": affected,
                "output_layer_id": layer.id(),
            },
            outputs={"fields": [f.name() for f in layer.fields()]},
        )

    def _vector_table_drop_fields_impl(self, layer_ref: str, fields: list[str], in_place: bool) -> dict[str, Any]:
        """执行矢量相关的 table drop fields impl 逻辑。"""
        source_layer = self._resolve_vector_layer_ref(layer_ref)
        layer, mode = self._prepare_vector_target_layer(
            source_layer, in_place, "drop_fields_copy"
        )
        if not fields:
            raise Exception("fields is required")
        indexes: list[int] = []
        missing: list[str] = []
        for field_name in fields:
            idx = self._field_index(layer, field_name)
            if idx < 0:
                missing.append(field_name)
            else:
                indexes.append(idx)

        def op() -> int:
            """执行当前局部编辑操作。"""
            affected = 0
            for idx in sorted(indexes, reverse=True):
                if layer.deleteAttribute(idx):
                    affected += 1
            layer.updateFields()
            return affected

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result(
            "vector_table_drop_fields",
            summary={
                "mode": mode,
                "affected_count": affected,
                "output_layer_id": layer.id(),
            },
            outputs={
                "fields": [f.name() for f in layer.fields()],
                "missing_fields": missing,
            },
        )

    def _vector_table_rename_field_impl(self, layer_ref: str, field_name: str, new_field_name: str, in_place: bool) -> dict[str, Any]:
        """执行矢量相关的 table rename field impl 逻辑。"""
        source_layer = self._resolve_vector_layer_ref(layer_ref)
        layer, mode = self._prepare_vector_target_layer(
            source_layer, in_place, "rename_field_copy"
        )
        old_name = field_name.strip()
        new_name = new_field_name.strip()
        idx = self._field_index(layer, old_name)
        if idx < 0:
            raise Exception(f"Field not found: {old_name}")
        if self._field_index(layer, new_name) >= 0:
            raise Exception(f"Field already exists: {new_name}")

        def op() -> int:
            """执行当前局部编辑操作。"""
            if not layer.renameAttribute(idx, new_name):
                raise Exception(f"Failed to rename field {old_name} -> {new_name}")
            layer.updateFields()
            return 1

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result(
            "vector_table_rename_field",
            summary={
                "mode": mode,
                "affected_count": affected,
                "output_layer_id": layer.id(),
            },
            outputs={"fields": [f.name() for f in layer.fields()]},
        )

    def _vector_table_calculate_field_impl(self, layer_ref: str, field_name: str, expression: str, field_type: str, where: str | None, in_place: bool) -> dict[str, Any]:
        """执行矢量相关的 table calculate field impl 逻辑。"""
        source_layer = self._resolve_vector_layer_ref(layer_ref)
        layer, mode = self._prepare_vector_target_layer(
            source_layer, in_place, "calculate_field_copy"
        )
        expr = QgsExpression(expression or "")
        if expr.hasParserError():
            raise Exception(f"Invalid expression: {expr.parserErrorString()}")
        where_expr = None
        if where:
            where_expr = QgsExpression(where)
            if where_expr.hasParserError():
                raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

        idx = self._field_index(layer, field_name)
        created = False

        def op() -> int:
            """执行当前局部编辑操作。"""
            nonlocal idx
            nonlocal created
            if idx < 0:
                if not layer.addAttribute(QgsField(field_name, self._to_field_type(field_type))):
                    raise Exception(f"Failed to create field: {field_name}")
                layer.updateFields()
                idx = self._field_index(layer, field_name)
                created = True
            context = QgsExpressionContext()
            context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
            affected = 0
            for feature in layer.getFeatures():
                context.setFeature(feature)
                if where_expr is not None:
                    keep = where_expr.evaluate(context)
                    if where_expr.hasEvalError():
                        raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
                    if not bool(keep):
                        continue
                value = expr.evaluate(context)
                if expr.hasEvalError():
                    raise Exception(f"Invalid expression: {expr.evalErrorString()}")
                if not layer.changeAttributeValue(feature.id(), idx, value):
                    raise Exception(f"Failed to update feature: {feature.id()}")
                affected += 1
            return affected

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result(
            "vector_table_calculate_field",
            summary={
                "mode": mode,
                "affected_count": affected,
                "field_created": created,
                "output_layer_id": layer.id(),
            },
            outputs={"fields": [f.name() for f in layer.fields()]},
        )

    def _vector_table_query_records_impl(self, layer_ref: str, where: str | None, fields: list[str] | None, limit: int, offset: int, order_by: str | None, include_geometry: bool) -> dict[str, Any]:
        """执行矢量相关的 table query records impl 逻辑。"""
        if not str(layer_ref).strip():
            raise Exception("layer_ref is required")
        layer = self._resolve_vector_layer_ref(layer_ref)
        where_expr = None
        if where:
            where_expr = QgsExpression(where)
            if where_expr.hasParserError():
                raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

        requested, applied, capped = self._normalize_feature_limit(limit)
        applied_offset = max(0, self._safe_int(offset, 0))
        all_fields = [f.name() for f in layer.fields()]
        selected_fields = all_fields if not fields else fields
        for field_name in selected_fields:
            if self._field_index(layer, field_name) < 0:
                raise Exception(f"Field not found: {field_name}")

        context = QgsExpressionContext()
        context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
        matched: list[QgsFeature] = []
        for feature in layer.getFeatures():
            if where_expr is not None:
                context.setFeature(feature)
                keep = where_expr.evaluate(context)
                if where_expr.hasEvalError():
                    raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
                if not bool(keep):
                    continue
            matched.append(feature)

        if order_by:
            matched = sorted(matched, key=lambda f: str(f.attribute(order_by)))
        sliced = matched[applied_offset : applied_offset + applied]
        records: list[dict[str, Any]] = []
        for feature in sliced:
            item = {
                "id": feature.id(),
                "attributes": {
                    name: self._serialize_value(feature.attribute(name))
                    for name in selected_fields
                },
            }
            if include_geometry and feature.hasGeometry():
                item["geometry_wkt"] = feature.geometry().asWkt(precision=6)
            records.append(item)

        return self._ok_result("vector_table_query_records", summary={"returned": len(records), "matched_total": len(matched), "offset": applied_offset, "requested_limit": requested, "applied_limit": applied, "limit_capped": capped}, outputs={"records": records})

    def _vector_table_insert_records_impl(self, layer_ref: str, records: list[dict[str, Any]], in_place: bool) -> dict[str, Any]:
        """执行矢量相关的 table insert records impl 逻辑。"""
        source_layer = self._resolve_vector_layer_ref(layer_ref)
        layer, mode = self._prepare_vector_target_layer(
            source_layer, in_place, "insert_records_copy"
        )
        if not records:
            raise Exception("records is required")
        field_names = [field.name() for field in layer.fields()]
        field_name_set = set(field_names)
        ignored_fields: list[str] = []
        ignored_seen: set[str] = set()

        def op() -> int:
            """执行当前局部编辑操作。"""
            affected = 0
            for row in records:
                feature = QgsFeature(layer.fields())
                attrs = dict(row)
                geometry_wkt = attrs.pop("geometry_wkt", None)
                unknown_fields = sorted(
                    key for key in attrs.keys() if key not in field_name_set
                )
                for field_name in unknown_fields:
                    if field_name in ignored_seen:
                        continue
                    ignored_seen.add(field_name)
                    ignored_fields.append(field_name)
                feature.setAttributes([attrs.get(field_name) for field_name in field_names])
                if geometry_wkt:
                    geometry = QgsGeometry.fromWkt(str(geometry_wkt))
                    if geometry.isNull():
                        raise Exception(f"Invalid geometry_wkt: {geometry_wkt}")
                    feature.setGeometry(geometry)
                if not layer.addFeature(feature):
                    raise Exception("Failed to insert feature")
                affected += 1
            return affected

        affected = self._apply_vector_edit(layer, op)
        warnings: list[str] = []
        if ignored_fields:
            warnings.append("Ignored unknown fields: " + ", ".join(ignored_fields))
        return self._ok_result(
            "vector_table_insert_records",
            summary={
                "mode": mode,
                "affected_count": affected,
                "output_layer_id": layer.id(),
            },
            outputs={
                "feature_count": layer.featureCount(),
                "ignored_fields": ignored_fields,
            },
            warnings=warnings or None,
        )

    def _vector_table_update_records_impl(self, layer_ref: str, where: str | None, set_literals: dict[str, Any] | None, set_expressions: dict[str, str] | None, in_place: bool) -> dict[str, Any]:
        """执行矢量相关的 table update records impl 逻辑。"""
        source_layer = self._resolve_vector_layer_ref(layer_ref)
        layer, mode = self._prepare_vector_target_layer(
            source_layer, in_place, "update_records_copy"
        )
        literals = set_literals or {}
        expressions = set_expressions or {}
        if not literals and not expressions:
            raise Exception("Either set_literals or set_expressions must be provided")

        where_expr = None
        if where:
            where_expr = QgsExpression(where)
            if where_expr.hasParserError():
                raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

        compiled: dict[str, QgsExpression] = {}
        for key, expr_text in expressions.items():
            compiled_expr = QgsExpression(expr_text)
            if compiled_expr.hasParserError():
                raise Exception(f"Invalid expression for {key}: {compiled_expr.parserErrorString()}")
            compiled[key] = compiled_expr

        field_indexes: dict[str, int] = {}
        for key in set(literals.keys()) | set(expressions.keys()):
            idx = self._field_index(layer, key)
            if idx < 0:
                raise Exception(f"Field not found: {key}")
            field_indexes[key] = idx

        def op() -> int:
            """执行当前局部编辑操作。"""
            context = QgsExpressionContext()
            context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
            affected = 0
            for feature in layer.getFeatures():
                context.setFeature(feature)
                if where_expr is not None:
                    keep = where_expr.evaluate(context)
                    if where_expr.hasEvalError():
                        raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
                    if not bool(keep):
                        continue
                for key, value in literals.items():
                    if not layer.changeAttributeValue(feature.id(), field_indexes[key], value):
                        raise Exception(f"Failed to update field {key}")
                for key, compiled_expr in compiled.items():
                    value = compiled_expr.evaluate(context)
                    if compiled_expr.hasEvalError():
                        raise Exception(f"Invalid expression for {key}: {compiled_expr.evalErrorString()}")
                    if not layer.changeAttributeValue(feature.id(), field_indexes[key], value):
                        raise Exception(f"Failed to update field {key}")
                affected += 1
            return affected

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result(
            "vector_table_update_records",
            summary={
                "mode": mode,
                "affected_count": affected,
                "output_layer_id": layer.id(),
            },
            outputs={"feature_count": layer.featureCount()},
        )

    def _vector_table_delete_records_impl(self, layer_ref: str, where: str | None, in_place: bool, confirm_destructive: bool) -> dict[str, Any]:
        """执行矢量相关的 table delete records impl 逻辑。"""
        if not confirm_destructive:
            raise Exception("confirm_destructive must be true for delete operation")
        source_layer = self._resolve_vector_layer_ref(layer_ref)
        layer, mode = self._prepare_vector_target_layer(
            source_layer, in_place, "delete_records_copy"
        )
        where_expr = None
        if where:
            where_expr = QgsExpression(where)
            if where_expr.hasParserError():
                raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

        def op() -> int:
            """执行当前局部编辑操作。"""
            context = QgsExpressionContext()
            context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
            fids: list[int] = []
            for feature in layer.getFeatures():
                if where_expr is not None:
                    context.setFeature(feature)
                    keep = where_expr.evaluate(context)
                    if where_expr.hasEvalError():
                        raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
                    if not bool(keep):
                        continue
                fids.append(int(feature.id()))
            if not fids:
                return 0
            if not layer.deleteFeatures(fids):
                raise Exception("Failed to delete records")
            return len(fids)

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result(
            "vector_table_delete_records",
            summary={
                "mode": mode,
                "affected_count": affected,
                "output_layer_id": layer.id(),
            },
            outputs={"feature_count": layer.featureCount()},
        )

    def _vector_table_truncate_impl(self, layer_ref: str, in_place: bool, confirm_destructive: bool) -> dict[str, Any]:
        """执行矢量相关的 table truncate impl 逻辑。"""
        if not confirm_destructive:
            raise Exception("confirm_destructive must be true for truncate operation")
        source_layer = self._resolve_vector_layer_ref(layer_ref)
        layer, mode = self._prepare_vector_target_layer(
            source_layer, in_place, "truncate_records_copy"
        )

        def op() -> int:
            """执行当前局部编辑操作。"""
            fids = [int(feature.id()) for feature in layer.getFeatures()]
            if not fids:
                return 0
            if not layer.deleteFeatures(fids):
                raise Exception("Failed to truncate records")
            return len(fids)

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result(
            "vector_table_truncate",
            summary={
                "mode": mode,
                "affected_count": affected,
                "output_layer_id": layer.id(),
            },
            outputs={"feature_count": layer.featureCount()},
        )

    def _vector_stats_basic_impl(self, layer_ref: str, field_name: str) -> dict[str, Any]:
        """执行矢量相关的 stats basic impl 逻辑。"""
        layer = self._resolve_vector_layer_ref(layer_ref)
        if self._field_index(layer, field_name) < 0:
            raise Exception(f"Invalid field: {field_name}")
        values: list[float] = []
        for feature in layer.getFeatures():
            value = feature.attribute(field_name)
            if value is None:
                continue
            try:
                values.append(float(value))
            except (TypeError, ValueError):
                continue
        if not values:
            raise Exception(f"No numeric values found for field: {field_name}")
        return self._ok_result(
            "vector_stats_basic",
            summary={
                "count": len(values),
                "min": min(values),
                "max": max(values),
                "sum": sum(values),
                "mean": sum(values) / len(values),
                "stdev": statistics.stdev(values) if len(values) > 1 else 0.0,
            },
            outputs={"field_name": field_name},
        )

    def _vector_stats_by_categories_impl(self, layer_ref: str, category_fields: list[str], values_field: str | None) -> dict[str, Any]:
        """执行矢量相关的 stats by categories impl 逻辑。"""
        layer = self._resolve_vector_layer_ref(layer_ref)
        if not category_fields:
            raise Exception("category_fields is required")
        for field_name in category_fields:
            if self._field_index(layer, field_name) < 0:
                raise Exception(f"Field not found: {field_name}")
        if values_field and self._field_index(layer, values_field) < 0:
            raise Exception(f"Field not found: {values_field}")

        grouped: dict[tuple[Any, ...], dict[str, Any]] = {}
        for feature in layer.getFeatures():
            key = tuple(feature.attribute(name) for name in category_fields)
            bucket = grouped.setdefault(key, {"count": 0, "sum": 0.0, "values_count": 0})
            bucket["count"] += 1
            if values_field:
                try:
                    bucket["sum"] += float(feature.attribute(values_field))
                    bucket["values_count"] += 1
                except (TypeError, ValueError):
                    pass

        groups: list[dict[str, Any]] = []
        for key, bucket in grouped.items():
            item = {
                "categories": {
                    category_fields[i]: self._serialize_value(key[i])
                    for i in range(len(category_fields))
                },
                "count": bucket["count"],
            }
            if values_field and bucket["values_count"] > 0:
                item["sum"] = bucket["sum"]
                item["mean"] = bucket["sum"] / bucket["values_count"]
            groups.append(item)

        return self._ok_result("vector_stats_by_categories", summary={"group_count": len(groups), "input_feature_count": layer.featureCount()}, outputs={"groups": groups})

    # Raster stats.
    def _raster_stats_basic_impl(self, layer_ref: str, band: int) -> dict[str, Any]:
        """执行栅格相关的 stats basic impl 逻辑。"""
        layer = self._resolve_raster_layer_ref(layer_ref)
        band_number = max(1, self._safe_int(band, 1))
        stats = layer.dataProvider().bandStatistics(band_number)
        return self._ok_result("raster_stats_basic", summary={"band": band_number, "count": int(stats.elementCount), "min": float(stats.minimumValue), "max": float(stats.maximumValue), "mean": float(stats.mean), "std_dev": float(stats.stdDev)}, outputs={"layer_id": layer.id()})

    def _raster_stats_zonal_impl(self, vector_layer_ref: str, raster_layer_ref: str, raster_band: int, column_prefix: str, in_place: bool) -> dict[str, Any]:
        """执行栅格相关的 stats zonal impl 逻辑。"""
        vector_layer = self._resolve_vector_layer_ref(vector_layer_ref)
        raster_layer = self._resolve_raster_layer_ref(raster_layer_ref)
        self._ensure_processing_runtime()
        algorithm_id = "native:zonalstatisticsfb"
        if QgsApplication.processingRegistry().algorithmById(algorithm_id) is None:
            raise Exception(f"Algorithm not available: {algorithm_id}")

        parameters = {
            "INPUT": vector_layer,
            "INPUT_RASTER": raster_layer,
            "RASTER_BAND": max(1, self._safe_int(raster_band, 1)),
            "COLUMN_PREFIX": column_prefix or "z_",
            "OUTPUT": vector_layer if in_place else "TEMPORARY_OUTPUT",
        }
        result = processing.run(algorithm_id, parameters)
        output_layer_id = self._coerce_output_identifier(result.get("OUTPUT"))
        if not output_layer_id and in_place:
            output_layer_id = vector_layer.id()
        return self._ok_result(
            "raster_stats_zonal",
            summary={
                "mode": "in_place" if in_place else "copy",
                "algorithm": algorithm_id,
                "output_layer_id": output_layer_id,
            },
            outputs={"result": self._serialize_value(result)},
        )

    def _raster_stats_cell_impl(self, raster_layer_refs: list[str], statistic: int, ignore_nodata: bool) -> dict[str, Any]:
        """执行栅格相关的 stats cell impl 逻辑。"""
        if not raster_layer_refs:
            raise Exception("raster_layer_refs is required")
        layers = [self._resolve_raster_layer_ref(ref) for ref in raster_layer_refs]
        self._ensure_processing_runtime()
        result = processing.run(
            "native:cellstatistics",
            {
                "INPUT": layers,
                "STATISTIC": self._safe_int(statistic, 0),
                "IGNORE_NODATA": bool(ignore_nodata),
                "REFERENCE_LAYER": layers[0],
                "OUTPUT_NODATA_VALUE": -9999,
                "OUTPUT": "TEMPORARY_OUTPUT",
            },
        )
        return self._ok_result("raster_stats_cell", summary={"input_count": len(layers), "output": self._coerce_output_identifier(result.get("OUTPUT"))}, outputs={"result": self._serialize_value(result)})

    @staticmethod
    def _detect_dataset_kind(path: Path) -> str | None:
        """检测 dataset kind。"""
        vector_exts = {".shp", ".gpkg", ".geojson", ".json", ".kml", ".gml", ".sqlite", ".csv"}
        raster_exts = {".tif", ".tiff", ".img", ".vrt", ".asc", ".jp2", ".png", ".jpg", ".jpeg"}
        suffix = path.suffix.lower()
        if suffix in vector_exts:
            return "vector"
        if suffix in raster_exts:
            return "raster"
        return None

    @staticmethod
    def _detect_vector_geometry_type(path: Path) -> str:
        """检测 vector geometry type。"""
        layer = QgsVectorLayer(str(path), "__probe__", "ogr")
        if not layer.isValid():
            return "unknown"
        return {0: "point", 1: "line", 2: "polygon"}.get(int(layer.geometryType()), "unknown")

    # Dataset scan/load.
    def _dataset_list_files_impl(self, directory: str, recursive: bool, dataset_kind: str, geometry_type: str, name_glob: str, limit: int) -> dict[str, Any]:
        """执行数据集相关的 list files impl 逻辑。"""
        root = Path(directory)
        if not root.exists() or not root.is_dir():
            raise Exception(f"Directory not found: {directory}")
        kind_filter = self._normalize_dataset_kind(dataset_kind)
        geom_filter = self._normalize_geometry_type_filter(geometry_type)
        pattern = self._normalize_name_glob(name_glob)
        requested, applied, capped = self._normalize_dataset_limit(limit)

        iterator = root.rglob("*") if recursive else root.iterdir()
        datasets: list[dict[str, Any]] = []
        matched_total = 0
        for entry in iterator:
            if not entry.is_file() or not fnmatch(entry.name, pattern):
                continue
            detected_kind = self._detect_dataset_kind(entry)
            if detected_kind is None:
                continue
            if kind_filter != "both" and detected_kind != kind_filter:
                continue
            detected_geom = self._detect_vector_geometry_type(entry) if detected_kind == "vector" else "unknown"
            if geom_filter != "any" and detected_kind == "vector" and detected_geom != geom_filter:
                continue
            matched_total += 1
            if len(datasets) >= applied:
                continue
            datasets.append({"path": str(entry), "name": entry.name, "dataset_kind": detected_kind, "geometry_type": detected_geom, "size_bytes": entry.stat().st_size})

        return {"directory": str(root), "requested_limit": requested, "applied_limit": applied, "limit_capped": capped, "returned": len(datasets), "matched_total": matched_total, "truncated": matched_total > len(datasets), "datasets": datasets}

    def _dataset_load_from_directory_impl(self, directory: str, recursive: bool, dataset_kind: str, geometry_type: str, name_glob: str, limit: int, skip_invalid: bool) -> dict[str, Any]:
        """执行数据集相关的 load from directory impl 逻辑。"""
        list_result = self._dataset_list_files_impl(directory, recursive, dataset_kind, geometry_type, name_glob, limit)
        loaded: list[dict[str, Any]] = []
        failed: list[dict[str, Any]] = []
        for dataset in list_result["datasets"]:
            path = dataset["path"]
            try:
                if dataset["dataset_kind"] == "vector":
                    loaded.append(self._vector_add_layer_impl(path, provider="ogr"))
                else:
                    loaded.append(self._raster_add_layer_impl(path, provider="gdal"))
            except Exception as exc:
                if not skip_invalid:
                    raise Exception(f"Failed to load dataset from directory: {path}. {exc}") from exc
                failed.append({"path": path, "error": str(exc)})
        return {"requested_count": len(list_result["datasets"]), "loaded_count": len(loaded), "failed_count": len(failed), "loaded": loaded, "failed": failed}

    def _dataset_inspect_shapefile_bundle_impl(
        self,
        path: str,
        recursive: bool,
        name_glob: str,
        limit: int,
        task_name: str,
    ) -> dict[str, Any]:
        """执行数据集相关的 inspect shapefile bundle impl 逻辑。"""
        target = self._resolve_filesystem_query_path(path)
        if not target.exists():
            raise Exception(f"Path not found: {path}")
        pattern = self._normalize_name_glob(name_glob or "*.shp")
        requested, applied, capped = self._normalize_dataset_limit(limit)

        if target.is_file():
            if target.suffix.lower() != ".shp":
                raise Exception(f"Shapefile path is required: {path}")
            candidates = [target]
        else:
            iterator = target.rglob("*.shp") if recursive else target.glob("*.shp")
            candidates = sorted(
                entry
                for entry in iterator
                if entry.is_file() and fnmatch(entry.name, pattern)
            )

        bundles: list[dict[str, Any]] = []
        matched_total = 0
        blocking_bundle_count = 0
        warning_bundle_count = 0
        for candidate in candidates:
            matched_total += 1
            if len(bundles) >= applied:
                continue
            bundle = self._inspect_shapefile_bundle_entry(candidate)
            if not bundle["complete_bundle"]:
                blocking_bundle_count += 1
            if bundle["warnings"]:
                warning_bundle_count += 1
            bundles.append(bundle)

        effective_task = ""
        if task_name.strip():
            effective_task = self._ensure_shapefile_run_summary(
                self._normalize_task_name(task_name, target.stem),
                status="prechecked",
                inputs={
                    "input_path": str(target),
                    "recursive": bool(recursive),
                    "name_glob": pattern,
                },
            )
            self._append_shapefile_run_step(
                "inspect_shapefile_bundle",
                summary={
                    "returned": len(bundles),
                    "matched_total": matched_total,
                    "blocking_bundle_count": blocking_bundle_count,
                    "warning_bundle_count": warning_bundle_count,
                },
                outputs={"bundles": bundles},
                status="prechecked",
            )

        return self._ok_result(
            "dataset_inspect_shapefile_bundle",
            summary={
                "task_name": effective_task,
                "returned": len(bundles),
                "matched_total": matched_total,
                "requested_limit": requested,
                "applied_limit": applied,
                "limit_capped": capped,
                "blocking_bundle_count": blocking_bundle_count,
                "warning_bundle_count": warning_bundle_count,
            },
            outputs={
                "path": str(target),
                "bundles": bundles,
            },
        )

    def _vector_check_validity_report_impl(
        self,
        layer_ref: str,
        path: str,
        required_fields: list[str] | None,
        expected_crs: str | None,
        task_name: str,
    ) -> dict[str, Any]:
        """执行矢量相关的 check validity report impl 逻辑。"""
        layer, source_path = self._resolve_vector_source(layer_ref=layer_ref, path=path)
        report = self._inspect_vector_layer_validity(
            layer,
            required_fields=required_fields,
            expected_crs=expected_crs,
        )
        blocking_issue_count = len(
            [issue for issue in report["issues"] if issue.get("severity") == "error"]
        )
        warning_issue_count = len(
            [issue for issue in report["issues"] if issue.get("severity") == "warning"]
        )
        effective_task = ""
        if task_name.strip():
            effective_task = self._ensure_shapefile_run_summary(
                self._normalize_task_name(task_name, layer.name() or Path(source_path).stem),
                status="validated",
                inputs={
                    "layer_ref": str(layer_ref).strip(),
                    "path": str(path).strip(),
                    "expected_crs": expected_crs or "",
                    "required_fields": list(required_fields or []),
                },
            )
            self._append_shapefile_run_step(
                "vector_check_validity_report",
                summary={
                    "layer_id": report["layer_id"],
                    "blocking_issue_count": blocking_issue_count,
                    "warning_issue_count": warning_issue_count,
                    "safe_for_export": report["safe_for_export"],
                },
                outputs={"report": report},
                status="validated",
            )

        warnings = [
            "Layer contains duplicate geometries."
            for issue in report["issues"]
            if issue.get("code") == "duplicate_geometry"
        ]
        warnings.extend(
            [
                "Layer contains duplicate records."
                for issue in report["issues"]
                if issue.get("code") == "duplicate_record"
            ]
        )
        warnings.extend(
            [
                "Layer contains multipart features."
                for issue in report["issues"]
                if issue.get("code") == "multipart_feature"
            ]
        )
        warnings = list(dict.fromkeys(warnings))

        return self._ok_result(
            "vector_check_validity_report",
            summary={
                "task_name": effective_task,
                "layer_id": report["layer_id"],
                "feature_count": report["feature_count"],
                "blocking_issue_count": blocking_issue_count,
                "warning_issue_count": warning_issue_count,
                "safe_for_export": report["safe_for_export"],
            },
            outputs={
                "source_path": source_path,
                "report": report,
            },
            warnings=warnings,
        )

    def _vector_prepare_work_layer_impl(
        self,
        layer_ref: str,
        path: str,
        task_name: str,
        target_crs: str | None,
        normalize_field_names: bool,
        multipart_policy: str,
    ) -> dict[str, Any]:
        """执行矢量相关的 prepare work layer impl 逻辑。"""
        policy = (multipart_policy or "keep").strip().lower() or "keep"
        if policy not in {"keep", "singleparts"}:
            raise Exception(
                "multipart_policy must be one of: keep, singleparts"
            )

        source_layer, source_path = self._resolve_vector_source(layer_ref=layer_ref, path=path)
        effective_task = self._ensure_shapefile_run_summary(
            self._normalize_task_name(task_name, source_layer.name() or Path(source_path).stem),
            status="preparing",
            inputs={
                "layer_ref": str(layer_ref).strip(),
                "path": str(path).strip(),
                "target_crs": target_crs or "",
                "normalize_field_names": bool(normalize_field_names),
                "multipart_policy": policy,
            },
        )
        initial_report = self._inspect_vector_layer_validity(source_layer)
        work_layer = self._materialize_vector_layer(
            source_layer,
            f"{self._normalize_output_stem(effective_task)}_work",
        )
        work_layer.setName(
            self._copy_layer_name(
                source_layer,
                f"{self._normalize_output_stem(effective_task)}_work",
            )
        )
        self._tag_work_layer(
            work_layer,
            effective_task,
            source_path=source_path,
            temp_paths=[],
        )

        removed_empty_geometry_count = self._remove_empty_geometries(work_layer)
        fixed_invalid_geometry_count = self._make_layer_geometries_valid(work_layer)
        removed_duplicate_geometry_count = self._remove_duplicate_geometries(work_layer)
        target_crs_obj = self._parse_target_crs(target_crs) if target_crs else None
        reprojected_feature_count = 0
        if target_crs_obj is not None and target_crs_obj.isValid():
            reprojected_feature_count = self._reproject_layer_in_place(work_layer, target_crs_obj)
        field_name_mapping: dict[str, str] = {}
        if normalize_field_names:
            field_name_mapping = self._rename_layer_fields_for_shapefile(work_layer)
        multipart_split_feature_delta = 0
        if policy == "singleparts":
            current_report = self._inspect_vector_layer_validity(work_layer)
            if current_report["multipart_feature_count"]:
                work_layer, multipart_split_feature_delta = self._explode_multipart_layer(
                    work_layer,
                    effective_task,
                )
        final_report = self._inspect_vector_layer_validity(
            work_layer,
            expected_crs=target_crs or None,
        )
        self._record_run_output("work_layer_ids", work_layer.id())
        self._append_shapefile_run_step(
            "vector_prepare_work_layer",
            summary={
                "output_layer_id": work_layer.id(),
                "removed_empty_geometry_count": removed_empty_geometry_count,
                "fixed_invalid_geometry_count": fixed_invalid_geometry_count,
                "removed_duplicate_geometry_count": removed_duplicate_geometry_count,
                "reprojected_feature_count": reprojected_feature_count,
                "multipart_split_feature_delta": multipart_split_feature_delta,
                "field_rename_count": len(field_name_mapping),
            },
            outputs={
                "initial_report": initial_report,
                "final_report": final_report,
                "output_layer_id": work_layer.id(),
                "field_name_mapping": field_name_mapping,
            },
            warnings=[
                "Work layer still has field names longer than 10 characters."
                for _ in final_report["field_name_length_gt_10"]
                if not normalize_field_names
            ],
            status="prepared",
        )

        warnings: list[str] = []
        if final_report["field_name_length_gt_10"]:
            warnings.append("Work layer still contains field names longer than 10 characters.")
        if final_report["multipart_feature_count"] and policy != "singleparts":
            warnings.append("Work layer still contains multipart features.")
        if final_report["duplicate_record_count"]:
            warnings.append("Work layer still contains duplicate records.")

        return self._ok_result(
            "vector_prepare_work_layer",
            summary={
                "task_name": effective_task,
                "output_layer_id": work_layer.id(),
                "feature_count": final_report["feature_count"],
                "crs": final_report["crs"],
                "removed_empty_geometry_count": removed_empty_geometry_count,
                "fixed_invalid_geometry_count": fixed_invalid_geometry_count,
                "removed_duplicate_geometry_count": removed_duplicate_geometry_count,
                "reprojected_feature_count": reprojected_feature_count,
                "multipart_split_feature_delta": multipart_split_feature_delta,
                "field_rename_count": len(field_name_mapping),
            },
            outputs={
                "source_path": source_path,
                "output_layer_id": work_layer.id(),
                "output_layer_name": work_layer.name(),
                "field_name_mapping": field_name_mapping,
                "initial_report": initial_report,
                "final_report": final_report,
            },
            warnings=warnings,
        )

    def _vector_export_shapefile_impl(
        self,
        layer_ref: str,
        output_directory: str,
        file_name: str,
        task_name: str,
        overwrite: bool,
        auto_truncate_field_names: bool,
        confirm_write: bool,
        confirm_destructive: bool,
    ) -> dict[str, Any]:
        """执行矢量相关的 export shapefile impl 逻辑。"""
        self._ensure_filesystem_write_confirmed(confirm_write)
        layer = self._resolve_vector_layer_ref(layer_ref)
        output_dir = self._resolve_filesystem_write_path(output_directory)
        output_dir.mkdir(parents=True, exist_ok=True)

        effective_task = self._ensure_shapefile_run_summary(
            self._normalize_task_name(task_name, layer.name()),
            status="exporting",
            inputs={
                "output_directory": str(output_dir),
                "file_name": str(file_name).strip(),
                "overwrite": bool(overwrite),
                "auto_truncate_field_names": bool(auto_truncate_field_names),
            },
        )
        requested_stem = Path(str(file_name).strip()).stem if str(file_name).strip() else ""
        output_stem = self._normalize_output_stem(
            requested_stem or effective_task or layer.name()
        )
        target_path = output_dir / f"{output_stem}.shp"
        if target_path.exists() and not overwrite:
            raise Exception(f"Output shapefile already exists: {target_path}")
        if target_path.exists() and overwrite and not confirm_destructive:
            raise Exception(
                "confirm_destructive must be true when overwrite is enabled"
            )

        preflight_report = self._inspect_vector_layer_validity(layer)
        blocking_errors: list[str] = []
        if preflight_report["null_geometry_count"] or preflight_report["empty_geometry_count"]:
            blocking_errors.append("Layer contains null or empty geometries.")
        if preflight_report["invalid_geometry_count"]:
            blocking_errors.append("Layer contains invalid geometries.")
        if blocking_errors:
            raise Exception(" ".join(blocking_errors))

        export_layer = layer
        export_copy_id = ""
        field_name_mapping: dict[str, str] = {}
        created_export_copy = False
        try:
            long_field_names = self._long_field_names(layer)
            if long_field_names:
                if not auto_truncate_field_names:
                    raise Exception(
                        "Layer contains field names longer than 10 characters and "
                        "auto_truncate_field_names is false"
                    )
                export_layer = self._materialize_vector_layer(layer, "shapefile_export")
                created_export_copy = True
                export_copy_id = export_layer.id()
                field_name_mapping = self._rename_layer_fields_for_shapefile(export_layer)
                self._tag_work_layer(
                    export_layer,
                    effective_task,
                    source_path=layer.source(),
                    temp_paths=[],
                )

            if target_path.exists():
                QgsVectorFileWriter.deleteShapeFile(str(target_path))

            options = QgsVectorFileWriter.SaveVectorOptions()
            options.driverName = "ESRI Shapefile"
            options.fileEncoding = "UTF-8"
            options.layerName = output_stem
            if overwrite:
                options.actionOnExistingFile = (
                    QgsVectorFileWriter.ActionOnExistingFile.CreateOrOverwriteFile
                )

            error_code, error_message, new_file_name, new_layer = (
                QgsVectorFileWriter.writeAsVectorFormatV3(
                    export_layer,
                    str(target_path),
                    QgsProject.instance().transformContext(),
                    options,
                )
            )
            if error_code != QgsVectorFileWriter.WriterError.NoError:
                raise Exception(
                    f"Failed to export shapefile: {error_message or error_code}"
                )

            output_members = sorted(
                str(item)
                for item in output_dir.glob(f"{target_path.stem}.*")
                if item.is_file()
            )
            final_report = self._inspect_vector_layer_validity(export_layer)
            self._record_run_output("final_shapefiles", str(target_path))
            self._append_shapefile_run_step(
                "vector_export_shapefile",
                summary={
                    "output_path": str(target_path),
                    "sidecar_count": len(output_members),
                    "field_rename_count": len(field_name_mapping),
                    "export_copy_id": export_copy_id,
                },
                outputs={
                    "output_path": str(target_path),
                    "output_members": output_members,
                    "field_name_mapping": field_name_mapping,
                    "preflight_report": preflight_report,
                    "final_report": final_report,
                    "new_file_name": str(new_file_name),
                    "new_layer_name": str(new_layer),
                },
                warnings=[
                    "Exported shapefile contains null attribute values."
                    for _ in final_report["null_attribute_fields"]
                ],
                status="exported",
            )
            del new_layer

            warnings = list(
                dict.fromkeys(
                    [
                        "Exported shapefile contains null attribute values."
                        for _ in final_report["null_attribute_fields"]
                    ]
                )
            )
            return self._ok_result(
                "vector_export_shapefile",
                summary={
                    "task_name": effective_task,
                    "output_path": str(target_path),
                    "sidecar_count": len(output_members),
                    "field_rename_count": len(field_name_mapping),
                },
                outputs={
                    "output_path": str(target_path),
                    "output_members": output_members,
                    "field_name_mapping": field_name_mapping,
                    "preflight_report": preflight_report,
                    "final_report": final_report,
                },
                warnings=warnings,
            )
        finally:
            if created_export_copy and QgsProject.instance().mapLayer(export_layer.id()) is not None:
                QgsProject.instance().removeMapLayer(export_layer.id())

    def _project_cleanup_work_layers_impl(
        self,
        task_name: str,
        layer_ids: list[str] | None,
        temp_paths: list[str] | None,
        delete_temp_files: bool,
        confirm_write: bool,
        confirm_destructive: bool,
    ) -> dict[str, Any]:
        """执行工程相关的 cleanup work layers impl 逻辑。"""
        normalized_task = (task_name or "").strip()
        target_layers = self._collect_cleanup_target_layers(normalized_task, layer_ids)
        collected_temp_paths: list[str] = []
        for layer in target_layers:
            collected_temp_paths.extend(self._get_layer_temp_paths(layer))
        collected_temp_paths.extend(
            [
                str(item).strip()
                for item in (temp_paths or [])
                if str(item).strip()
            ]
        )
        unique_temp_paths = list(dict.fromkeys(collected_temp_paths))

        removed_layer_ids: list[str] = []
        for layer in target_layers:
            removed_layer_ids.append(layer.id())
            QgsProject.instance().removeMapLayer(layer.id())

        deleted_temp_paths: list[str] = []
        missing_temp_paths: list[str] = []
        if delete_temp_files and unique_temp_paths:
            self._ensure_filesystem_write_confirmed(confirm_write)
            if not confirm_destructive:
                raise Exception(
                    "confirm_destructive must be true when delete_temp_files is enabled"
                )
            for raw_path in unique_temp_paths:
                candidate = self._resolve_filesystem_write_path(raw_path)
                if candidate.suffix.lower() == ".shp":
                    if candidate.exists():
                        if not QgsVectorFileWriter.deleteShapeFile(str(candidate)):
                            raise Exception(f"Failed to delete shapefile bundle: {candidate}")
                        deleted_temp_paths.append(str(candidate))
                    else:
                        missing_temp_paths.append(str(candidate))
                    continue
                if candidate.is_dir():
                    shutil.rmtree(candidate)
                    deleted_temp_paths.append(str(candidate))
                    continue
                if candidate.exists():
                    candidate.unlink()
                    deleted_temp_paths.append(str(candidate))
                else:
                    missing_temp_paths.append(str(candidate))

        if normalized_task:
            self._ensure_shapefile_run_summary(normalized_task, status="cleaned")
        self._append_shapefile_run_step(
            "project_cleanup_work_layers",
            summary={
                "removed_layer_count": len(removed_layer_ids),
                "deleted_temp_path_count": len(deleted_temp_paths),
                "missing_temp_path_count": len(missing_temp_paths),
            },
            outputs={
                "removed_layer_ids": removed_layer_ids,
                "deleted_temp_paths": deleted_temp_paths,
                "missing_temp_paths": missing_temp_paths,
            },
            status="cleaned",
        )

        return self._ok_result(
            "project_cleanup_work_layers",
            summary={
                "task_name": normalized_task,
                "removed_layer_count": len(removed_layer_ids),
                "deleted_temp_path_count": len(deleted_temp_paths),
                "missing_temp_path_count": len(missing_temp_paths),
            },
            outputs={
                "removed_layer_ids": removed_layer_ids,
                "deleted_temp_paths": deleted_temp_paths,
                "missing_temp_paths": missing_temp_paths,
            },
        )

    # Filesystem query/edit/stats.
    def _filesystem_query_list_entries_impl(self, directory: str, recursive: bool, include_files: bool, include_directories: bool, name_glob: str, limit: int) -> dict[str, Any]:
        """执行文件系统相关的 query list entries impl 逻辑。"""
        if not include_files and not include_directories:
            raise Exception("include_files and include_directories cannot both be false")
        root = self._resolve_filesystem_query_path(directory)
        if not root.exists() or not root.is_dir():
            raise Exception(f"Directory not found: {directory}")
        pattern = self._normalize_name_glob(name_glob)
        requested, applied, capped = self._normalize_filesystem_limit(limit)
        iterator = root.rglob("*") if recursive else root.iterdir()
        entries: list[dict[str, Any]] = []
        matched_total = 0
        for entry in iterator:
            if entry.is_file() and not include_files:
                continue
            if entry.is_dir() and not include_directories:
                continue
            if not fnmatch(entry.name, pattern):
                continue
            matched_total += 1
            if len(entries) >= applied:
                continue
            info = self._path_info(entry)
            info["relative_path"] = str(entry.relative_to(root))
            entries.append(info)
        return self._ok_result("filesystem_query_list_entries", summary={"returned_count": len(entries), "matched_total": matched_total, "requested_limit": requested, "applied_limit": applied, "limit_capped": capped, "truncated": matched_total > len(entries)}, outputs={"directory": str(root), "entries": entries})

    def _filesystem_query_entry_info_impl(self, path: str) -> dict[str, Any]:
        """执行文件系统相关的 query entry info impl 逻辑。"""
        entry = self._resolve_filesystem_query_path(path)
        if not entry.exists():
            raise Exception(f"Path not found: {path}")
        return self._ok_result("filesystem_query_entry_info", summary={"exists": True}, outputs={"entry": self._path_info(entry)})

    def _filesystem_query_read_text_impl(self, path: str, max_chars: int | None) -> dict[str, Any]:
        """执行文件系统相关的 query read text impl 逻辑。"""
        entry = self._resolve_filesystem_query_path(path)
        if not entry.exists() or not entry.is_file():
            raise Exception(f"File not found: {path}")
        if max_chars is None:
            text = entry.read_text(encoding="utf-8")
            return self._ok_result("filesystem_query_read_text", summary={"truncated": False, "max_chars": None}, outputs={"text": text})
        applied = max(0, self._safe_int(max_chars, 0))
        with entry.open("r", encoding="utf-8") as handle:
            text = handle.read(applied + 1)
        return self._ok_result("filesystem_query_read_text", summary={"truncated": len(text) > applied, "max_chars": applied}, outputs={"text": text[:applied]})

    def _filesystem_edit_write_text_impl(self, path: str, content: str, overwrite: bool, confirm_destructive: bool, create_parents: bool, confirm_write: bool) -> dict[str, Any]:
        """执行文件系统相关的 edit write text impl 逻辑。"""
        self._ensure_filesystem_write_confirmed(confirm_write)
        target = self._resolve_filesystem_write_path(path)
        if target.exists():
            if overwrite:
                if not confirm_destructive:
                    raise Exception("confirm_destructive must be true when overwrite is enabled")
            else:
                raise Exception(f"File already exists: {path}")
        if not target.parent.exists():
            if create_parents:
                target.parent.mkdir(parents=True, exist_ok=True)
            else:
                raise Exception(f"Parent directory not found: {target.parent}")
        target.write_text(content, encoding="utf-8")
        return self._ok_result("filesystem_edit_write_text", summary={"written_chars": len(content), "path": str(target)})

    def _filesystem_edit_append_text_impl(self, path: str, content: str, create_parents: bool, confirm_write: bool) -> dict[str, Any]:
        """执行文件系统相关的 edit append text impl 逻辑。"""
        self._ensure_filesystem_write_confirmed(confirm_write)
        target = self._resolve_filesystem_write_path(path)
        if not target.parent.exists():
            if create_parents:
                target.parent.mkdir(parents=True, exist_ok=True)
            else:
                raise Exception(f"Parent directory not found: {target.parent}")
        with target.open("a", encoding="utf-8") as handle:
            handle.write(content)
        return self._ok_result("filesystem_edit_append_text", summary={"appended_chars": len(content), "path": str(target)})

    def _filesystem_edit_copy_entry_impl(self, source_path: str, target_path: str, overwrite: bool, confirm_destructive: bool, confirm_write: bool) -> dict[str, Any]:
        """执行文件系统相关的 edit copy entry impl 逻辑。"""
        self._ensure_filesystem_write_confirmed(confirm_write)
        source = self._resolve_filesystem_query_path(source_path)
        target = self._resolve_filesystem_write_path(target_path)
        if not source.exists():
            raise Exception(f"Source path not found: {source_path}")
        if target.exists():
            if not overwrite:
                raise Exception(f"Target already exists: {target_path}")
            if not confirm_destructive:
                raise Exception("confirm_destructive must be true when overwrite is enabled")
            if target.is_dir():
                shutil.rmtree(target)
            else:
                target.unlink()
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source, target) if source.is_dir() else shutil.copy2(source, target)
        return self._ok_result("filesystem_edit_copy_entry", summary={"source_path": str(source), "target_path": str(target)})

    def _filesystem_edit_move_entry_impl(self, source_path: str, target_path: str, overwrite: bool, confirm_destructive: bool, confirm_write: bool) -> dict[str, Any]:
        """执行文件系统相关的 edit move entry impl 逻辑。"""
        self._ensure_filesystem_write_confirmed(confirm_write)
        source = self._resolve_filesystem_write_path(source_path)
        target = self._resolve_filesystem_write_path(target_path)
        if not source.exists():
            raise Exception(f"Source path not found: {source_path}")
        if target.exists():
            if not overwrite:
                raise Exception(f"Target already exists: {target_path}")
            if not confirm_destructive:
                raise Exception("confirm_destructive must be true when overwrite is enabled")
            if target.is_dir():
                shutil.rmtree(target)
            else:
                target.unlink()
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(source), str(target))
        return self._ok_result("filesystem_edit_move_entry", summary={"source_path": str(source), "target_path": str(target)})

    def _filesystem_edit_delete_entry_impl(self, path: str, confirm_destructive: bool, confirm_write: bool) -> dict[str, Any]:
        """执行文件系统相关的 edit delete entry impl 逻辑。"""
        self._ensure_filesystem_write_confirmed(confirm_write)
        if not confirm_destructive:
            raise Exception("confirm_destructive must be true for delete operation")
        target = self._resolve_filesystem_write_path(path)
        if not target.exists():
            raise Exception(f"Path not found: {path}")
        shutil.rmtree(target) if target.is_dir() else target.unlink()
        return self._ok_result("filesystem_edit_delete_entry", summary={"deleted_path": str(target)})

    def _filesystem_stats_directory_impl(self, directory: str, recursive: bool) -> dict[str, Any]:
        """执行文件系统相关的 stats directory impl 逻辑。"""
        root = self._resolve_filesystem_query_path(directory)
        if not root.exists() or not root.is_dir():
            raise Exception(f"Directory not found: {directory}")
        iterator = root.rglob("*") if recursive else root.iterdir()
        file_count = 0
        directory_count = 0
        total_size_bytes = 0
        extensions: dict[str, int] = {}
        for entry in iterator:
            if entry.is_dir():
                directory_count += 1
                continue
            file_count += 1
            total_size_bytes += entry.stat().st_size
            ext = entry.suffix.lower() or "<no_ext>"
            extensions[ext] = extensions.get(ext, 0) + 1
        return self._ok_result("filesystem_stats_directory", summary={"file_count": file_count, "directory_count": directory_count, "total_size_bytes": total_size_bytes}, outputs={"extensions": extensions})

    # Processing metadata/execute.
    def _processing_list_providers_impl(self) -> dict[str, Any]:
        """执行 Processing 相关的 list providers impl 逻辑。"""
        self._ensure_processing_runtime()
        providers = list(QgsApplication.processingRegistry().providers())
        payload = []
        total_algorithm_count = 0
        active_provider_count = 0
        active_algorithm_count = 0
        for provider in providers:
            active = True
            if hasattr(provider, "isActive") and callable(provider.isActive):
                try:
                    active = bool(provider.isActive())
                except Exception:
                    active = True
            algorithm_count = 0
            if hasattr(provider, "algorithms") and callable(provider.algorithms):
                try:
                    algorithm_count = len(provider.algorithms())
                except Exception:
                    algorithm_count = 0
            total_algorithm_count += algorithm_count
            if active:
                active_provider_count += 1
                active_algorithm_count += algorithm_count
            payload.append(
                {
                    "id": provider.id(),
                    "name": provider.name(),
                    "active": active,
                    "algorithm_count": algorithm_count,
                }
            )
        return {
            "count": len(payload),
            "count_scope": "registered_providers",
            "total_algorithm_count": total_algorithm_count,
            "active_provider_count": active_provider_count,
            "active_algorithm_count": active_algorithm_count,
            "providers": payload,
        }

    def _processing_get_algorithms_impl(self, algorithm_id: str | None, provider_id: str | None, include_parameters: bool, include_outputs: bool, limit: int | None) -> dict[str, Any]:
        """执行 Processing 相关的 get algorithms impl 逻辑。"""
        self._ensure_processing_runtime()
        registry = QgsApplication.processingRegistry()
        if algorithm_id:
            algorithm = registry.algorithmById(algorithm_id)
            if algorithm is None:
                raise Exception(f"Algorithm not found: {algorithm_id}")
            return {"algorithm": self._serialize_algorithm(algorithm, include_parameters=True, include_outputs=True)}

        algorithms = list(registry.algorithms())
        if provider_id:
            text = provider_id.strip().lower()
            algorithms = [a for a in algorithms if a.provider() and (a.provider().id().lower() == text or a.provider().name().lower() == text)]

        requested, applied, capped = self._normalize_algorithm_list_limit(limit)
        total = len(algorithms)
        if total > applied:
            algorithms = algorithms[:applied]
        return {
            "count": total,
            "returned": len(algorithms),
            "truncated": total > len(algorithms),
            "requested_limit": requested,
            "applied_limit": applied,
            "limit_capped": capped,
            "algorithms": [self._serialize_algorithm(a, include_parameters=include_parameters, include_outputs=include_outputs) for a in algorithms],
        }

    def _processing_get_parameter_template_impl(self, algorithm_id: str) -> dict[str, Any]:
        """执行 Processing 相关的 get parameter template impl 逻辑。"""
        self._ensure_processing_runtime()
        algorithm = QgsApplication.processingRegistry().algorithmById(algorithm_id)
        if algorithm is None:
            raise Exception(f"Algorithm not found: {algorithm_id}")
        required: list[dict[str, Any]] = []
        optional: list[dict[str, Any]] = []
        for parameter in algorithm.parameterDefinitions():
            item = self._serialize_parameter(parameter)
            (optional if item.get("optional") else required).append(item)
        return {
            "algorithm": {"id": algorithm.id(), "name": algorithm.displayName()},
            "required_parameters": required,
            "optional_parameters": optional,
            "outputs": [self._serialize_output(o) for o in algorithm.outputDefinitions()],
        }

    def _processing_execute_algorithm_impl(self, algorithm: str, parameters: dict[str, Any], load_results: bool, allow_disk_write: bool, allow_in_place_edit: bool) -> dict[str, Any]:
        """执行 Processing 相关的 execute algorithm impl 逻辑。"""
        if not isinstance(parameters, dict):
            raise Exception("parameters must be an object")
        effective, warnings = self._sanitize_processing_parameters(parameters, allow_disk_write, allow_in_place_edit)
        result = self._execute_processing_call(algorithm, effective, load_results)
        return self._ok_result("processing_execute_algorithm", summary={"algorithm": algorithm, "load_results": bool(load_results)}, outputs={"result": self._serialize_value(result)}, warnings=warnings, safety_policy={"allow_disk_write": bool(allow_disk_write), "allow_in_place_edit": bool(allow_in_place_edit)}, effective_parameters=self._serialize_value(effective))

    def _processing_execute_on_layers_impl(self, algorithm: str, layer_bindings: dict[str, Any], parameters: dict[str, Any], load_results: bool, batch_mode: bool, allow_disk_write: bool, allow_in_place_edit: bool) -> dict[str, Any]:
        """执行 Processing 相关的 execute on layers impl 逻辑。"""
        if not isinstance(parameters, dict):
            raise Exception("parameters must be an object")
        normalized_bindings = self._normalize_layer_bindings(layer_bindings)
        run_count = max(len(refs) for refs in normalized_bindings.values()) if batch_mode else 1

        success_count = 0
        failure_count = 0
        runs: list[dict[str, Any]] = []
        all_warnings: list[str] = []
        last_effective = dict(parameters)

        for index in range(run_count):
            try:
                effective = dict(parameters)
                for binding_name, refs in normalized_bindings.items():
                    if batch_mode and index >= len(refs):
                        raise Exception(f"layer binding '{binding_name}' missing value for batch index {index}")
                    ref = refs[index] if batch_mode else refs[0]
                    effective[binding_name] = self._resolve_layer_ref(ref).id()

                effective, warnings = self._sanitize_processing_parameters(effective, allow_disk_write, allow_in_place_edit)
                last_effective = effective
                all_warnings.extend(warnings)
                result = self._execute_processing_call(algorithm, effective, load_results)
                success_count += 1
                runs.append({"index": index, "ok": True, "result": self._serialize_value(result)})
            except Exception as exc:
                failure_count += 1
                runs.append({"index": index, "ok": False, "error": str(exc)})
                if not batch_mode:
                    raise

        return {
            "ok": failure_count == 0,
            "tool": "processing_execute_on_layers",
            "run_count": run_count,
            "success_count": success_count,
            "failure_count": failure_count,
            "runs": runs,
            "safety_policy": {"allow_disk_write": bool(allow_disk_write), "allow_in_place_edit": bool(allow_in_place_edit)},
            "warnings": all_warnings,
            "effective_parameters": self._serialize_value(last_effective),
        }

_REGISTERED_TOOL_DOCSTRINGS: dict[str, str] = {

    "common_get_qgis_info": _tool_doc(
        "返回当前 QGIS Desktop 会话、平台、插件与 Processing MCP 运行状态，适合作为所有自动化流程的环境探测入口。",
        "无业务输入。",
        "QGIS Desktop 已启动且 processingmcpserver 插件已加载。",
        "无写操作，只读取当前应用、项目与插件状态。",
        "无。",
        "返回 qgis、platform、python、active_project、active_plugins，以及 processing_mcp.filesystem.write_policy 安全摘要等环境信息。",
    ),
    "vector_add_layer": _tool_doc(
        "把单个矢量数据源加载到当前 QGIS 工程。",
        "path 指向矢量数据文件或数据源，provider 默认 ogr，name 可覆盖图层显示名。",
        "目标路径必须存在且能被对应 provider 正常识别。",
        "会向当前工程新增一个矢量图层，但不会改写源数据文件。",
        "无 destructive 开关；若路径无效会直接报错并中止加载。",
        "返回新图层的 id、name、type 与 feature_count，便于后续继续引用。",
    ),
    "vector_add_layers": _tool_doc(
        "批量把多个矢量数据源加载到当前 QGIS 工程。",
        "paths 是待加载路径数组，provider 默认 ogr，skip_invalid 控制遇到坏数据时是跳过还是整体失败。",
        "至少提供一个可访问路径，且对应数据源应能被 provider 识别。",
        "会向当前工程新增多个矢量图层，但不会改写源数据文件。",
        "skip_invalid=true 时会尽量继续加载其余路径；skip_invalid=false 时任何一个失败都会报错终止。",
        "返回 requested_count、loaded_count、failed_count，以及 loaded 和 failed 的逐项结果。",
    ),
    "raster_add_layer": _tool_doc(
        "把单个栅格数据源加载到当前 QGIS 工程。",
        "path 指向栅格文件或数据源，provider 默认 gdal，name 可覆盖图层显示名。",
        "目标路径必须存在且能被 GDAL 或指定 provider 识别。",
        "会向当前工程新增一个栅格图层，但不会改写源数据文件。",
        "无 destructive 开关；路径或数据源无效时直接失败。",
        "返回新图层的 id、name、type、width、height 与 band_count。",
    ),
    "raster_add_layers": _tool_doc(
        "批量把多个栅格数据源加载到当前 QGIS 工程。",
        "paths 是待加载路径数组，provider 默认 gdal，skip_invalid 控制遇到坏数据时是跳过还是整体失败。",
        "至少提供一个可访问路径，且对应数据源应能被 provider 识别。",
        "会向当前工程新增多个栅格图层，但不会改写源数据文件。",
        "skip_invalid=true 时会跳过坏数据继续执行；skip_invalid=false 时任一失败都会中止。",
        "返回 requested_count、loaded_count、failed_count，以及 loaded 和 failed 的逐项信息。",
    ),
    "layer_list": _tool_doc(
        "列出当前工程中可见或全部图层的基础清单，用于给模型建立当前项目上下文。",
        "layer_types 可选 vector、raster 或 both，include_hidden 控制是否包含图层面板中隐藏图层，name_glob 用于名称通配过滤。",
        "当前工程中至少存在已加载图层时结果更有意义，但空工程也可调用。",
        "无写操作，只读取工程图层注册表与图层树可见性。",
        "无。",
        "返回按过滤条件匹配的图层数组，元素包含 id、name、type、visible、provider 以及矢量或栅格的补充摘要。",
    ),
    "layer_get_panel_tree": _tool_doc(
        "读取图层面板树结构，帮助模型理解分组、顺序和隐藏状态。",
        "include_hidden 控制是否把隐藏图层与分组也放入返回结构。",
        "当前工程已初始化即可调用，最好已有图层树内容。",
        "无写操作，只读取 QgsLayerTree 结构。",
        "无。",
        "返回 tree 主结构，以及为兼容旧调用保留的 groups、layers 扁平摘要，便于模型按图层面板顺序推理。",
    ),
    "layer_get_details": _tool_doc(
        "按图层 id 或图层名读取单个图层的详细元数据。",
        "layer_ref 可以是唯一 layer id，也可以是能唯一解析的图层名称。",
        "目标图层必须已经加载到当前工程，名称引用不能歧义。",
        "无写操作，只读取图层元数据与字段摘要。",
        "无。",
        "返回 id、name、type、provider、source、crs，以及矢量的 fields 和 feature_count 或栅格的尺寸与波段数。",
    ),
    "layer_remove": _tool_doc(
        "从当前工程中移除单个图层。",
        "layer_id 必须是当前工程已存在的 layer id，不接受模糊名称。",
        "目标图层必须已经在当前工程注册。",
        "会修改当前工程图层列表和图层面板，但不会删除底层数据文件。",
        "无 confirm_destructive 开关，因此调用前应先用 layer_list 或 layer_get_details 复核目标 id。",
        "返回 removed 字段，值为已移除的 layer id。",
    ),
    "layer_remove_batch": _tool_doc(
        "从当前工程中批量移除多个图层。",
        "layer_ids 是 layer id 数组，空白值会被忽略。",
        "建议先用 layer_list 确认待删除 id；不存在的 id 不会抛错，而是记录到 missing。",
        "会修改当前工程图层列表和图层面板，但不会删除底层数据文件。",
        "无 confirm_destructive 开关，因此应只传入已确认的 layer id。",
        "返回 removed 与 missing 两个数组，便于区分成功移除和未命中的目标。",
    ),
    "layer_resolve_references": _tool_doc(
        "把图层名称或 layer id 解析成唯一 layer id，便于后续安全调用其它工具。",
        "refs 是待解析引用数组，strict 控制遇到 missing 或 ambiguous 时是返回详情还是直接失败。",
        "目标引用应来自当前工程；若名称重复会被归类为 ambiguous。",
        "无写操作，只读取当前工程图层注册表。",
        "strict=false 时会尽量返回 resolved、missing、ambiguous；strict=true 时只要存在缺失或歧义就抛错。",
        "返回 resolved 映射、missing 数组和 ambiguous 映射。",
    ),
    "vector_get_layer_features": _tool_doc(
        "提取矢量图层的样本要素，供模型观察字段值和几何概貌。",
        "layer_ref 指向矢量图层，limit 是希望返回的要素数。",
        "目标图层必须存在且为矢量图层。",
        "无写操作，只顺序读取要素并序列化属性与 geometry_wkt。",
        "limit 会被内部最大阈值裁剪，返回里会明确给出 requested_limit、applied_limit 与 limit_capped。",
        "返回 layer_id、feature_count、fields、features 以及 limit 应用结果。",
    ),
    "vector_table_add_field": _tool_doc(
        "为矢量图层增加一个新字段。",
        "layer_ref 指向矢量图层，field_name 是新字段名，field_type、field_length、field_precision 用于定义字段类型，in_place 控制是否直接改源图层。",
        "目标图层必须存在，且 field_name 不能为空且不能与现有字段重名。",
        "会修改图层字段结构；默认副本模式下先复制图层再修改。",
        _COPY_VECTOR_SAFETY,
        "返回 summary.mode、affected_count、output_layer_id 以及最新字段列表。",
    ),
    "vector_table_drop_fields": _tool_doc(
        "删除矢量图层中的一个或多个字段。",
        "layer_ref 指向矢量图层，fields 是待删除字段名数组，in_place 控制是否直接改源图层。",
        "目标图层必须存在，且至少提供一个字段名。",
        "会修改图层字段结构；不存在的字段不会阻止执行，而是记录到 missing_fields。",
        _COPY_VECTOR_SAFETY,
        "返回 summary.mode、affected_count、output_layer_id、剩余字段列表和 missing_fields。",
    ),
    "vector_table_rename_field": _tool_doc(
        "重命名矢量图层中的单个字段。",
        "layer_ref 指向矢量图层，field_name 是旧字段名，new_field_name 是新字段名，in_place 控制是否直接改源图层。",
        "目标图层必须存在，旧字段必须存在，新字段名不能与已有字段重名。",
        "会修改图层字段结构；默认副本模式下会生成新的输出图层。",
        _COPY_VECTOR_SAFETY,
        "返回 summary.mode、affected_count、output_layer_id 和重命名后的字段列表。",
    ),
    "vector_table_calculate_field": _tool_doc(
        "按 QGIS 表达式批量计算字段值，可在字段不存在时自动创建字段。",
        "layer_ref 指向矢量图层，field_name 是目标字段，expression 是赋值表达式，where 可选筛选条件，field_type 只在自动建字段时生效，in_place 控制是否直接改源图层。",
        "目标图层必须存在，expression 和 where 若提供都必须能被 QGIS 表达式解析并成功求值。",
        "会更新匹配要素的属性值；字段不存在时会先新增该字段。",
        _COPY_VECTOR_SAFETY,
        "返回 summary.mode、affected_count、field_created、output_layer_id 和更新后的字段列表。",
    ),
    "vector_table_query_records": _tool_doc(
        "按条件查询矢量图层记录，适合做数据抽样、分页和字段级检查。",
        "layer_ref 指向矢量图层，where 是可选过滤表达式，fields 控制返回字段，limit 和 offset 控制分页，order_by 指定简单排序字段，include_geometry 控制是否附带 geometry_wkt。",
        "目标图层必须存在，where 表达式和字段名必须有效。",
        "无写操作，只读取并序列化匹配记录。",
        "limit 会被内部阈值裁剪；order_by 仅按属性字符串表示做简单排序，不是完整 SQL 排序。",
        "返回 matched_total、returned、offset、limit 应用结果和 records 数组。",
    ),
    "vector_table_insert_records": _tool_doc(
        "向矢量图层批量插入新记录，可选携带 geometry_wkt。",
        "layer_ref 指向矢量图层，records 是对象数组，键名按字段名匹配，geometry_wkt 若提供会被解析为新要素几何，in_place 控制是否直接改源图层。",
        "目标图层必须存在，records 不能为空，geometry_wkt 若提供必须是有效 WKT。",
        "会新增要素；未知字段不会写入，而是被忽略并记录到 warnings 与 outputs.ignored_fields。",
        _COPY_VECTOR_SAFETY,
        "返回 summary.mode、affected_count、output_layer_id、feature_count，以及 ignored_fields 和 warnings。",
    ),
    "vector_table_update_records": _tool_doc(
        "按条件批量更新矢量图层记录，支持常量赋值和表达式赋值。",
        "layer_ref 指向矢量图层，where 是可选筛选表达式，set_literals 传常量值映射，set_expressions 传表达式映射，in_place 控制是否直接改源图层。",
        "目标图层必须存在，至少提供 set_literals 或 set_expressions 之一，涉及字段和表达式都必须有效。",
        "会更新匹配要素属性值；默认副本模式下生成新图层后再更新。",
        _COPY_VECTOR_SAFETY,
        "返回 summary.mode、affected_count、output_layer_id 和更新后 feature_count。",
    ),
    "vector_table_delete_records": _tool_doc(
        "按条件删除矢量图层中的部分记录。",
        "layer_ref 指向矢量图层，where 可选筛选表达式，in_place 控制是否直接改源图层，confirm_destructive 必须明确确认删除行为。",
        "目标图层必须存在；若提供 where，表达式必须有效。",
        "会删除匹配记录；默认副本模式下删除的是副本图层中的记录。",
        _DESTRUCTIVE_VECTOR_SAFETY,
        "返回 summary.mode、affected_count、output_layer_id 和删除后的 feature_count。",
    ),
    "vector_table_truncate": _tool_doc(
        "清空矢量图层中的全部记录，但保留图层结构和字段。",
        "layer_ref 指向矢量图层，in_place 控制是否直接改源图层，confirm_destructive 必须明确确认清空行为。",
        "目标图层必须存在。",
        "会删除全部要素；默认副本模式下清空的是副本图层。",
        _DESTRUCTIVE_VECTOR_SAFETY,
        "返回 summary.mode、affected_count、output_layer_id 和清空后的 feature_count。",
    ),
    "vector_stats_basic": _tool_doc(
        "计算矢量字段的基础统计量，适合先判断数值分布。",
        "layer_ref 指向矢量图层，field_name 是目标字段。",
        "目标图层必须存在且字段存在，字段最好是可统计的数值型字段。",
        "无写操作，只读取字段值并做统计。",
        "无。",
        "返回 count、sum、mean、min、max、stdev 等基础统计摘要。",
    ),
    "vector_stats_by_categories": _tool_doc(
        "按一个或多个分类字段汇总记录数或数值字段统计，适合做分组分析。",
        "layer_ref 指向矢量图层，category_fields 是分类字段数组，values_field 可选；为空时通常只做分组计数。",
        "目标图层必须存在，分类字段必须存在；若提供 values_field，该字段也必须存在。",
        "无写操作，只读取要素属性并做分组汇总。",
        "无。",
        "返回按分类组合分组后的统计结果，便于模型理解类别分布。",
    ),
    "raster_stats_basic": _tool_doc(
        "读取栅格单个波段的基础统计量。",
        "layer_ref 指向栅格图层，band 指定波段号，默认 1。",
        "目标图层必须存在且为栅格，指定波段必须有效。",
        "无写操作，只读取栅格波段统计信息。",
        "无。",
        "返回目标波段的最小值、最大值、均值、标准差等基础统计摘要。",
    ),
    "raster_stats_zonal": _tool_doc(
        "把栅格统计结果按分区写入矢量图层属性表，常用于区域统计。",
        "vector_layer_ref 指向分区矢量图层，raster_layer_ref 指向栅格图层，raster_band 指定波段，column_prefix 控制输出字段前缀，in_place 控制是否直接写回原矢量图层。",
        "矢量与栅格图层都必须存在，且空间关系应满足分区统计需求。",
        "会运行原生 zonal statistics 算法，并在矢量属性表增加统计字段；默认副本模式下生成新的输出图层。",
        _COPY_VECTOR_SAFETY,
        "返回 summary.mode、algorithm、output_layer_id 和底层 Processing 结果对象。",
    ),
    "raster_stats_cell": _tool_doc(
        "对多个栅格做逐像元统计运算，例如逐像元均值、最小值或最大值。",
        "raster_layer_refs 是栅格引用数组，statistic 选择逐像元统计类型，ignore_nodata 控制是否忽略 NoData。",
        "至少提供一个有效栅格图层，且各栅格最好在分辨率、范围和 CRS 上可兼容处理。",
        "会运行 Processing 栅格统计算法，通常产生新的输出栅格结果；是否自动加载取决于算法默认行为和 Processing 设置。",
        "无 destructive 开关，但会触发 Processing 执行。",
        "返回算法摘要和序列化后的 Processing 输出结果。",
    ),
    "dataset_list_files": _tool_doc(
        "扫描目录并识别可加载的数据集文件，区分 vector 与 raster。",
        "directory 是根目录，recursive 控制是否递归，dataset_kind 控制筛选 vector、raster 或 both，geometry_type 与 name_glob 用于进一步过滤，limit 控制返回上限。",
        "目录必须存在且可访问。",
        "无写操作，只遍历文件系统并按扩展名和几何类型推断数据集。",
        "limit 会被内部阈值裁剪，geometry_type 过滤仅对识别出的矢量数据集生效。",
        "返回 datasets 数组及 requested_limit、applied_limit、limit_capped 等扫描摘要。",
    ),
    "dataset_load_from_directory": _tool_doc(
        "先扫描目录中的候选数据集，再把匹配的 vector 或 raster 图层批量加载到当前工程。",
        "参数和 dataset_list_files 基本一致，skip_invalid 控制遇到坏数据时是跳过还是整体失败。",
        "目录必须存在，且目录下至少有可识别的数据集文件才有意义。",
        "会向当前工程新增多个图层，但不会改写源数据文件。",
        "skip_invalid=true 时失败项会进入 failed；skip_invalid=false 时第一个失败就终止。",
        "返回 requested_count、loaded_count、failed_count，以及 loaded 和 failed 明细。",
    ),
    "dataset_inspect_shapefile_bundle": _tool_doc(
        "扫描目录或单个 `.shp` 文件，检查 shapefile bundle 及其 sidecar 文件是否完整，并返回结构化风险摘要。",
        "path 可以是目录或单个 `.shp` 文件，recursive 控制目录递归扫描，name_glob 过滤候选文件，limit 控制返回上限，task_name 可把结果写入最近一次运行摘要。",
        "path 必须存在；若传文件则必须是 `.shp`。",
        "无写操作，只读取 shapefile bundle 成员信息和矢量元数据。",
        "limit 会被内部阈值裁剪；task_name 非空时会更新 qgis://workflow/shapefile/run-summary。",
        "返回 bundles 数组，元素包含 `.shp/.shx/.dbf/.prj/.cpg` 完整性、大小、CRS、几何类型、字段名、命名风险和 warnings。",
    ),
    "vector_check_validity_report": _tool_doc(
        "对单个矢量图层或矢量文件做 shapefile 导向的结构化体检，统一输出几何、记录、字段和 CRS 风险。",
        "layer_ref 与 path 二选一；required_fields 用于声明必需字段，expected_crs 用于声明目标 CRS，task_name 可把结果写入运行摘要。",
        "目标图层必须存在，或 path 必须指向有效矢量数据文件。",
        "无写操作，只读取图层要素、字段和 CRS 元数据。",
        "task_name 非空时会更新 qgis://workflow/shapefile/run-summary；若传 expected_crs，结果会包含 CRS mismatch 判定。",
        "返回 report 对象，覆盖 null/empty geometry、invalid geometry、duplicate geometry、duplicate record、multipart、字段长度和 safe_for_export 判断。",
    ),
    "vector_prepare_work_layer": _tool_doc(
        "把输入矢量图层整理成带任务标签的临时工作层，并串行执行 shapefile 稳定模板的标准化前置动作。",
        "layer_ref 与 path 二选一；task_name 用于标记工作层和运行摘要；target_crs 控制目标坐标系；normalize_field_names 控制是否把字段名压到 10 字符以内；multipart_policy 可选 keep 或 singleparts。",
        "目标图层必须存在或 path 指向有效矢量数据文件；target_crs 若提供必须可被 QGIS 解析。",
        "会在当前工程新增一个临时工作层，并对该临时层执行空几何清理、几何修复、重复几何清理、重投影和可选字段改名/拆多部件操作。",
        "默认不写盘；所有修改都落在临时工作层上，原始输入图层和源文件不会被直接改写。",
        "返回 output_layer_id、field_name_mapping、initial_report、final_report，以及各标准化步骤的影响计数。",
    ),
    "vector_export_shapefile": _tool_doc(
        "把当前工作层或任意矢量图层安全导出为最终 shapefile，并在导出前强制复核几何与字段约束。",
        "layer_ref 指向待导出的矢量图层，output_directory 是输出目录，file_name 可覆盖输出文件名，task_name 用于运行摘要，overwrite 控制是否允许覆盖，auto_truncate_field_names 控制是否自动裁剪超长字段名，confirm_write 与 confirm_destructive 用于显式确认写盘和覆盖。",
        "目标图层必须存在；output_directory 必须可创建；当 overwrite=true 时必须同时传 confirm_destructive=true。",
        "会在磁盘写出 shapefile bundle；若需要自动裁剪字段名，内部会生成临时导出副本并在导出后自动移除。",
        "必须显式设置 confirm_write=true；覆盖现有输出时还必须 confirm_destructive=true；存在无效几何或空几何时会阻断导出。",
        "返回 output_path、output_members、field_name_mapping、preflight_report 和 final_report，便于在导出后继续走质量 gate。",
    ),
    "project_cleanup_work_layers": _tool_doc(
        "按 task_name 或 layer_ids 清理 shapefile 工作流创建的临时图层，并可选删除显式登记的临时文件。",
        "task_name 用于按任务标签批量匹配工作层，layer_ids 可精确指定图层，temp_paths 可补充显式临时路径，delete_temp_files 控制是否删除这些路径，confirm_write 与 confirm_destructive 仅在删文件时生效。",
        "至少提供可匹配的 task_name、layer_ids 或 temp_paths 中的一类才有意义；若 delete_temp_files=true 且存在路径，则必须 confirm_write=true 且 confirm_destructive=true。",
        "会从当前工程移除匹配的临时图层；可选地删除临时文件或临时目录。",
        "删除磁盘临时文件时需要显式双确认；仅移除工程图层时不触碰源数据文件。",
        "返回 removed_layer_ids、deleted_temp_paths 和 missing_temp_paths，便于回收检查与日志留存。",
    ),
    "filesystem_query_list_entries": _tool_doc(
        "列出目录中的文件或子目录条目，适合在执行文件操作前先做只读探查。",
        "directory 是根目录，recursive 控制是否递归，include_files 和 include_directories 控制返回对象类型，name_glob 过滤名称，limit 控制返回上限。",
        "目录必须存在；include_files 与 include_directories 不能同时为 false。",
        "无写操作，只读取文件系统元数据。",
        "limit 会被内部阈值裁剪；结果里会明确 returned_count、matched_total 与 truncated。",
        "返回目录路径、entries 数组以及 limit 应用摘要。",
    ),
    "filesystem_query_entry_info": _tool_doc(
        "读取单个文件或目录的基础元数据。",
        "path 指向文件或目录。",
        "目标路径必须存在。",
        "无写操作，只读取文件系统元数据。",
        "无。",
        "返回 entry 对象，包含类型、大小、时间戳和可用路径信息。",
    ),
    "filesystem_query_read_text": _tool_doc(
        "按 UTF-8 读取文本文件内容，适合让模型读取配置、脚本或日志片段。",
        "path 指向文本文件，max_chars 可选，用于限制返回字符数。",
        "目标路径必须存在且是文件，并且内容应能按 UTF-8 解码。",
        "无写操作，只读取文件内容。",
        "max_chars 为 None 时返回全文；传入数值时只读取 max_chars+1 个字符用于判断截断，并在 summary.truncated 中标记。",
        "返回 text 字段和截断摘要。",
    ),
    "filesystem_edit_write_text": _tool_doc(
        "以 UTF-8 一次性写入文本文件，适合新建配置或覆盖写文件。",
        "path 是目标文件路径，content 是完整文本内容，overwrite 控制是否允许覆盖已存在文件，confirm_destructive 用于确认覆盖，create_parents 控制是否自动创建父目录，confirm_write 用于显式确认写操作。",
        "若目标已存在且 overwrite=false 会直接失败；父目录不存在时只有 create_parents=true 才会自动创建。",
        "会创建或覆盖磁盘文件。",
        _FILESYSTEM_WRITE_SAFETY,
        "返回写入字符数和最终 path 摘要。",
    ),
    "filesystem_edit_append_text": _tool_doc(
        "向文本文件尾部追加 UTF-8 内容。",
        "path 是目标文件路径，content 是待追加文本，create_parents 控制父目录不存在时是否自动创建，confirm_write 用于显式确认写操作。",
        "目标路径的父目录必须存在或允许自动创建；目标文件不存在时会被创建。",
        "会在磁盘上创建文件或修改现有文件末尾内容。",
        "该工具不会覆盖已有内容，但仍属于写盘操作，调用前应确认目标路径。",
        "返回追加字符数和最终 path 摘要。",
    ),
    "filesystem_edit_copy_entry": _tool_doc(
        "复制单个文件或整个目录到新位置。",
        "source_path 是源路径，target_path 是目标路径，overwrite 控制是否允许覆盖目标，confirm_destructive 用于确认覆盖已存在目标，confirm_write 用于显式确认写操作。",
        "源路径必须存在；目标若已存在且 overwrite=false 会失败。",
        "会在磁盘上创建新的文件或目录副本；目录复制会递归复制内容。",
        _FILESYSTEM_WRITE_SAFETY,
        "返回 source_path 和 target_path 摘要。",
    ),
    "filesystem_edit_move_entry": _tool_doc(
        "把文件或目录移动到新位置。",
        "source_path 是源路径，target_path 是目标路径，overwrite 控制是否允许覆盖目标，confirm_destructive 用于确认覆盖已存在目标，confirm_write 用于显式确认写操作。",
        "源路径必须存在；目标若已存在且 overwrite=false 会失败。",
        "会修改磁盘目录结构，源路径在成功后会消失。",
        _FILESYSTEM_WRITE_SAFETY,
        "返回 source_path 和 target_path 摘要，表示移动已完成。",
    ),
    "filesystem_edit_delete_entry": _tool_doc(
        "删除单个文件或整个目录树。",
        "path 指向待删除文件或目录，confirm_destructive 必须明确确认删除，confirm_write 用于显式确认写操作。",
        "目标路径必须存在。",
        "会永久删除磁盘上的文件或目录内容。",
        "只有 confirm_write=true 且 confirm_destructive=true 才允许执行删除。",
        "返回 deleted_path 摘要。",
    ),
    "filesystem_stats_directory": _tool_doc(
        "统计目录中的文件数、目录数和累计大小，适合在批处理前估算工作量。",
        "directory 是根目录，recursive 控制是否递归统计。",
        "目录必须存在。",
        "无写操作，只遍历文件系统做统计。",
        "无。",
        "返回文件数、目录数、总字节数等目录统计摘要。",
    ),
    "processing_list_providers": _tool_doc(
        "列出当前 QGIS Processing 注册表中的 provider 清单。",
        "无业务输入。",
        "Processing 运行时必须可用。",
        "无写操作，只读取 Processing 注册表。",
        "无。",
        "返回 provider 的 id、name、active、algorithm_count，以及 count_scope、total_algorithm_count、active_provider_count、active_algorithm_count 和总数。",
    ),
    "processing_get_algorithms": _tool_doc(
        "查询 Processing 算法目录，支持按 provider 过滤，或按 algorithm_id 精确读取单个算法。",
        "algorithm_id 提供时返回单个算法完整定义；provider_id 提供时按 provider 过滤；include_parameters 和 include_outputs 控制列表模式下是否展开参数和输出；limit 控制列表返回上限。",
        "Processing 运行时必须可用；若指定 algorithm_id，则该算法必须存在。",
        "无写操作，只读取 Processing 算法元数据。",
        "列表模式的 limit 会被内部阈值裁剪；单算法模式会始终返回该算法的完整参数和输出定义。",
        "返回单个 algorithm 对象或 algorithms 数组，以及 count、returned、truncated 等摘要。",
    ),
    "processing_get_parameter_template": _tool_doc(
        "把某个 Processing 算法的输入参数和输出定义整理成适合模型填写的模板。",
        "algorithm_id 是目标算法 id。",
        "Processing 运行时必须可用，且算法必须存在。",
        "无写操作，只读取算法参数定义和输出定义。",
        "无。",
        "返回 algorithm 基本信息、required_parameters、optional_parameters 和 outputs。",
    ),
    "processing_execute_algorithm": _tool_doc(
        "执行单次 Processing 算法调用，是通用算法执行入口。",
        "algorithm 是算法 id，parameters 是参数对象，load_results 控制是否把结果加载到当前工程，allow_disk_write 控制是否允许磁盘输出路径通过安全检查，allow_in_place_edit 控制是否允许原位编辑参数通过安全检查。",
        "Processing 运行时必须可用，algorithm 必须存在，parameters 必须是对象。",
        "会触发 Processing 执行，可能生成临时输出、加载新图层，或在显式允许时写盘或原位修改。",
        _PROCESSING_SAFETY,
        "返回 algorithm、load_results、result、warnings、safety_policy 和 effective_parameters。",
    ),
    "processing_execute_on_layers": _tool_doc(
        "把一组图层引用先解析成真实 layer id，再执行单次或批量 Processing 算法调用，适合模型按图层绑定自动批处理。",
        "algorithm 是算法 id，layer_bindings 把参数名映射到图层引用或引用数组，parameters 是其余参数对象，load_results 控制是否加载结果，batch_mode 控制是否按绑定数组逐批运行，allow_disk_write 与 allow_in_place_edit 控制安全检查。",
        "Processing 运行时必须可用，parameters 必须是对象，layer_bindings 必须能解析到有效图层；batch_mode=true 时每个绑定在对应索引都要有值。",
        "会触发一次或多次 Processing 执行，并在每轮执行前把图层引用替换为真实 layer id。",
        _PROCESSING_SAFETY + " batch_mode=false 时任一失败会直接抛错；batch_mode=true 时失败会记录到 runs 中继续处理后续批次。",
        "返回 ok、run_count、success_count、failure_count、runs、warnings、safety_policy 和最后一次 effective_parameters。",
    ),
}

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

def register_tools(mcp, tools: ProcessingMCPTools, enable_execute_code: bool = True) -> None:
    """注册 tools 能力。"""
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
            """执行 wrapper 相关逻辑。"""
            return getattr(tools, _tool_name)(*args, **kwargs)

        _wrapper.__name__ = tool_name
        _wrapper.__doc__ = f"MCP tool wrapper for {tool_name}."
        tool_factory()(_wrapper)

_validate_tool_entry_contracts()
_apply_registered_tool_docstrings()
