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

TOOL_NAME = 'vector_prepare_work_layer'
TOOL_DOC = '把输入矢量图层整理成带任务标签的临时工作层，并串行执行 shapefile 稳定模板的标准化前置动作。 layer_ref 与 path 二选一；task_name 用于标记工作层和运行摘要；target_crs 控制目标坐标系；normalize_field_names 控制是否把字段名压到 10 字符以内；multipart_policy 可选 keep 或 singleparts。 目标图层必须存在或 path 指向有效矢量数据文件；target_crs 若提供必须可被 QGIS 解析。 会在当前工程新增一个临时工作层，并对该临时层执行空几何清理、几何修复、重复几何清理、重投影和可选字段改名/拆多部件操作。 默认不写盘；所有修改都落在临时工作层上，原始输入图层和源文件不会被直接改写。 返回 output_layer_id、field_name_mapping、initial_report、final_report，以及各标准化步骤的影响计数。'

def vector_prepare_work_layer(self, layer_ref: str = "", path: str = "", task_name: str = "", target_crs: str | None = None, normalize_field_names: bool = False, multipart_policy: str = "keep") -> dict[str, Any]:
    """Handle a vector work layer."""
    return self._run(self._vector_prepare_work_layer_impl, layer_ref, path, task_name, target_crs, normalize_field_names, multipart_policy)

def _vector_prepare_work_layer_impl(
    self,
    layer_ref: str,
    path: str,
    task_name: str,
    target_crs: str | None,
    normalize_field_names: bool,
    multipart_policy: str,
) -> dict[str, Any]:
    """Implement the vector work layer logic."""
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

@staticmethod
def _copy_layer_name(layer: QgsMapLayer, suffix: str = "copy") -> str:
    """Handle copy layer name."""
    base_name = (layer.name() or "layer").strip() or "layer"
    clean_suffix = suffix.strip().replace(" ", "_") if suffix else "copy"
    return f"{base_name}_{clean_suffix}"

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

def _explode_multipart_layer(
    self, layer: QgsVectorLayer, task_name: str
) -> tuple[QgsVectorLayer, int]:
    """Handle explode multipart layer."""
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

def _inspect_vector_layer_validity(
    self,
    layer: QgsVectorLayer,
    required_fields: list[str] | None = None,
    expected_crs: str | None = None,
) -> dict[str, Any]:
    """Inspect vector layer validity."""
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

def _make_layer_geometries_valid(self, layer: QgsVectorLayer) -> int:
    """Handle make layer geometries valid."""
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

def _materialize_vector_layer(
    self, layer: QgsVectorLayer, suffix: str = "copy"
) -> QgsVectorLayer:
    """Handle materialize vector layer."""
    cloned_layer = layer.materialize(QgsFeatureRequest())
    if cloned_layer is None or not cloned_layer.isValid():
        raise Exception(f"Failed to materialize vector layer copy: {layer.id()}")
    cloned_layer.setName(self._copy_layer_name(layer, suffix))
    QgsProject.instance().addMapLayer(cloned_layer)
    return cloned_layer

@staticmethod
def _normalize_output_stem(text: str) -> str:
    """Handle normalize output stem."""
    value = re.sub(r"[^0-9A-Za-z_]+", "_", text or "")
    value = value.strip("_")
    return value or "output"

@staticmethod
def _normalize_task_name(task_name: str | None, fallback: str = "") -> str:
    """Handle normalize task name."""
    value = (task_name or "").strip()
    if value:
        return value
    return fallback.strip() or "shapefile-task"

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """Handle ok result."""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _parse_target_crs(
    self, target_crs: str | None
) -> QgsCoordinateReferenceSystem | None:
    """Handle parse target crs."""
    value = (target_crs or "").strip()
    if not value:
        return None
    crs = QgsCoordinateReferenceSystem()
    if not crs.createFromUserInput(value) or not crs.isValid():
        raise Exception(f"Invalid target_crs: {target_crs}")
    return crs

def _record_run_output(self, output_key: str, value: str) -> None:
    """Handle record run output."""
    outputs = self._shapefile_run_summary.setdefault("outputs", {})
    bucket = outputs.setdefault(output_key, [])
    if value not in bucket:
        bucket.append(value)

def _remove_duplicate_geometries(self, layer: QgsVectorLayer) -> int:
    """Remove duplicate geometries."""
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

def _remove_empty_geometries(self, layer: QgsVectorLayer) -> int:
    """Remove empty geometries."""
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

def _rename_layer_fields_for_shapefile(
    self, layer: QgsVectorLayer
) -> dict[str, str]:
    """Handle rename layer fields for shapefile."""
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

def _reproject_layer_in_place(
    self,
    layer: QgsVectorLayer,
    target_crs: QgsCoordinateReferenceSystem,
) -> int:
    """Handle reproject layer in place."""
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

def _resolve_vector_source(
    self,
    layer_ref: str = "",
    path: str = "",
) -> tuple[QgsVectorLayer, str]:
    """Resolve vector source."""
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

def _tag_work_layer(
    self,
    layer: QgsVectorLayer,
    task_name: str,
    source_path: str = "",
    temp_paths: list[str] | None = None,
) -> None:
    """Handle tag work layer."""
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

def _coerce_vector_output_layer(
    self,
    output_value: Any,
    layer_name: str,
) -> QgsVectorLayer:
    """Handle coerce vector output layer."""
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

