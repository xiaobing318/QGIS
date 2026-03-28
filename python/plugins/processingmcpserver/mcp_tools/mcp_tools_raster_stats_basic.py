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

TOOL_NAME = 'raster_stats_basic'
TOOL_DOC = '???读取栅格单个波段的基础统计量。 ?????layer_ref 指向栅格图层，band 指定波段号，默认 1。 ?????目标图层必须存在且为栅格，指定波段必须有效。 ??????无写操作，只读取栅格波段统计信息。 ?????无。 ?????返回目标波段的最小值、最大值、均值、标准差等基础统计摘要。'

def raster_stats_basic(self, layer_ref: str, band: int = 1) -> dict[str, Any]:
    """Handle basic raster statistics."""
    return self._run(self._raster_stats_basic_impl, layer_ref, band)

def _raster_stats_basic_impl(self, layer_ref: str, band: int) -> dict[str, Any]:
    """Build the basic raster statistics."""
    layer = self._resolve_raster_layer_ref(layer_ref)
    band_number = max(1, self._safe_int(band, 1))
    stats = layer.dataProvider().bandStatistics(band_number)
    return self._ok_result("raster_stats_basic", summary={"band": band_number, "count": int(stats.elementCount), "min": float(stats.minimumValue), "max": float(stats.maximumValue), "mean": float(stats.mean), "std_dev": float(stats.stdDev)}, outputs={"layer_id": layer.id()})

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

@staticmethod
def _safe_int(value: Any, default: int) -> int:
    """Handle safe int."""
    try:
        return int(value)
    except (TypeError, ValueError):
        return default

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
    'raster_stats_basic': raster_stats_basic,
    '_raster_stats_basic_impl': _raster_stats_basic_impl,
    '_ok_result': _ok_result,
    '_resolve_raster_layer_ref': _resolve_raster_layer_ref,
    '_safe_int': _safe_int,
    '_resolve_layer_ref': _resolve_layer_ref,
}
