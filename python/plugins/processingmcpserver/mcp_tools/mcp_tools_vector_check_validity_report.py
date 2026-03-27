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

TOOL_NAME = 'vector_check_validity_report'
TOOL_DOC = '???对单个矢量图层或矢量文件做 shapefile 导向的结构化体检，统一输出几何、记录、字段和 CRS 风险。 ?????layer_ref 与 path 二选一；required_fields 用于声明必需字段，expected_crs 用于声明目标 CRS，task_name 可把结果写入运行摘要。 ?????目标图层必须存在，或 path 必须指向有效矢量数据文件。 ??????无写操作，只读取图层要素、字段和 CRS 元数据。 ?????task_name 非空时会更新 qgis://workflow/shapefile/run-summary；若传 expected_crs，结果会包含 CRS mismatch 判定。 ?????返回 report 对象，覆盖 null/empty geometry、invalid geometry、duplicate geometry、duplicate record、multipart、字段长度和 safe_for_export 判断。'

def vector_check_validity_report(self, layer_ref: str = "", path: str = "", required_fields: list[str] | None = None, expected_crs: str | None = None, task_name: str = "") -> dict[str, Any]:
    """执行矢量相关的 check validity report 逻辑。"""
    return self._run(self._vector_check_validity_report_impl, layer_ref, path, required_fields, expected_crs, task_name)

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

@staticmethod
def _normalize_task_name(task_name: str | None, fallback: str = "") -> str:
    """归一化 task_name。"""
    value = (task_name or "").strip()
    if value:
        return value
    return fallback.strip() or "shapefile-task"

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """执行 ok result 相关逻辑。"""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

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
    """返回 UTC ISO 时间。"""
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")

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

@staticmethod
def _field_index(layer: QgsVectorLayer, field_name: str) -> int:
    """执行 field index 相关逻辑。"""
    return layer.fields().indexFromName(field_name)

def _long_field_names(self, layer: QgsVectorLayer) -> list[str]:
    """返回超出 shapefile 约束的字段名。"""
    return [field.name() for field in layer.fields() if len(field.name()) > 10]

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

def _resolve_filesystem_query_path(self, path: str | Path) -> Path:
    """解析 filesystem query path。"""
    return self._normalize_filesystem_path(path)

def _resolve_vector_layer_ref(self, layer_ref: Any) -> QgsVectorLayer:
    """解析 vector layer ref。"""
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.VectorLayer:
        raise Exception(f"Layer is not a vector layer: {layer_ref}")
    return layer

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

@staticmethod
def _normalize_filesystem_path(path: str | Path) -> Path:
    """归一化 filesystem path。"""
    candidate = path if isinstance(path, Path) else Path(str(path).strip()).expanduser()
    if not candidate.is_absolute():
        candidate = Path.cwd() / candidate
    return candidate.resolve(strict=False)

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

TOOL_METHODS: dict[str, object] = {
    'vector_check_validity_report': vector_check_validity_report,
    '_vector_check_validity_report_impl': _vector_check_validity_report_impl,
    '_append_shapefile_run_step': _append_shapefile_run_step,
    '_ensure_shapefile_run_summary': _ensure_shapefile_run_summary,
    '_inspect_vector_layer_validity': _inspect_vector_layer_validity,
    '_normalize_task_name': _normalize_task_name,
    '_ok_result': _ok_result,
    '_resolve_vector_source': _resolve_vector_source,
    '_serialize_value': _serialize_value,
    '_utc_now_iso': _utc_now_iso,
    '_reset_shapefile_run_summary': _reset_shapefile_run_summary,
    '_collect_null_attribute_fields': _collect_null_attribute_fields,
    '_display_geometry_type': _display_geometry_type,
    '_field_index': _field_index,
    '_long_field_names': _long_field_names,
    '_parse_target_crs': _parse_target_crs,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_empty_shapefile_run_summary': _empty_shapefile_run_summary,
    '_normalize_filesystem_path': _normalize_filesystem_path,
    '_resolve_layer_ref': _resolve_layer_ref,
}
