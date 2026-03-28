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

TOOL_NAME = 'raster_stats_zonal'
TOOL_DOC = '把栅格统计结果按分区写入矢量图层属性表，常用于区域统计。 vector_layer_ref 指向分区矢量图层，raster_layer_ref 指向栅格图层，raster_band 指定波段，column_prefix 控制输出字段前缀，in_place 控制是否直接写回原矢量图层。 矢量与栅格图层都必须存在，且空间关系应满足分区统计需求。 会运行原生 zonal statistics 算法，并在矢量属性表增加统计字段；默认副本模式下生成新的输出图层。 默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 返回 summary.mode、algorithm、output_layer_id 和底层 Processing 结果对象。'

def raster_stats_zonal(self, vector_layer_ref: str, raster_layer_ref: str, raster_band: int = 1, column_prefix: str = "z_", in_place: bool = False) -> dict[str, Any]:
    """Handle zonal raster statistics."""
    return self._run(self._raster_stats_zonal_impl, vector_layer_ref, raster_layer_ref, raster_band, column_prefix, in_place)

def _raster_stats_zonal_impl(self, vector_layer_ref: str, raster_layer_ref: str, raster_band: int, column_prefix: str, in_place: bool) -> dict[str, Any]:
    """Build the zonal raster statistics."""
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

@staticmethod
def _coerce_output_identifier(value: Any) -> str:
    """Handle coerce output identifier."""
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

def _ensure_processing_runtime(self) -> None:
    """Handle ensure processing runtime."""
    _ensure_processing_initialized()

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """Handle ok result."""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _resolve_raster_layer_ref(self, layer_ref: Any) -> QgsRasterLayer:
    """Resolve raster layer ref."""
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.RasterLayer:
        raise Exception(f"Layer is not a raster layer: {layer_ref}")
    return layer

def _resolve_vector_layer_ref(self, layer_ref: Any) -> QgsVectorLayer:
    """Resolve vector layer ref."""
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.VectorLayer:
        raise Exception(f"Layer is not a vector layer: {layer_ref}")
    return layer

@staticmethod
def _safe_int(value: Any, default: int) -> int:
    """Handle safe int."""
    try:
        return int(value)
    except (TypeError, ValueError):
        return default

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
    'raster_stats_zonal': raster_stats_zonal,
    '_raster_stats_zonal_impl': _raster_stats_zonal_impl,
    '_coerce_output_identifier': _coerce_output_identifier,
    '_ensure_processing_runtime': _ensure_processing_runtime,
    '_ok_result': _ok_result,
    '_resolve_raster_layer_ref': _resolve_raster_layer_ref,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_safe_int': _safe_int,
    '_serialize_value': _serialize_value,
    '_resolve_layer_ref': _resolve_layer_ref,
}
