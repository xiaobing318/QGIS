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

TOOL_NAME = 'vector_add_layers'
TOOL_DOC = '批量把多个矢量数据源加载到当前 QGIS 工程。 paths 必须是 list[str]，provider 默认 ogr，skip_invalid 控制遇到坏数据时是跳过还是整体失败。 至少提供一个可访问路径，且对应数据源应能被 provider 识别。 会向当前工程新增多个矢量图层，但不会改写源数据文件。 skip_invalid=true 时会尽量继续加载其余路径；skip_invalid=false 时任何一个失败都会报错终止。 返回 requested_count、loaded_count、failed_count，以及 loaded 和 failed 的逐项结果。'

def vector_add_layers(self, paths: list[str], provider: str = "ogr", skip_invalid: bool = True) -> dict[str, Any]:
    """
    作用：处理 `vector_add_layers` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `vector_add_layers` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `paths`（`list[str]`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `provider`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"ogr"`。
    - 参数 `skip_invalid`（`bool`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `True`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._vector_add_layers_impl, paths, provider, skip_invalid)

def _vector_add_layers_impl(self, paths: list[str], provider: str, skip_invalid: bool) -> dict[str, Any]:
    """
    作用：实现 `_vector_add_layers_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_vector_add_layers_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `paths`（`list[str]`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `provider`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `skip_invalid`（`bool`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    if not isinstance(paths, list):
        raise Exception("paths must be a list[str]")
    if any(not isinstance(path, str) for path in paths):
        raise Exception("paths must be a list[str]")

    loaded: list[dict[str, Any]] = []
    failed: list[dict[str, Any]] = []
    for path in paths:
        try:
            loaded.append(self._vector_add_layer_impl(path, provider=provider))
        except Exception as exc:
            if not skip_invalid:
                raise Exception(f"Failed to load vector dataset: {path}. {exc}") from exc
            failed.append({"path": path, "error": str(exc)})
    return {"requested_count": len(paths), "loaded_count": len(loaded), "failed_count": len(failed), "loaded": loaded, "failed": failed}

def _vector_add_layer_impl(self, path: str, provider: str = "ogr", name: str | None = None) -> dict[str, Any]:
    """
    作用：实现 `_vector_add_layer_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_vector_add_layer_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `provider`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"ogr"`。
    - 参数 `name`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `None`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    layer = QgsVectorLayer(path, name or Path(path).stem, provider)
    if not layer.isValid():
        raise Exception(f"Layer is not valid: {path}")
    QgsProject.instance().addMapLayer(layer)
    return {"id": layer.id(), "name": layer.name(), "type": self._layer_type_token(layer), "feature_count": layer.featureCount()}

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
    'vector_add_layers': vector_add_layers,
    '_vector_add_layers_impl': _vector_add_layers_impl,
    '_vector_add_layer_impl': _vector_add_layer_impl,
    '_layer_type_token': _layer_type_token,
}
