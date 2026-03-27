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

TOOL_NAME = 'processing_list_providers'
TOOL_DOC = '???列出当前 QGIS Processing 注册表中的 provider 清单。 ?????无业务输入。 ?????Processing 运行时必须可用。 ??????无写操作，只读取 Processing 注册表。 ?????无。 ?????返回 provider 的 id、name、active、algorithm_count，以及 count_scope、total_algorithm_count、active_provider_count、active_algorithm_count 和总数。'

def processing_list_providers(self) -> dict[str, Any]:
    """执行 Processing 相关的 list providers 逻辑。"""
    return self._run(self._processing_list_providers_impl)

def _processing_list_providers_impl(self) -> dict[str, Any]:
    """执行 Processing 相关的 list providers impl 逻辑。"""
    self._ensure_processing_runtime()
    providers = list(QgsApplication.processingRegistry().providers())
    payload = []
    total_algorithm_count = 0
    active_provider_count = 0
    active_algorithm_count = 0
    for provider in providers:
        active = True
        if hasattr(provider, "isActive") and callable(provider.isActive):
            try:
                active = bool(provider.isActive())
            except Exception:
                active = True
        algorithm_count = 0
        if hasattr(provider, "algorithms") and callable(provider.algorithms):
            try:
                algorithm_count = len(provider.algorithms())
            except Exception:
                algorithm_count = 0
        total_algorithm_count += algorithm_count
        if active:
            active_provider_count += 1
            active_algorithm_count += algorithm_count
        payload.append(
            {
                "id": provider.id(),
                "name": provider.name(),
                "active": active,
                "algorithm_count": algorithm_count,
            }
        )
    return {
        "count": len(payload),
        "count_scope": "registered_providers",
        "total_algorithm_count": total_algorithm_count,
        "active_provider_count": active_provider_count,
        "active_algorithm_count": active_algorithm_count,
        "providers": payload,
    }

def _ensure_processing_runtime(self) -> None:
    """确保 processing runtime 已就绪。"""
    _ensure_processing_initialized()

TOOL_METHODS: dict[str, object] = {
    'processing_list_providers': processing_list_providers,
    '_processing_list_providers_impl': _processing_list_providers_impl,
    '_ensure_processing_runtime': _ensure_processing_runtime,
}
