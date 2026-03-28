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

TOOL_NAME = 'layer_get_details'
TOOL_DOC = '按图层 id 或图层名读取单个图层的详细元数据。 layer_ref 可以是唯一 layer id，也可以是能唯一解析的图层名称。 目标图层必须已经加载到当前工程，名称引用不能歧义。 无写操作，只读取图层元数据与字段摘要。 无。 返回 id、name、type、provider、source、crs，以及矢量的 fields 和 feature_count 或栅格的尺寸与波段数。'

def layer_get_details(self, layer_ref: str) -> dict[str, Any]:
    """
    作用：处理 `layer_get_details` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `layer_get_details` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._layer_get_details_impl, layer_ref)

def _layer_get_details_impl(self, layer_ref: str) -> dict[str, Any]:
    """
    作用：实现 `_layer_get_details_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_layer_get_details_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    layer = self._resolve_layer_ref(layer_ref)
    result: dict[str, Any] = {
        "id": layer.id(),
        "name": layer.name(),
        "type": self._layer_type_token(layer),
        "provider": layer.providerType() if hasattr(layer, "providerType") else "",
        "source": layer.source() if hasattr(layer, "source") else "",
        "crs": layer.crs().authid() if hasattr(layer, "crs") and layer.crs().isValid() else "",
    }
    if layer.type() == QgsMapLayer.VectorLayer:
        result["feature_count"] = layer.featureCount()
        result["fields"] = [f.name() for f in layer.fields()]
    elif layer.type() == QgsMapLayer.RasterLayer:
        result["width"] = layer.width()
        result["height"] = layer.height()
        result["band_count"] = layer.bandCount()
    return result

@staticmethod
def _layer_type_token(layer: QgsMapLayer) -> str:
    """
    作用：封装内部辅助步骤 `_layer_type_token`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_layer_type_token`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `layer`（`QgsMapLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    if layer.type() == QgsMapLayer.VectorLayer:
        return f"vector_{int(layer.geometryType())}"
    if layer.type() == QgsMapLayer.RasterLayer:
        return "raster"
    return str(int(layer.type()))

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
    'layer_get_details': layer_get_details,
    '_layer_get_details_impl': _layer_get_details_impl,
    '_layer_type_token': _layer_type_token,
    '_resolve_layer_ref': _resolve_layer_ref,
}
