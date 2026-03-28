"""Processing MCP tool module."""
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

DEFAULT_FEATURE_LIMIT = 10
MAX_FEATURE_LIMIT = 100
DEFAULT_DATASET_LIMIT = 50
MAX_DATASET_LIMIT = 100
DEFAULT_FILESYSTEM_LIST_LIMIT = 100
MAX_FILESYSTEM_LIST_LIMIT = 200
DEFAULT_ALGORITHM_LIST_LIMIT = 30
MAX_ALGORITHM_LIST_LIMIT = 60

_PROCESSING_INITIALIZED = False

def _ensure_processing_initialized() -> None:
    global _PROCESSING_INITIALIZED
    if _PROCESSING_INITIALIZED:
        return
    Processing.initialize()
    _PROCESSING_INITIALIZED = True

TOOL_NAME = 'project_cleanup_work_layers'
TOOL_DOC = '???按 task_name 或 layer_ids 清理 shapefile 工作流创建的临时图层，并可选删除显式登记的临时文件。 ?????task_name 用于按任务标签批量匹配工作层，layer_ids 可精确指定图层，temp_paths 可补充显式临时路径，delete_temp_files 控制是否删除这些路径，confirm_write 与 confirm_destructive 仅在删文件时生效。 ?????至少提供可匹配的 task_name、layer_ids 或 temp_paths 中的一类才有意义；若 delete_temp_files=true 且存在路径，则必须 confirm_write=true 且 confirm_destructive=true。 ??????会从当前工程移除匹配的临时图层；可选地删除临时文件或临时目录。 ?????删除磁盘临时文件时需要显式双确认；仅移除工程图层时不触碰源数据文件。 ?????返回 removed_layer_ids、deleted_temp_paths 和 missing_temp_paths，便于回收检查与日志留存。'

def project_cleanup_work_layers(self, task_name: str = "", layer_ids: list[str] | None = None, temp_paths: list[str] | None = None, delete_temp_files: bool = True, confirm_write: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
    """Handle work layers from the project."""
    return self._run(self._project_cleanup_work_layers_impl, task_name, layer_ids, temp_paths, delete_temp_files, confirm_write, confirm_destructive)

def _project_cleanup_work_layers_impl(
    self,
    task_name: str,
    layer_ids: list[str] | None,
    temp_paths: list[str] | None,
    delete_temp_files: bool,
    confirm_write: bool,
    confirm_destructive: bool,
) -> dict[str, Any]:
    """Implement the work layers from the project logic."""
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

def _append_shapefile_run_step(
    self,
    step: str,
    summary: dict[str, Any] | None = None,
    outputs: dict[str, Any] | None = None,
    warnings: list[str] | None = None,
    status: str | None = None,
) -> None:
    """Handle append shapefile run step."""
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

def _collect_cleanup_target_layers(
    self,
    task_name: str,
    layer_ids: list[str] | None,
) -> list[QgsMapLayer]:
    """Handle collect cleanup target layers."""
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

@staticmethod
def _ensure_filesystem_write_confirmed(confirm_write: bool) -> None:
    """Handle ensure filesystem write confirmed."""
    if not confirm_write:
        raise Exception(
            "confirm_write must be true for filesystem_edit_* operations"
        )

def _ensure_shapefile_run_summary(
    self,
    task_name: str = "",
    status: str | None = None,
    inputs: dict[str, Any] | None = None,
) -> str:
    """Handle ensure shapefile run summary."""
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

def _get_layer_temp_paths(self, layer: QgsMapLayer) -> list[str]:
    """Return the layer temp paths."""
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

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """Handle ok result."""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _resolve_filesystem_write_path(self, path: str | Path) -> Path:
    """Resolve filesystem write path."""
    return self._resolve_filesystem_query_path(path)

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
        return [_serialize_value(item) for item in value]
    if isinstance(value, dict):
        return {str(k): _serialize_value(v) for k, v in value.items()}
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

@staticmethod
def _layer_custom_property(layer: QgsMapLayer, key: str, default: Any = None) -> Any:
    """Handle layer custom property."""
    if hasattr(layer, "customProperty"):
        try:
            return layer.customProperty(key, default)
        except Exception:
            return default
    return default

def _reset_shapefile_run_summary(
    self,
    task_name: str,
    status: str,
    inputs: dict[str, Any] | None = None,
) -> str:
    """Handle reset shapefile run summary."""
    normalized = (task_name or "").strip()
    self._shapefile_run_summary = self._empty_shapefile_run_summary()
    self._shapefile_run_summary["task_name"] = normalized
    self._shapefile_run_summary["status"] = status
    if inputs:
        self._shapefile_run_summary["inputs"] = self._serialize_value(inputs)
    return normalized

def _resolve_filesystem_query_path(self, path: str | Path) -> Path:
    """Resolve filesystem query path."""
    return self._normalize_filesystem_path(path)

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

@staticmethod
def _normalize_filesystem_path(path: str | Path) -> Path:
    """Handle normalize filesystem path."""
    candidate = path if isinstance(path, Path) else Path(str(path).strip()).expanduser()
    if not candidate.is_absolute():
        candidate = Path.cwd() / candidate
    return candidate.resolve(strict=False)

TOOL_METHODS: dict[str, object] = {
    'project_cleanup_work_layers': project_cleanup_work_layers,
    '_project_cleanup_work_layers_impl': _project_cleanup_work_layers_impl,
    '_append_shapefile_run_step': _append_shapefile_run_step,
    '_collect_cleanup_target_layers': _collect_cleanup_target_layers,
    '_ensure_filesystem_write_confirmed': _ensure_filesystem_write_confirmed,
    '_ensure_shapefile_run_summary': _ensure_shapefile_run_summary,
    '_get_layer_temp_paths': _get_layer_temp_paths,
    '_ok_result': _ok_result,
    '_resolve_filesystem_write_path': _resolve_filesystem_write_path,
    '_serialize_value': _serialize_value,
    '_utc_now_iso': _utc_now_iso,
    '_layer_custom_property': _layer_custom_property,
    '_reset_shapefile_run_summary': _reset_shapefile_run_summary,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_empty_shapefile_run_summary': _empty_shapefile_run_summary,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
