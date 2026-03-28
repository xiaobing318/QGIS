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

TOOL_NAME = 'processing_get_parameter_template'
TOOL_DOC = '把某个 Processing 算法的输入参数和输出定义整理成适合模型填写的模板。 algorithm_id 是目标算法 id。 Processing 运行时必须可用，且算法必须存在。 无写操作，只读取算法参数定义和输出定义。 无。 返回 algorithm 基本信息、required_parameters、optional_parameters 和 outputs。'

def processing_get_parameter_template(self, algorithm_id: str) -> dict[str, Any]:
    """
    作用：处理 `processing_get_parameter_template` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `processing_get_parameter_template` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `algorithm_id`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._processing_get_parameter_template_impl, algorithm_id)

def _processing_get_parameter_template_impl(self, algorithm_id: str) -> dict[str, Any]:
    """
    作用：实现 `_processing_get_parameter_template_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_processing_get_parameter_template_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `algorithm_id`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    self._ensure_processing_runtime()
    algorithm = QgsApplication.processingRegistry().algorithmById(algorithm_id)
    if algorithm is None:
        raise Exception(f"Algorithm not found: {algorithm_id}")
    required: list[dict[str, Any]] = []
    optional: list[dict[str, Any]] = []
    for parameter in algorithm.parameterDefinitions():
        item = self._serialize_parameter(parameter)
        (optional if item.get("optional") else required).append(item)
    return {
        "algorithm": {"id": algorithm.id(), "name": algorithm.displayName()},
        "required_parameters": required,
        "optional_parameters": optional,
        "outputs": [self._serialize_output(o) for o in algorithm.outputDefinitions()],
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

def _serialize_output(self, output_def) -> dict[str, Any]:
    """
    作用：封装内部辅助步骤 `_serialize_output`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_serialize_output`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `output_def`：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return {
        "name": self._safe_call(output_def, "name"),
        "description": self._safe_call(output_def, "description"),
        "type": self._safe_call(output_def, "type"),
        "type_name": self._safe_call(output_def, "typeName"),
    }

def _serialize_parameter(self, param) -> dict[str, Any]:
    """
    作用：封装内部辅助步骤 `_serialize_parameter`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_serialize_parameter`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `param`：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    flags = self._safe_call(param, "flags", 0) or 0
    result = {
        "name": self._safe_call(param, "name"),
        "description": self._safe_call(param, "description"),
        "type": self._safe_call(param, "type"),
        "type_name": self._safe_call(param, "typeName"),
        "default": self._serialize_value(self._safe_call(param, "defaultValue")),
        "optional": bool(flags & QgsProcessingParameterDefinition.Flag.FlagOptional),
        "hidden": bool(flags & QgsProcessingParameterDefinition.Flag.FlagHidden),
    }
    if isinstance(param, QgsProcessingParameterEnum):
        result["options"] = list(param.options())
    if hasattr(param, "allowMultiple"):
        result["allow_multiple"] = bool(self._safe_call(param, "allowMultiple", False))
    return result

@staticmethod
def _safe_call(obj: object, name: str, default: Any = None) -> Any:
    """
    作用：封装内部辅助步骤 `_safe_call`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_safe_call`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `obj`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `default`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 返回：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
    """
    attr = getattr(obj, name, None)
    if callable(attr):
        try:
            return attr()
        except Exception:
            return default
    return attr if attr is not None else default

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

TOOL_METHODS: dict[str, object] = {
    'processing_get_parameter_template': processing_get_parameter_template,
    '_processing_get_parameter_template_impl': _processing_get_parameter_template_impl,
    '_ensure_processing_runtime': _ensure_processing_runtime,
    '_serialize_output': _serialize_output,
    '_serialize_parameter': _serialize_parameter,
    '_safe_call': _safe_call,
    '_serialize_value': _serialize_value,
}
