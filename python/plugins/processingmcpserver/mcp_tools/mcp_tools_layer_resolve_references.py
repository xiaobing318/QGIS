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

TOOL_NAME = 'layer_resolve_references'
TOOL_DOC = '???把图层名称或 layer id 解析成唯一 layer id，便于后续安全调用其它工具。 ?????refs 是待解析引用数组，strict 控制遇到 missing 或 ambiguous 时是返回详情还是直接失败。 ?????目标引用应来自当前工程；若名称重复会被归类为 ambiguous。 ??????无写操作，只读取当前工程图层注册表。 ?????strict=false 时会尽量返回 resolved、missing、ambiguous；strict=true 时只要存在缺失或歧义就抛错。 ?????返回 resolved 映射、missing 数组和 ambiguous 映射。'

def layer_resolve_references(self, refs: list[str], strict: bool = False) -> dict[str, Any]:
    """Handle layer references."""
    return self._run(self._layer_resolve_references_impl, refs, strict)

def _layer_resolve_references_impl(self, refs: list[str], strict: bool) -> dict[str, Any]:
    """Build the layer references."""
    resolved: dict[str, str] = {}
    missing: list[str] = []
    ambiguous: dict[str, list[str]] = {}
    layers = QgsProject.instance().mapLayers()
    for ref in refs or []:
        text = str(ref).strip()
        if not text:
            continue
        if text in layers:
            resolved[text] = text
            continue

        matches = [layer for layer in layers.values() if layer.name() == text]
        if not matches:
            missing.append(text)
            continue
        if len(matches) > 1:
            ambiguous[text] = [layer.id() for layer in matches]
            continue
        resolved[text] = matches[0].id()
    if strict and (missing or ambiguous):
        details: list[str] = []
        if missing:
            details.append(f"missing={missing}")
        if ambiguous:
            details.append(f"ambiguous={ambiguous}")
        raise Exception("Layer reference resolve failed: " + "; ".join(details))
    return {"resolved": resolved, "missing": missing, "ambiguous": ambiguous}

TOOL_METHODS: dict[str, object] = {
    'layer_resolve_references': layer_resolve_references,
    '_layer_resolve_references_impl': _layer_resolve_references_impl,
}