def _ensure_processing_runtime(self) -> None:
    """Handle ensure processing runtime."""
    _ensure_processing_initialized()

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
def _layer_custom_property(layer: QgsMapLayer, key: str, default: Any = None) -> Any:
    """Handle layer custom property."""
    if hasattr(layer, "customProperty"):
        try:
            return layer.customProperty(key, default)
        except Exception:
            return default
    return default

def _collect_null_attribute_fields(self, layer: QgsVectorLayer) -> list[str]:
    """Handle collect null attribute fields."""
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
    """Handle display geometry type."""
    return {
        Qgis.GeometryType.Point: "point",
        Qgis.GeometryType.Line: "line",
        Qgis.GeometryType.Polygon: "polygon",
        Qgis.GeometryType.Null: "null",
        Qgis.GeometryType.Unknown: "unknown",
    }.get(layer.geometryType(), "unknown")

@staticmethod
def _field_index(layer: QgsVectorLayer, field_name: str) -> int:
    """Handle field index."""
    return layer.fields().indexFromName(field_name)

def _long_field_names(self, layer: QgsVectorLayer) -> list[str]:
    """Handle long field names."""
    return [field.name() for field in layer.fields() if len(field.name()) > 10]

def _apply_vector_edit(self, layer: QgsVectorLayer, operation) -> Any:
    """Handle apply vector edit."""
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
def _normalize_shapefile_field_name(
    field_name: str,
    used_names: set[str],
) -> str:
    """Handle normalize shapefile field name."""
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

def _resolve_filesystem_query_path(self, path: str | Path) -> Path:
    """Resolve filesystem query path."""
    return self._normalize_filesystem_path(path)

def _resolve_vector_layer_ref(self, layer_ref: Any) -> QgsVectorLayer:
    """Resolve vector layer ref."""
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.VectorLayer:
        raise Exception(f"Layer is not a vector layer: {layer_ref}")
    return layer

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
def _begin_vector_edit(layer: QgsVectorLayer) -> bool:
    """Handle begin vector edit."""
    if layer.isEditable():
        return False
    if not layer.startEditing():
        raise Exception("Failed to start editing vector layer")
    return True

@staticmethod
def _finish_vector_edit(layer: QgsVectorLayer, started_here: bool) -> None:
    """Handle finish vector edit."""
    if not started_here:
        return
    if layer.commitChanges():
        return
    errors = "; ".join(layer.commitErrors()) if hasattr(layer, "commitErrors") else ""
    layer.rollBack()
    raise Exception(f"Failed to commit vector layer changes: {errors}")

@staticmethod
def _normalize_filesystem_path(path: str | Path) -> Path:
    """Handle normalize filesystem path."""
    candidate = path if isinstance(path, Path) else Path(str(path).strip()).expanduser()
    if not candidate.is_absolute():
        candidate = Path.cwd() / candidate
    return candidate.resolve(strict=False)

def _resolve_layer_ref(self, layer_ref: Any) -> QgsMapLayer:
    """Resolve layer ref."""
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
    'vector_prepare_work_layer': vector_prepare_work_layer,
    '_vector_prepare_work_layer_impl': _vector_prepare_work_layer_impl,
    '_append_shapefile_run_step': _append_shapefile_run_step,
    '_copy_layer_name': _copy_layer_name,
    '_ensure_shapefile_run_summary': _ensure_shapefile_run_summary,
    '_explode_multipart_layer': _explode_multipart_layer,
    '_inspect_vector_layer_validity': _inspect_vector_layer_validity,
    '_make_layer_geometries_valid': _make_layer_geometries_valid,
    '_materialize_vector_layer': _materialize_vector_layer,
    '_normalize_output_stem': _normalize_output_stem,
    '_normalize_task_name': _normalize_task_name,
    '_ok_result': _ok_result,
    '_parse_target_crs': _parse_target_crs,
    '_record_run_output': _record_run_output,
    '_remove_duplicate_geometries': _remove_duplicate_geometries,
    '_remove_empty_geometries': _remove_empty_geometries,
    '_rename_layer_fields_for_shapefile': _rename_layer_fields_for_shapefile,
    '_reproject_layer_in_place': _reproject_layer_in_place,
    '_resolve_vector_source': _resolve_vector_source,
    '_tag_work_layer': _tag_work_layer,
    '_serialize_value': _serialize_value,
    '_utc_now_iso': _utc_now_iso,
    '_reset_shapefile_run_summary': _reset_shapefile_run_summary,
    '_coerce_vector_output_layer': _coerce_vector_output_layer,
    '_ensure_processing_runtime': _ensure_processing_runtime,
    '_get_layer_temp_paths': _get_layer_temp_paths,
    '_layer_custom_property': _layer_custom_property,
    '_collect_null_attribute_fields': _collect_null_attribute_fields,
    '_display_geometry_type': _display_geometry_type,
    '_field_index': _field_index,
    '_long_field_names': _long_field_names,
    '_apply_vector_edit': _apply_vector_edit,
    '_normalize_shapefile_field_name': _normalize_shapefile_field_name,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_empty_shapefile_run_summary': _empty_shapefile_run_summary,
    '_begin_vector_edit': _begin_vector_edit,
    '_finish_vector_edit': _finish_vector_edit,
    '_normalize_filesystem_path': _normalize_filesystem_path,
    '_resolve_layer_ref': _resolve_layer_ref,
}
