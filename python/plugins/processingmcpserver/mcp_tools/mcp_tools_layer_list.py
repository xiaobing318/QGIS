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

TOOL_NAME = 'layer_list'
TOOL_DOC = '列出当前工程中可见或全部图层的基础清单，用于给模型建立当前项目上下文。 layer_types 可选 vector、raster 或 both，include_hidden 控制是否包含图层面板中隐藏图层，name_glob 用于名称通配过滤。 当前工程中至少存在已加载图层时结果更有意义，但空工程也可调用。 无写操作，只读取工程图层注册表与图层树可见性。 无。 返回按过滤条件匹配的图层数组，元素包含 id、name、type、visible、provider 以及矢量或栅格的补充摘要。'

def layer_list(self, layer_types: str = "both", include_hidden: bool = True, name_glob: str = "*") -> list[dict[str, Any]]:
    """
    作用：处理 `layer_list` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `layer_list` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_types`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"both"`。
    - 参数 `include_hidden`（`bool`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `True`。
    - 参数 `name_glob`（`str`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `"*"`。
    - 返回：返回 `list[dict[str, Any]]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `list[dict[str, Any]]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._layer_list_impl, layer_types, include_hidden, name_glob)

def _layer_list_impl(self, layer_types: str, include_hidden: bool, name_glob: str) -> list[dict[str, Any]]:
    """
    作用：实现 `_layer_list_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_layer_list_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_types`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `include_hidden`（`bool`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `name_glob`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `list[dict[str, Any]]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `list[dict[str, Any]]` 类型结果，返回值语义遵循该函数实现约定。
    """
    normalized_types = self._normalize_layer_types_filter(layer_types)
    pattern = self._normalize_name_glob(name_glob)
    project = QgsProject.instance()
    entries: list[dict[str, Any]] = []
    for layer_id, layer in project.mapLayers().items():
        is_vector = layer.type() == QgsMapLayer.VectorLayer
        is_raster = layer.type() == QgsMapLayer.RasterLayer
        if normalized_types == "vector" and not is_vector:
            continue
        if normalized_types == "raster" and not is_raster:
            continue
        if not fnmatch(layer.name(), pattern):
            continue
        visible = self._is_layer_visible(project, layer_id)
        if not include_hidden and not visible:
            continue
        item: dict[str, Any] = {
            "id": layer_id,
            "name": layer.name(),
            "type": self._layer_type_token(layer),
            "visible": visible,
            "provider": layer.providerType() if hasattr(layer, "providerType") else "",
        }
        if is_vector:
            item["feature_count"] = layer.featureCount()
            item["fields"] = [f.name() for f in layer.fields()]
        elif is_raster:
            item["width"] = layer.width()
            item["height"] = layer.height()
            item["band_count"] = layer.bandCount()
        entries.append(item)
    return entries

@staticmethod
def _is_layer_visible(project: QgsProject, layer_id: str) -> bool:
    """
    作用：封装内部辅助步骤 `_is_layer_visible`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_is_layer_visible`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `project`（`QgsProject`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `layer_id`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
    """
    node = project.layerTreeRoot().findLayer(layer_id) if project.layerTreeRoot() else None
    if node is None:
        return False
    try:
        return bool(node.isVisible())
    except Exception:
        return False

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

@staticmethod
def _normalize_layer_types_filter(layer_types: str | None) -> str:
    """
    作用：封装内部辅助步骤 `_normalize_layer_types_filter`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_normalize_layer_types_filter`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `layer_types`（`str | None`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    value = (layer_types or "both").strip().lower()
    if value in {"both", "all", "any"}:
        return "both"
    if value in {"vector", "raster"}:
        return value
    raise Exception(f"Invalid layer_types: {layer_types}")

@staticmethod
def _normalize_name_glob(name_glob: str | None) -> str:
    """
    作用：封装内部辅助步骤 `_normalize_name_glob`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_normalize_name_glob`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `name_glob`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    value = (name_glob or "*").strip()
    return value or "*"

TOOL_METHODS: dict[str, object] = {
    'layer_list': layer_list,
    '_layer_list_impl': _layer_list_impl,
    '_is_layer_visible': _is_layer_visible,
    '_layer_type_token': _layer_type_token,
    '_normalize_layer_types_filter': _normalize_layer_types_filter,
    '_normalize_name_glob': _normalize_name_glob,
}
