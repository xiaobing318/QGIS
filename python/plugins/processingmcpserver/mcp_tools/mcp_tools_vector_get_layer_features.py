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

TOOL_NAME = 'vector_get_layer_features'
TOOL_DOC = '提取矢量图层的样本要素，供模型观察字段值和几何概貌。 layer_ref 指向矢量图层，limit 是希望返回的要素数。 目标图层必须存在且为矢量图层。 无写操作，只顺序读取要素并序列化属性与 geometry_wkt。 limit 会被内部最大阈值裁剪，返回里会明确给出 requested_limit、applied_limit 与 limit_capped。 返回 layer_id、feature_count、fields、features 以及 limit 应用结果。'

def vector_get_layer_features(self, layer_ref: str, limit: int = DEFAULT_FEATURE_LIMIT) -> dict[str, Any]:
    """
    作用：处理 `vector_get_layer_features` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `vector_get_layer_features` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。 默认值为 `DEFAULT_FEATURE_LIMIT`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._vector_get_layer_features_impl, layer_ref, limit)

def _vector_get_layer_features_impl(self, layer_ref: str, limit: int) -> dict[str, Any]:
    """
    作用：实现 `_vector_get_layer_features_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_vector_get_layer_features_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    layer = self._resolve_vector_layer_ref(layer_ref)
    requested, applied, capped = self._normalize_feature_limit(limit)
    features: list[dict[str, Any]] = []
    for index, feature in enumerate(layer.getFeatures()):
        if index >= applied:
            break
        attrs = {
            field.name(): self._serialize_value(feature.attribute(field.name()))
            for field in layer.fields()
        }
        features.append(
            {
                "id": feature.id(),
                "attributes": attrs,
                "geometry_wkt": feature.geometry().asWkt(precision=6) if feature.hasGeometry() else None,
            }
        )
    return {
        "layer_id": layer.id(),
        "feature_count": layer.featureCount(),
        "fields": [f.name() for f in layer.fields()],
        "features": features,
        "requested_limit": requested,
        "applied_limit": applied,
        "limit_capped": capped,
        "max_feature_limit": self.MAX_FEATURE_LIMIT,
    }

def _normalize_feature_limit(self, limit: Any) -> tuple[int, int, bool]:
    """
    作用：封装内部辅助步骤 `_normalize_feature_limit`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_normalize_feature_limit`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `limit`（`Any`）：数值控制参数，用于限制范围、数量或时限。
    - 返回：返回 `tuple[int, int, bool]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[int, int, bool]` 类型结果，返回值语义遵循该函数实现约定。
    """
    requested = self._safe_int(limit, self.DEFAULT_FEATURE_LIMIT)
    normalized = max(0, requested)
    applied = min(normalized, self.MAX_FEATURE_LIMIT)
    return requested, applied, applied != normalized

def _resolve_vector_layer_ref(self, layer_ref: Any) -> QgsVectorLayer:
    """
    作用：封装内部辅助步骤 `_resolve_vector_layer_ref`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_resolve_vector_layer_ref`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`Any`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `QgsVectorLayer` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `QgsVectorLayer` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.VectorLayer:
        raise Exception(f"Layer is not a vector layer: {layer_ref}")
    return layer

@staticmethod
def _serialize_value(value: Any) -> Any:
    """
    作用：封装内部辅助步骤 `_serialize_value`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_serialize_value`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `value`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
    """
    if value is None:
        return None
    if isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, (date, datetime, time_value)):
        return value.isoformat()
    if isinstance(value, QDateTime):
        return value.toString(Qt.DateFormat.ISODateWithMs)
    if isinstance(value, QDate):
        return value.toString(Qt.DateFormat.ISODate)
    if isinstance(value, QTime):
        return value.toString(Qt.DateFormat.ISODateWithMs)
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, (list, tuple, set)):
        return [_serialize_value(item) for item in value]
    if isinstance(value, dict):
        return {str(k): _serialize_value(v) for k, v in value.items()}
    if hasattr(value, "id") and callable(value.id):
        try:
            return value.id()
        except Exception:
            pass
    if hasattr(value, "toString") and callable(value.toString):
        try:
            return value.toString()
        except Exception:
            pass
    return str(value)

@staticmethod
def _safe_int(value: Any, default: int) -> int:
    """
    作用：封装内部辅助步骤 `_safe_int`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_safe_int`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `value`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `default`（`int`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `int` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `int` 类型结果，返回值语义遵循该函数实现约定。
    """
    try:
        return int(value)
    except (TypeError, ValueError):
        return default

def _resolve_layer_ref(self, layer_ref: Any) -> QgsMapLayer:
    """
    作用：封装内部辅助步骤 `_resolve_layer_ref`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_resolve_layer_ref`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`Any`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `QgsMapLayer` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `QgsMapLayer` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
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
    'vector_get_layer_features': vector_get_layer_features,
    '_vector_get_layer_features_impl': _vector_get_layer_features_impl,
    '_normalize_feature_limit': _normalize_feature_limit,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_serialize_value': _serialize_value,
    '_safe_int': _safe_int,
    '_resolve_layer_ref': _resolve_layer_ref,
}
