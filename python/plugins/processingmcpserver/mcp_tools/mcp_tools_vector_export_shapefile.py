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

TOOL_NAME = 'vector_export_shapefile'
TOOL_DOC = '把当前工作层或任意矢量图层安全导出为最终 shapefile，并在导出前强制复核几何与字段约束。 layer_ref 指向待导出的矢量图层，output_directory 是输出目录，file_name 可覆盖输出文件名，task_name 用于运行摘要，overwrite 控制是否允许覆盖，auto_truncate_field_names 控制是否自动裁剪超长字段名，confirm_write 与 confirm_destructive 用于显式确认写盘和覆盖。 目标图层必须存在；output_directory 必须可创建；当 overwrite=true 时必须同时传 confirm_destructive=true。 会在磁盘写出 shapefile bundle；若需要自动裁剪字段名，内部会生成临时导出副本并在导出后自动移除。 必须显式设置 confirm_write=true；覆盖现有输出时还必须 confirm_destructive=true；存在无效几何或空几何时会阻断导出。 返回 output_path、output_members、field_name_mapping、preflight_report 和 final_report，便于在导出后继续走质量 gate。'

def vector_export_shapefile(self, layer_ref: str, output_directory: str, file_name: str = "", task_name: str = "", overwrite: bool = False, auto_truncate_field_names: bool = True, confirm_write: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
    """Handle a shapefile export."""
    return self._run(self._vector_export_shapefile_impl, layer_ref, output_directory, file_name, task_name, overwrite, auto_truncate_field_names, confirm_write, confirm_destructive)

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
    """Implement the shapefile export logic."""
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

def _long_field_names(self, layer: QgsVectorLayer) -> list[str]:
    """Handle long field names."""
    return [field.name() for field in layer.fields() if len(field.name()) > 10]

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

def _record_run_output(self, output_key: str, value: str) -> None:
    """Handle record run output."""
    outputs = self._shapefile_run_summary.setdefault("outputs", {})
    bucket = outputs.setdefault(output_key, [])
    if value not in bucket:
        bucket.append(value)

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

def _resolve_filesystem_write_path(self, path: str | Path) -> Path:
    """Resolve filesystem write path."""
    return self._resolve_filesystem_query_path(path)

def _resolve_vector_layer_ref(self, layer_ref: Any) -> QgsVectorLayer:
    """Resolve vector layer ref."""
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.VectorLayer:
        raise Exception(f"Layer is not a vector layer: {layer_ref}")
    return layer

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

@staticmethod
def _copy_layer_name(layer: QgsMapLayer, suffix: str = "copy") -> str:
    """Handle copy layer name."""
    base_name = (layer.name() or "layer").strip() or "layer"
    clean_suffix = suffix.strip().replace(" ", "_") if suffix else "copy"
    return f"{base_name}_{clean_suffix}"

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

TOOL_METHODS: dict[str, object] = {
    'vector_export_shapefile': vector_export_shapefile,
    '_vector_export_shapefile_impl': _vector_export_shapefile_impl,
    '_append_shapefile_run_step': _append_shapefile_run_step,
    '_ensure_filesystem_write_confirmed': _ensure_filesystem_write_confirmed,
    '_ensure_shapefile_run_summary': _ensure_shapefile_run_summary,
    '_inspect_vector_layer_validity': _inspect_vector_layer_validity,
    '_long_field_names': _long_field_names,
    '_materialize_vector_layer': _materialize_vector_layer,
    '_normalize_output_stem': _normalize_output_stem,
    '_normalize_task_name': _normalize_task_name,
    '_ok_result': _ok_result,
    '_record_run_output': _record_run_output,
    '_rename_layer_fields_for_shapefile': _rename_layer_fields_for_shapefile,
    '_resolve_filesystem_write_path': _resolve_filesystem_write_path,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_tag_work_layer': _tag_work_layer,
    '_serialize_value': _serialize_value,
    '_utc_now_iso': _utc_now_iso,
    '_reset_shapefile_run_summary': _reset_shapefile_run_summary,
    '_collect_null_attribute_fields': _collect_null_attribute_fields,
    '_display_geometry_type': _display_geometry_type,
    '_field_index': _field_index,
    '_parse_target_crs': _parse_target_crs,
    '_copy_layer_name': _copy_layer_name,
    '_apply_vector_edit': _apply_vector_edit,
    '_normalize_shapefile_field_name': _normalize_shapefile_field_name,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_resolve_layer_ref': _resolve_layer_ref,
    '_empty_shapefile_run_summary': _empty_shapefile_run_summary,
    '_begin_vector_edit': _begin_vector_edit,
    '_finish_vector_edit': _finish_vector_edit,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
