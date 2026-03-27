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

TOOL_NAME = 'raster_stats_cell'
TOOL_DOC = '???对多个栅格做逐像元统计运算，例如逐像元均值、最小值或最大值。 ?????raster_layer_refs 是栅格引用数组，statistic 选择逐像元统计类型，ignore_nodata 控制是否忽略 NoData。 ?????至少提供一个有效栅格图层，且各栅格最好在分辨率、范围和 CRS 上可兼容处理。 ??????会运行 Processing 栅格统计算法，通常产生新的输出栅格结果；是否自动加载取决于算法默认行为和 Processing 设置。 ?????无 destructive 开关，但会触发 Processing 执行。 ?????返回算法摘要和序列化后的 Processing 输出结果。'

def raster_stats_cell(self, raster_layer_refs: list[str], statistic: int = 0, ignore_nodata: bool = True) -> dict[str, Any]:
    """执行栅格相关的 stats cell 逻辑。"""
    return self._run(self._raster_stats_cell_impl, raster_layer_refs, statistic, ignore_nodata)

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

def _ensure_processing_runtime(self) -> None:
    """确保 processing runtime 已就绪。"""
    _ensure_processing_initialized()

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """执行 ok result 相关逻辑。"""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _resolve_raster_layer_ref(self, layer_ref: Any) -> QgsRasterLayer:
    """解析 raster layer ref。"""
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.RasterLayer:
        raise Exception(f"Layer is not a raster layer: {layer_ref}")
    return layer

@staticmethod
def _safe_int(value: Any, default: int) -> int:
    """执行 safe int 相关逻辑。"""
    try:
        return int(value)
    except (TypeError, ValueError):
        return default

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
    'raster_stats_cell': raster_stats_cell,
    '_raster_stats_cell_impl': _raster_stats_cell_impl,
    '_coerce_output_identifier': _coerce_output_identifier,
    '_ensure_processing_runtime': _ensure_processing_runtime,
    '_ok_result': _ok_result,
    '_resolve_raster_layer_ref': _resolve_raster_layer_ref,
    '_safe_int': _safe_int,
    '_serialize_value': _serialize_value,
    '_resolve_layer_ref': _resolve_layer_ref,
}
