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

TOOL_NAME = 'processing_list_providers'
TOOL_DOC = '列出当前 QGIS Processing 注册表中的 provider 清单。 无业务输入。 Processing 运行时必须可用。 无写操作，只读取 Processing 注册表。 无。 返回 provider 的 id、name、active、algorithm_count，以及 count_scope、total_algorithm_count、active_provider_count、active_algorithm_count 和总数。'

def processing_list_providers(self) -> dict[str, Any]:
    """
    作用：处理 `processing_list_providers` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `processing_list_providers` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._processing_list_providers_impl)

def _processing_list_providers_impl(self) -> dict[str, Any]:
    """
    作用：实现 `_processing_list_providers_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_processing_list_providers_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
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
    """
    作用：确保 `_ensure_processing_runtime` 负责的前置状态可用，必要时执行初始化或修复动作。
    用途：确保 `_ensure_processing_runtime` 负责的前置状态可用，必要时执行初始化或修复动作。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    _ensure_processing_initialized()

TOOL_METHODS: dict[str, object] = {
    'processing_list_providers': processing_list_providers,
    '_processing_list_providers_impl': _processing_list_providers_impl,
    '_ensure_processing_runtime': _ensure_processing_runtime,
}
