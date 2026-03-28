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

TOOL_NAME = 'layer_get_panel_tree'
TOOL_DOC = '读取图层面板树结构，帮助模型理解分组、顺序和隐藏状态。 include_hidden 控制是否把隐藏图层与分组也放入返回结构。 当前工程已初始化即可调用，最好已有图层树内容。 无写操作，只读取 QgsLayerTree 结构。 无。 返回 tree 主结构，以及为兼容旧调用保留的 groups、layers 扁平摘要，便于模型按图层面板顺序推理。'

def layer_get_panel_tree(self, include_hidden: bool = True) -> dict[str, Any]:
    """
    作用：处理 `layer_get_panel_tree` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `layer_get_panel_tree` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `include_hidden`（`bool`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `True`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._layer_get_panel_tree_impl, include_hidden)

def _layer_get_panel_tree_impl(self, include_hidden: bool) -> dict[str, Any]:
    """
    作用：实现 `_layer_get_panel_tree_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_layer_get_panel_tree_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `include_hidden`（`bool`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    root = QgsProject.instance().layerTreeRoot()
    groups: list[dict[str, Any]] = []
    layers: list[dict[str, Any]] = []
    project = QgsProject.instance()

    def layer_payload(layer: QgsMapLayer) -> dict[str, Any]:
        """
        作用：处理 `layer_payload` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `layer_payload` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
        参数与返回：
        - 参数 `layer`（`QgsMapLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
        - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
        """
        item: dict[str, Any] = {
            "id": layer.id(),
            "name": layer.name(),
            "type": self._layer_type_token(layer),
            "visible": self._is_layer_visible(project, layer.id()),
            "provider": layer.providerType() if hasattr(layer, "providerType") else "",
        }
        if layer.type() == QgsMapLayer.VectorLayer:
            item["feature_count"] = layer.featureCount()
            item["fields"] = [field.name() for field in layer.fields()]
        elif layer.type() == QgsMapLayer.RasterLayer:
            item["width"] = layer.width()
            item["height"] = layer.height()
            item["band_count"] = layer.bandCount()
        return item

    def walk(group: QgsLayerTreeGroup, prefix: str) -> list[dict[str, Any]]:
        """
        作用：处理 `walk` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `walk` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
        参数与返回：
        - 参数 `group`（`QgsLayerTreeGroup`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `prefix`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回 `list[dict[str, Any]]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `list[dict[str, Any]]` 类型结果，返回值语义遵循该函数实现约定。
        """
        children_payload: list[dict[str, Any]] = []
        for child in group.children():
            visible = bool(child.isVisible()) if hasattr(child, "isVisible") else True
            if not include_hidden and not visible:
                continue
            if isinstance(child, QgsLayerTreeGroup):
                path = f"{prefix}/{child.name()}" if prefix else child.name()
                groups.append(
                    {
                        "name": child.name(),
                        "path": path,
                        "visible": visible,
                    }
                )
                group_item = {
                    "node_type": "group",
                    "name": child.name(),
                    "path": path,
                    "visible": visible,
                    "children": walk(child, path),
                }
                children_payload.append(group_item)
                continue
            if not isinstance(child, QgsLayerTreeLayer):
                continue
            layer = child.layer()
            if layer is None:
                continue
            item = layer_payload(layer)
            layers.append(item)
            children_payload.append({"node_type": "layer", **item})
        return children_payload

    tree = {
        "node_type": "group",
        "name": "",
        "path": "",
        "visible": True,
        "children": walk(root, "") if root is not None else [],
    }
    return {"tree": tree, "groups": groups, "layers": layers}

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

TOOL_METHODS: dict[str, object] = {
    'layer_get_panel_tree': layer_get_panel_tree,
    '_layer_get_panel_tree_impl': _layer_get_panel_tree_impl,
    '_is_layer_visible': _is_layer_visible,
    '_layer_type_token': _layer_type_token,
}
