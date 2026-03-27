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

TOOL_NAME = 'vector_stats_basic'
TOOL_DOC = '???计算矢量字段的基础统计量，适合先判断数值分布。 ?????layer_ref 指向矢量图层，field_name 是目标字段。 ?????目标图层必须存在且字段存在，字段最好是可统计的数值型字段。 ??????无写操作，只读取字段值并做统计。 ?????无。 ?????返回 count、sum、mean、min、max、stdev 等基础统计摘要。'

def vector_stats_basic(self, layer_ref: str, field_name: str) -> dict[str, Any]:
    """执行矢量相关的 stats basic 逻辑。"""
    return self._run(self._vector_stats_basic_impl, layer_ref, field_name)

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

@staticmethod
def _field_index(layer: QgsVectorLayer, field_name: str) -> int:
    """执行 field index 相关逻辑。"""
    return layer.fields().indexFromName(field_name)

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """执行 ok result 相关逻辑。"""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _resolve_vector_layer_ref(self, layer_ref: Any) -> QgsVectorLayer:
    """解析 vector layer ref。"""
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.VectorLayer:
        raise Exception(f"Layer is not a vector layer: {layer_ref}")
    return layer

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
    'vector_stats_basic': vector_stats_basic,
    '_vector_stats_basic_impl': _vector_stats_basic_impl,
    '_field_index': _field_index,
    '_ok_result': _ok_result,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_resolve_layer_ref': _resolve_layer_ref,
}
