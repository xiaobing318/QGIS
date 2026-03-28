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

TOOL_NAME = 'raster_stats_cell'
TOOL_DOC = '对多个栅格做逐像元统计运算，例如逐像元均值、最小值或最大值。 raster_layer_refs 是栅格引用数组，statistic 选择逐像元统计类型，ignore_nodata 控制是否忽略 NoData。 至少提供一个有效栅格图层，且各栅格最好在分辨率、范围和 CRS 上可兼容处理。 会运行 Processing 栅格统计算法，通常产生新的输出栅格结果；是否自动加载取决于算法默认行为和 Processing 设置。 无 destructive 开关，但会触发 Processing 执行。 返回算法摘要和序列化后的 Processing 输出结果。'

def raster_stats_cell(self, raster_layer_refs: list[str], statistic: int = 0, ignore_nodata: bool = True) -> dict[str, Any]:
    """
    作用：处理 `raster_stats_cell` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `raster_stats_cell` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `raster_layer_refs`（`list[str]`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `statistic`（`int`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `0`。
    - 参数 `ignore_nodata`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `True`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._raster_stats_cell_impl, raster_layer_refs, statistic, ignore_nodata)

def _raster_stats_cell_impl(self, raster_layer_refs: list[str], statistic: int, ignore_nodata: bool) -> dict[str, Any]:
    """
    作用：实现 `_raster_stats_cell_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_raster_stats_cell_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `raster_layer_refs`（`list[str]`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `statistic`（`int`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `ignore_nodata`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    if not raster_layer_refs:
        raise Exception("raster_layer_refs is required")
    layers = [self._resolve_raster_layer_ref(ref) for ref in raster_layer_refs]
    self._ensure_processing_runtime()
    result = processing.run(
        "native:cellstatistics",
        {
            "INPUT": layers,
            "STATISTIC": self._safe_int(statistic, 0),
            "IGNORE_NODATA": bool(ignore_nodata),
            "REFERENCE_LAYER": layers[0],
            "OUTPUT_NODATA_VALUE": -9999,
            "OUTPUT": "TEMPORARY_OUTPUT",
        },
    )
    return self._ok_result("raster_stats_cell", summary={"input_count": len(layers), "output": self._coerce_output_identifier(result.get("OUTPUT"))}, outputs={"result": self._serialize_value(result)})

@staticmethod
def _coerce_output_identifier(value: Any) -> str:
    """
    作用：封装内部辅助步骤 `_coerce_output_identifier`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_coerce_output_identifier`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `value`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    if value is None:
        return ""
    if isinstance(value, str):
        return value
    if hasattr(value, "id") and callable(value.id):
        try:
            return str(value.id())
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

def _resolve_raster_layer_ref(self, layer_ref: Any) -> QgsRasterLayer:
    """
    作用：封装内部辅助步骤 `_resolve_raster_layer_ref`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_resolve_raster_layer_ref`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`Any`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `QgsRasterLayer` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `QgsRasterLayer` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.RasterLayer:
        raise Exception(f"Layer is not a raster layer: {layer_ref}")
    return layer

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
    'raster_stats_cell': raster_stats_cell,
    '_raster_stats_cell_impl': _raster_stats_cell_impl,
    '_coerce_output_identifier': _coerce_output_identifier,
    '_ensure_processing_runtime': _ensure_processing_runtime,
    '_ok_result': _ok_result,
    '_resolve_raster_layer_ref': _resolve_raster_layer_ref,
    '_safe_int': _safe_int,
    '_serialize_value': _serialize_value,
    '_resolve_layer_ref': _resolve_layer_ref,
}
