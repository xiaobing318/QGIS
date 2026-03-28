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
    """
    作用：确保 `_ensure_processing_initialized` 负责的前置状态可用，必要时执行初始化或修复动作。
    用途：确保 `_ensure_processing_initialized` 负责的前置状态可用，必要时执行初始化或修复动作。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数：无。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    global _PROCESSING_INITIALIZED
    if _PROCESSING_INITIALIZED:
        return
    Processing.initialize()
    _PROCESSING_INITIALIZED = True

TOOL_NAME = 'layer_resolve_references'
TOOL_DOC = '把图层名称或 layer id 解析成唯一 layer id，便于后续安全调用其它工具。 refs 是待解析引用数组，strict 控制遇到 missing 或 ambiguous 时是返回详情还是直接失败。 目标引用应来自当前工程；若名称重复会被归类为 ambiguous。 无写操作，只读取当前工程图层注册表。 strict=false 时会尽量返回 resolved、missing、ambiguous；strict=true 时只要存在缺失或歧义就抛错。 返回 resolved 映射、missing 数组和 ambiguous 映射。'

def layer_resolve_references(self, refs: list[str], strict: bool = False) -> dict[str, Any]:
    """
    作用：处理 `layer_resolve_references` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `layer_resolve_references` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `refs`（`list[str]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `strict`（`bool`）：布尔开关参数，用于控制是否启用特定行为。 默认值为 `False`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._layer_resolve_references_impl, refs, strict)

def _layer_resolve_references_impl(self, refs: list[str], strict: bool) -> dict[str, Any]:
    """
    作用：实现 `_layer_resolve_references_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_layer_resolve_references_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `refs`（`list[str]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `strict`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
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
