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

TOOL_NAME = 'layer_remove_batch'
TOOL_DOC = '???从当前工程中批量移除多个图层。 ?????layer_ids 是 layer id 数组，空白值会被忽略。 ?????建议先用 layer_list 确认待删除 id；不存在的 id 不会抛错，而是记录到 missing。 ??????会修改当前工程图层列表和图层面板，但不会删除底层数据文件。 ?????无 confirm_destructive 开关，因此应只传入已确认的 layer id。 ?????返回 removed 与 missing 两个数组，便于区分成功移除和未命中的目标。'

def layer_remove_batch(self, layer_ids: list[str]) -> dict[str, list[str]]:
    """Handle multiple layers from the project."""
    return self._run(self._layer_remove_batch_impl, layer_ids)

def _layer_remove_batch_impl(self, layer_ids: list[str]) -> dict[str, list[str]]:
    """Build the multiple layers from the project."""
    project = QgsProject.instance()
    removed: list[str] = []
    missing: list[str] = []
    for layer_id in layer_ids or []:
        value = str(layer_id).strip()
        if not value:
            continue
        if value in project.mapLayers():
            project.removeMapLayer(value)
            removed.append(value)
        else:
            missing.append(value)
    return {"removed": removed, "missing": missing}

TOOL_METHODS: dict[str, object] = {
    'layer_remove_batch': layer_remove_batch,
    '_layer_remove_batch_impl': _layer_remove_batch_impl,
}
