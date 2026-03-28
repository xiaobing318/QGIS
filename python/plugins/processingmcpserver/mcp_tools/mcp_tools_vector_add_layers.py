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

TOOL_NAME = 'vector_add_layers'
TOOL_DOC = '批量把多个矢量数据源加载到当前 QGIS 工程。 paths 是待加载路径数组，provider 默认 ogr，skip_invalid 控制遇到坏数据时是跳过还是整体失败。 至少提供一个可访问路径，且对应数据源应能被 provider 识别。 会向当前工程新增多个矢量图层，但不会改写源数据文件。 skip_invalid=true 时会尽量继续加载其余路径；skip_invalid=false 时任何一个失败都会报错终止。 返回 requested_count、loaded_count、failed_count，以及 loaded 和 failed 的逐项结果。'

def vector_add_layers(self, paths: list[str], provider: str = "ogr", skip_invalid: bool = True) -> dict[str, Any]:
    """Handle multiple vector layers."""
    return self._run(self._vector_add_layers_impl, paths, provider, skip_invalid)

def _vector_add_layers_impl(self, paths: list[str], provider: str, skip_invalid: bool) -> dict[str, Any]:
    """Build the multiple vector layers."""
    loaded: list[dict[str, Any]] = []
    failed: list[dict[str, Any]] = []
    for path in paths or []:
        try:
            loaded.append(self._vector_add_layer_impl(path, provider=provider))
        except Exception as exc:
            if not skip_invalid:
                raise Exception(f"Failed to load vector dataset: {path}. {exc}") from exc
            failed.append({"path": path, "error": str(exc)})
    return {"requested_count": len(paths or []), "loaded_count": len(loaded), "failed_count": len(failed), "loaded": loaded, "failed": failed}

def _vector_add_layer_impl(self, path: str, provider: str = "ogr", name: str | None = None) -> dict[str, Any]:
    """Build the vector layer."""
    layer = QgsVectorLayer(path, name or Path(path).stem, provider)
    if not layer.isValid():
        raise Exception(f"Layer is not valid: {path}")
    QgsProject.instance().addMapLayer(layer)
    return {"id": layer.id(), "name": layer.name(), "type": self._layer_type_token(layer), "feature_count": layer.featureCount()}

@staticmethod
def _layer_type_token(layer: QgsMapLayer) -> str:
    """Handle layer type token."""
    if layer.type() == QgsMapLayer.VectorLayer:
        return f"vector_{int(layer.geometryType())}"
    if layer.type() == QgsMapLayer.RasterLayer:
        return "raster"
    return str(int(layer.type()))

TOOL_METHODS: dict[str, object] = {
    'vector_add_layers': vector_add_layers,
    '_vector_add_layers_impl': _vector_add_layers_impl,
    '_vector_add_layer_impl': _vector_add_layer_impl,
    '_layer_type_token': _layer_type_token,
}
