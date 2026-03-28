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

TOOL_NAME = 'layer_get_details'
TOOL_DOC = '???按图层 id 或图层名读取单个图层的详细元数据。 ?????layer_ref 可以是唯一 layer id，也可以是能唯一解析的图层名称。 ?????目标图层必须已经加载到当前工程，名称引用不能歧义。 ??????无写操作，只读取图层元数据与字段摘要。 ?????无。 ?????返回 id、name、type、provider、source、crs，以及矢量的 fields 和 feature_count 或栅格的尺寸与波段数。'

def layer_get_details(self, layer_ref: str) -> dict[str, Any]:
    """Handle layer details."""
    return self._run(self._layer_get_details_impl, layer_ref)

def _layer_get_details_impl(self, layer_ref: str) -> dict[str, Any]:
    """Build the layer details."""
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

@staticmethod
def _layer_type_token(layer: QgsMapLayer) -> str:
    """Handle layer type token."""
    if layer.type() == QgsMapLayer.VectorLayer:
        return f"vector_{int(layer.geometryType())}"
    if layer.type() == QgsMapLayer.RasterLayer:
        return "raster"
    return str(int(layer.type()))

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
    'layer_get_details': layer_get_details,
    '_layer_get_details_impl': _layer_get_details_impl,
    '_layer_type_token': _layer_type_token,
    '_resolve_layer_ref': _resolve_layer_ref,
}
