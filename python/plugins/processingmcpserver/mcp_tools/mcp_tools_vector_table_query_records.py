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

TOOL_NAME = 'vector_table_query_records'
TOOL_DOC = '按条件查询矢量图层记录，适合做数据抽样、分页和字段级检查。 layer_ref 指向矢量图层，where 是可选过滤表达式，fields 控制返回字段，limit 和 offset 控制分页，order_by 指定简单排序字段，include_geometry 控制是否附带 geometry_wkt。 目标图层必须存在，where 表达式和字段名必须有效。 无写操作，只读取并序列化匹配记录。 limit 会被内部阈值裁剪；order_by 仅按属性字符串表示做简单排序，不是完整 SQL 排序。 返回 matched_total、returned、offset、limit 应用结果和 records 数组。'

def vector_table_query_records(self, layer_ref: str, where: str | None = None, fields: list[str] | None = None, limit: int = DEFAULT_FEATURE_LIMIT, offset: int = 0, order_by: str | None = None, include_geometry: bool = False) -> dict[str, Any]:
    """
    作用：处理 `vector_table_query_records` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `vector_table_query_records` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `where`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `fields`（`list[str] | None`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `None`。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。 默认值为 `DEFAULT_FEATURE_LIMIT`。
    - 参数 `offset`（`int`）：数值控制参数，用于限制范围、数量或时限。 默认值为 `0`。
    - 参数 `order_by`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `include_geometry`（`bool`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `False`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._vector_table_query_records_impl, layer_ref, where, fields, limit, offset, order_by, include_geometry)

def _vector_table_query_records_impl(self, layer_ref: str, where: str | None, fields: list[str] | None, limit: int, offset: int, order_by: str | None, include_geometry: bool) -> dict[str, Any]:
    """
    作用：实现 `_vector_table_query_records_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_vector_table_query_records_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `where`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `fields`（`list[str] | None`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。
    - 参数 `offset`（`int`）：数值控制参数，用于限制范围、数量或时限。
    - 参数 `order_by`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `include_geometry`（`bool`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    if not str(layer_ref).strip():
        raise Exception("layer_ref is required")
    layer = self._resolve_vector_layer_ref(layer_ref)
    where_expr = None
    if where:
        where_expr = QgsExpression(where)
        if where_expr.hasParserError():
            raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

    requested, applied, capped = self._normalize_feature_limit(limit)
    applied_offset = max(0, self._safe_int(offset, 0))
    all_fields = [f.name() for f in layer.fields()]
    selected_fields = all_fields if not fields else fields
    for field_name in selected_fields:
        if self._field_index(layer, field_name) < 0:
            raise Exception(f"Field not found: {field_name}")

    context = QgsExpressionContext()
    context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
    matched: list[QgsFeature] = []
    for feature in layer.getFeatures():
        if where_expr is not None:
            context.setFeature(feature)
            keep = where_expr.evaluate(context)
            if where_expr.hasEvalError():
                raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
            if not bool(keep):
                continue
        matched.append(feature)

    if order_by:
        matched = sorted(matched, key=lambda f: str(f.attribute(order_by)))
    sliced = matched[applied_offset : applied_offset + applied]
    records: list[dict[str, Any]] = []
    for feature in sliced:
        item = {
            "id": feature.id(),
            "attributes": {
                name: self._serialize_value(feature.attribute(name))
                for name in selected_fields
            },
        }
        if include_geometry and feature.hasGeometry():
            item["geometry_wkt"] = feature.geometry().asWkt(precision=6)
        records.append(item)

    return self._ok_result("vector_table_query_records", summary={"returned": len(records), "matched_total": len(matched), "offset": applied_offset, "requested_limit": requested, "applied_limit": applied, "limit_capped": capped}, outputs={"records": records})

@staticmethod
def _field_index(layer: QgsVectorLayer, field_name: str) -> int:
    """
    作用：封装内部辅助步骤 `_field_index`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_field_index`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `layer`（`QgsVectorLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `field_name`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `int` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `int` 类型结果，返回值语义遵循该函数实现约定。
    """
    return layer.fields().indexFromName(field_name)

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
    'vector_table_query_records': vector_table_query_records,
    '_vector_table_query_records_impl': _vector_table_query_records_impl,
    '_field_index': _field_index,
    '_normalize_feature_limit': _normalize_feature_limit,
    '_ok_result': _ok_result,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_safe_int': _safe_int,
    '_serialize_value': _serialize_value,
    '_resolve_layer_ref': _resolve_layer_ref,
}
