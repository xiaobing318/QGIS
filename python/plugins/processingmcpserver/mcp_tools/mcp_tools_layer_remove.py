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

TOOL_NAME = 'layer_remove'
TOOL_DOC = '???从当前工程中移除单个图层。 ?????layer_id 必须是当前工程已存在的 layer id，不接受模糊名称。 ?????目标图层必须已经在当前工程注册。 ??????会修改当前工程图层列表和图层面板，但不会删除底层数据文件。 ?????无 confirm_destructive 开关，因此调用前应先用 layer_list 或 layer_get_details 复核目标 id。 ?????返回 removed 字段，值为已移除的 layer id。'

def layer_remove(self, layer_id: str) -> dict[str, str]:
    """Handle a layer from the project."""
    return self._run(self._layer_remove_impl, layer_id)

def _layer_remove_impl(self, layer_id: str) -> dict[str, str]:
    """Build the layer from the project."""
    project = QgsProject.instance()
    if layer_id not in project.mapLayers():
        raise Exception(f"Layer not found: {layer_id}")
    project.removeMapLayer(layer_id)
    return {"removed": layer_id}

TOOL_METHODS: dict[str, object] = {
    'layer_remove': layer_remove,
    '_layer_remove_impl': _layer_remove_impl,
}
