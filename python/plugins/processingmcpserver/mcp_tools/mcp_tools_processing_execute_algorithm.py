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

TOOL_NAME = 'processing_execute_algorithm'
TOOL_DOC = '执行单次 Processing 算法调用，是通用算法执行入口。 algorithm 是算法 id，parameters 是参数对象，load_results 控制是否把结果加载到当前工程，allow_disk_write 控制是否允许磁盘输出路径通过安全检查，allow_in_place_edit 控制是否允许原位编辑参数通过安全检查。 Processing 运行时必须可用，algorithm 必须存在，parameters 必须是对象。 会触发 Processing 执行，可能生成临时输出、加载新图层，或在显式允许时写盘或原位修改。 默认禁止磁盘写出和原位编辑；只有在明确需要时才把 allow_disk_write 或 allow_in_place_edit 设为 true，并应复核返回里的 safety_policy、warnings 与 effective_parameters。 返回 algorithm、load_results、result、warnings、safety_policy 和 effective_parameters。'

def processing_execute_algorithm(self, algorithm: str, parameters: dict[str, Any], load_results: bool = True, allow_disk_write: bool = False, allow_in_place_edit: bool = False) -> dict[str, Any]:
    """
    作用：处理 `processing_execute_algorithm` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `processing_execute_algorithm` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `algorithm`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `parameters`（`dict[str, Any]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `load_results`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `True`。
    - 参数 `allow_disk_write`（`bool`）：布尔开关参数，用于控制是否启用特定行为。 默认值为 `False`。
    - 参数 `allow_in_place_edit`（`bool`）：布尔开关参数，用于控制是否启用特定行为。 默认值为 `False`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._processing_execute_algorithm_impl, algorithm, parameters, load_results, allow_disk_write, allow_in_place_edit)

def _processing_execute_algorithm_impl(self, algorithm: str, parameters: dict[str, Any], load_results: bool, allow_disk_write: bool, allow_in_place_edit: bool) -> dict[str, Any]:
    """
    作用：实现 `_processing_execute_algorithm_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_processing_execute_algorithm_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `algorithm`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `parameters`（`dict[str, Any]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `load_results`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `allow_disk_write`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 参数 `allow_in_place_edit`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    if not isinstance(parameters, dict):
        raise Exception("parameters must be an object")
    effective, warnings = self._sanitize_processing_parameters(parameters, allow_disk_write, allow_in_place_edit)
    result = self._execute_processing_call(algorithm, effective, load_results)
    return self._ok_result("processing_execute_algorithm", summary={"algorithm": algorithm, "load_results": bool(load_results)}, outputs={"result": self._serialize_value(result)}, warnings=warnings, safety_policy={"allow_disk_write": bool(allow_disk_write), "allow_in_place_edit": bool(allow_in_place_edit)}, effective_parameters=self._serialize_value(effective))

def _execute_processing_call(
    self, algorithm: str, parameters: dict[str, Any], load_results: bool
) -> dict[str, Any]:
    """
    作用：封装内部辅助步骤 `_execute_processing_call`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_execute_processing_call`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `algorithm`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `parameters`（`dict[str, Any]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `load_results`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    self._ensure_processing_runtime()
    result = (
        processing.runAndLoadResults(algorithm, parameters)
        if load_results
        else processing.run(algorithm, parameters)
    )
    return result if isinstance(result, dict) else {"result": result}

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """
    作用：封装内部辅助步骤 `_ok_result`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_ok_result`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `tool`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `summary`（`dict[str, Any] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `outputs`（`dict[str, Any] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `warnings`（`list[str] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `**extra`：可变关键字参数，用于扩展命名输入。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _sanitize_processing_parameters(self, parameters: dict[str, Any], allow_disk_write: bool, allow_in_place_edit: bool) -> tuple[dict[str, Any], list[str]]:
    """
    作用：封装内部辅助步骤 `_sanitize_processing_parameters`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_sanitize_processing_parameters`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `parameters`（`dict[str, Any]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `allow_disk_write`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 参数 `allow_in_place_edit`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 返回：返回 `tuple[dict[str, Any], list[str]]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[dict[str, Any], list[str]]` 类型结果，返回值语义遵循该函数实现约定。
    """
    sanitized = dict(parameters)
    warnings: list[str] = []
    for key in list(sanitized.keys()):
        upper = str(key).upper()
        value = sanitized[key]
        if upper == "IN_PLACE" and bool(value) and not allow_in_place_edit:
            sanitized[key] = False
            warnings.append("IN_PLACE was forced to false by safety policy.")
        if self._is_disk_output_key(str(key)) and not allow_disk_write and self._is_disk_output_value(value):
            sanitized[key] = "TEMPORARY_OUTPUT"
            warnings.append(f"{key} was rewritten to TEMPORARY_OUTPUT by safety policy.")
    return sanitized, warnings

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

@staticmethod
def _is_disk_output_key(key: str) -> bool:
    """
    作用：封装内部辅助步骤 `_is_disk_output_key`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_is_disk_output_key`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `key`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
    """
    return "OUTPUT" in key.upper()

@staticmethod
def _is_disk_output_value(value: Any) -> bool:
    """
    作用：封装内部辅助步骤 `_is_disk_output_value`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_is_disk_output_value`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `value`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
    """
    if value is None:
        return False
    if isinstance(value, Path):
        return True
    if not isinstance(value, str):
        return False
    text = value.strip()
    if not text:
        return False
    if text.upper() in {"TEMPORARY_OUTPUT", "TEMPORARY", "MEMORY:", "MEMORY"}:
        return False
    if text.startswith("memory:"):
        return False
    return True

TOOL_METHODS: dict[str, object] = {
    'processing_execute_algorithm': processing_execute_algorithm,
    '_processing_execute_algorithm_impl': _processing_execute_algorithm_impl,
    '_execute_processing_call': _execute_processing_call,
    '_ok_result': _ok_result,
    '_sanitize_processing_parameters': _sanitize_processing_parameters,
    '_serialize_value': _serialize_value,
    '_ensure_processing_runtime': _ensure_processing_runtime,
    '_is_disk_output_key': _is_disk_output_key,
    '_is_disk_output_value': _is_disk_output_value,
}
