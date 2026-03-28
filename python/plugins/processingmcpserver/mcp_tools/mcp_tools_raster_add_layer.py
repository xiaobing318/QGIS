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

TOOL_NAME = 'raster_add_layer'
TOOL_DOC = '???把单个栅格数据源加载到当前 QGIS 工程。 ?????path 指向栅格文件或数据源，provider 默认 gdal，name 可覆盖图层显示名。 ?????目标路径必须存在且能被 GDAL 或指定 provider 识别。 ??????会向当前工程新增一个栅格图层，但不会改写源数据文件。 ?????无 destructive 开关；路径或数据源无效时直接失败。 ?????返回新图层的 id、name、type、width、height 与 band_count。'

def raster_add_layer(self, path: str, provider: str = "gdal", name: str | None = None) -> dict[str, Any]:
    """Handle a raster layer."""
    return self._run(self._raster_add_layer_impl, path, provider, name)

def _raster_add_layer_impl(self, path: str, provider: str = "gdal", name: str | None = None) -> dict[str, Any]:
    """Build the raster layer."""
    layer = QgsRasterLayer(path, name or Path(path).stem, provider)
    if not layer.isValid():
        raise Exception(f"Layer is not valid: {path}")
    QgsProject.instance().addMapLayer(layer)
    return {"id": layer.id(), "name": layer.name(), "type": "raster", "width": layer.width(), "height": layer.height(), "band_count": layer.bandCount()}

TOOL_METHODS: dict[str, object] = {
    'raster_add_layer': raster_add_layer,
    '_raster_add_layer_impl': _raster_add_layer_impl,
}
