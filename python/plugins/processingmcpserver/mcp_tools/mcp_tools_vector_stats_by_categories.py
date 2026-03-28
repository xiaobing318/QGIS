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

TOOL_NAME = 'vector_stats_by_categories'
TOOL_DOC = '???按一个或多个分类字段汇总记录数或数值字段统计，适合做分组分析。 ?????layer_ref 指向矢量图层，category_fields 是分类字段数组，values_field 可选；为空时通常只做分组计数。 ?????目标图层必须存在，分类字段必须存在；若提供 values_field，该字段也必须存在。 ??????无写操作，只读取要素属性并做分组汇总。 ?????无。 ?????返回按分类组合分组后的统计结果，便于模型理解类别分布。'

def vector_stats_by_categories(self, layer_ref: str, category_fields: list[str], values_field: str | None = None) -> dict[str, Any]:
    """Handle vector statistics by category."""
    return self._run(self._vector_stats_by_categories_impl, layer_ref, category_fields, values_field)

def _vector_stats_by_categories_impl(self, layer_ref: str, category_fields: list[str], values_field: str | None) -> dict[str, Any]:
    """Build the vector statistics by category."""
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

@staticmethod
def _field_index(layer: QgsVectorLayer, field_name: str) -> int:
    """Handle field index."""
    return layer.fields().indexFromName(field_name)

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """Handle ok result."""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _resolve_vector_layer_ref(self, layer_ref: Any) -> QgsVectorLayer:
    """Resolve vector layer ref."""
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.VectorLayer:
        raise Exception(f"Layer is not a vector layer: {layer_ref}")
    return layer

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
    'vector_stats_by_categories': vector_stats_by_categories,
    '_vector_stats_by_categories_impl': _vector_stats_by_categories_impl,
    '_field_index': _field_index,
    '_ok_result': _ok_result,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_serialize_value': _serialize_value,
    '_resolve_layer_ref': _resolve_layer_ref,
}
