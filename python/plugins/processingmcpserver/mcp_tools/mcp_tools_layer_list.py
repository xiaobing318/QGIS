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

TOOL_NAME = 'layer_list'
TOOL_DOC = '列出当前工程中可见或全部图层的基础清单，用于给模型建立当前项目上下文。 layer_types 可选 vector、raster 或 both，include_hidden 控制是否包含图层面板中隐藏图层，name_glob 用于名称通配过滤。 当前工程中至少存在已加载图层时结果更有意义，但空工程也可调用。 无写操作，只读取工程图层注册表与图层树可见性。 无。 返回按过滤条件匹配的图层数组，元素包含 id、name、type、visible、provider 以及矢量或栅格的补充摘要。'

def layer_list(self, layer_types: str = "both", include_hidden: bool = True, name_glob: str = "*") -> list[dict[str, Any]]:
    """Handle the loaded layer list."""
    return self._run(self._layer_list_impl, layer_types, include_hidden, name_glob)

def _layer_list_impl(self, layer_types: str, include_hidden: bool, name_glob: str) -> list[dict[str, Any]]:
    """Build the the loaded layer list."""
    normalized_types = self._normalize_layer_types_filter(layer_types)
    pattern = self._normalize_name_glob(name_glob)
    project = QgsProject.instance()
    entries: list[dict[str, Any]] = []
    for layer_id, layer in project.mapLayers().items():
        is_vector = layer.type() == QgsMapLayer.VectorLayer
        is_raster = layer.type() == QgsMapLayer.RasterLayer
        if normalized_types == "vector" and not is_vector:
            continue
        if normalized_types == "raster" and not is_raster:
            continue
        if not fnmatch(layer.name(), pattern):
            continue
        visible = self._is_layer_visible(project, layer_id)
        if not include_hidden and not visible:
            continue
        item: dict[str, Any] = {
            "id": layer_id,
            "name": layer.name(),
            "type": self._layer_type_token(layer),
            "visible": visible,
            "provider": layer.providerType() if hasattr(layer, "providerType") else "",
        }
        if is_vector:
            item["feature_count"] = layer.featureCount()
            item["fields"] = [f.name() for f in layer.fields()]
        elif is_raster:
            item["width"] = layer.width()
            item["height"] = layer.height()
            item["band_count"] = layer.bandCount()
        entries.append(item)
    return entries

@staticmethod
def _is_layer_visible(project: QgsProject, layer_id: str) -> bool:
    """Handle is layer visible."""
    node = project.layerTreeRoot().findLayer(layer_id) if project.layerTreeRoot() else None
    if node is None:
        return False
    try:
        return bool(node.isVisible())
    except Exception:
        return False

@staticmethod
def _layer_type_token(layer: QgsMapLayer) -> str:
    """Handle layer type token."""
    if layer.type() == QgsMapLayer.VectorLayer:
        return f"vector_{int(layer.geometryType())}"
    if layer.type() == QgsMapLayer.RasterLayer:
        return "raster"
    return str(int(layer.type()))

@staticmethod
def _normalize_layer_types_filter(layer_types: str | None) -> str:
    """Handle normalize layer types filter."""
    value = (layer_types or "both").strip().lower()
    if value in {"both", "all", "any"}:
        return "both"
    if value in {"vector", "raster"}:
        return value
    raise Exception(f"Invalid layer_types: {layer_types}")

@staticmethod
def _normalize_name_glob(name_glob: str | None) -> str:
    """Handle normalize name glob."""
    value = (name_glob or "*").strip()
    return value or "*"

TOOL_METHODS: dict[str, object] = {
    'layer_list': layer_list,
    '_layer_list_impl': _layer_list_impl,
    '_is_layer_visible': _is_layer_visible,
    '_layer_type_token': _layer_type_token,
    '_normalize_layer_types_filter': _normalize_layer_types_filter,
    '_normalize_name_glob': _normalize_name_glob,
}
