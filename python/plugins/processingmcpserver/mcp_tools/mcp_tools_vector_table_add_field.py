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

TOOL_NAME = 'vector_table_add_field'
TOOL_DOC = '为矢量图层增加一个新字段。 layer_ref 指向矢量图层，field_name 是新字段名，field_type、field_length、field_precision 用于定义字段类型，in_place 控制是否直接改源图层。 目标图层必须存在，且 field_name 不能为空且不能与现有字段重名。 会修改图层字段结构；默认副本模式下先复制图层再修改。 默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 返回 summary.mode、affected_count、output_layer_id 以及最新字段列表。'

def vector_table_add_field(self, layer_ref: str, field_name: str, field_type: str = "string", field_length: int = 0, field_precision: int = 0, in_place: bool = False) -> dict[str, Any]:
    """
    作用：处理 `vector_table_add_field` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `vector_table_add_field` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `field_name`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `field_type`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"string"`。
    - 参数 `field_length`（`int`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `0`。
    - 参数 `field_precision`（`int`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `0`。
    - 参数 `in_place`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `False`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._vector_table_add_field_impl, layer_ref, field_name, field_type, field_length, field_precision, in_place)

def _vector_table_add_field_impl(self, layer_ref: str, field_name: str, field_type: str, field_length: int, field_precision: int, in_place: bool) -> dict[str, Any]:
    """
    作用：实现 `_vector_table_add_field_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_vector_table_add_field_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer_ref`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `field_name`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `field_type`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `field_length`（`int`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `field_precision`（`int`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `in_place`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    source_layer = self._resolve_vector_layer_ref(layer_ref)
    layer, mode = self._prepare_vector_target_layer(
        source_layer, in_place, "add_field_copy"
    )
    name = field_name.strip()
    if not name:
        raise Exception("field_name is required")
    if self._field_index(layer, name) >= 0:
        raise Exception(f"Field already exists: {name}")
    field = QgsField(name, self._to_field_type(field_type), len=max(0, self._safe_int(field_length, 0)), prec=max(0, self._safe_int(field_precision, 0)))

    def op() -> int:
        """
        作用：处理 `op` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `op` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
        参数与返回：
        - 参数：无。
        - 返回：返回 `int` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `int` 类型结果，返回值语义遵循该函数实现约定。
        异常：可能显式抛出 `Exception`。
        """
        if not layer.addAttribute(field):
            raise Exception(f"Failed to add field: {name}")
        layer.updateFields()
        return 1

    affected = self._apply_vector_edit(layer, op)
    return self._ok_result(
        "vector_table_add_field",
        summary={
            "mode": mode,
            "affected_count": affected,
            "output_layer_id": layer.id(),
        },
        outputs={"fields": [f.name() for f in layer.fields()]},
    )

def _apply_vector_edit(self, layer: QgsVectorLayer, operation) -> Any:
    """
    作用：封装内部辅助步骤 `_apply_vector_edit`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_apply_vector_edit`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer`（`QgsVectorLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `operation`：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
    异常：在异常处理分支中会透传当前异常。
    """
    started_here = self._begin_vector_edit(layer)
    try:
        result = operation()
    except Exception:
        if started_here:
            layer.rollBack()
        raise
    self._finish_vector_edit(layer, started_here)
    return result

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

def _prepare_vector_target_layer(
    self, layer: QgsVectorLayer, in_place: bool, suffix: str
) -> tuple[QgsVectorLayer, str]:
    """
    作用：封装内部辅助步骤 `_prepare_vector_target_layer`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_prepare_vector_target_layer`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer`（`QgsVectorLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `in_place`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `suffix`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `tuple[QgsVectorLayer, str]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[QgsVectorLayer, str]` 类型结果，返回值语义遵循该函数实现约定。
    """
    if in_place:
        return layer, "in_place"
    return self._materialize_vector_layer(layer, suffix), "copy"

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
def _to_field_type(field_type: str) -> QVariant.Type:
    """
    作用：封装内部辅助步骤 `_to_field_type`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_to_field_type`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `field_type`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `QVariant.Type` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `QVariant.Type` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    value = (field_type or "string").strip().lower()
    mapping = {
        "string": QVariant.String,
        "str": QVariant.String,
        "int": QVariant.Int,
        "integer": QVariant.Int,
        "double": QVariant.Double,
        "float": QVariant.Double,
        "real": QVariant.Double,
        "bool": QVariant.Bool,
        "boolean": QVariant.Bool,
    }
    if value not in mapping:
        raise Exception(f"Unsupported field_type: {field_type}")
    return mapping[value]

@staticmethod
def _begin_vector_edit(layer: QgsVectorLayer) -> bool:
    """
    作用：封装内部辅助步骤 `_begin_vector_edit`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_begin_vector_edit`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `layer`（`QgsVectorLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    if layer.isEditable():
        return False
    if not layer.startEditing():
        raise Exception("Failed to start editing vector layer")
    return True

@staticmethod
def _finish_vector_edit(layer: QgsVectorLayer, started_here: bool) -> None:
    """
    作用：封装内部辅助步骤 `_finish_vector_edit`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_finish_vector_edit`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `layer`（`QgsVectorLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `started_here`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：无返回值。
    返回结果：无返回值。
    异常：可能显式抛出 `Exception`。
    """
    if not started_here:
        return
    if layer.commitChanges():
        return
    errors = "; ".join(layer.commitErrors()) if hasattr(layer, "commitErrors") else ""
    layer.rollBack()
    raise Exception(f"Failed to commit vector layer changes: {errors}")

def _materialize_vector_layer(
    self, layer: QgsVectorLayer, suffix: str = "copy"
) -> QgsVectorLayer:
    """
    作用：封装内部辅助步骤 `_materialize_vector_layer`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_materialize_vector_layer`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer`（`QgsVectorLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `suffix`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"copy"`。
    - 返回：返回 `QgsVectorLayer` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `QgsVectorLayer` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    cloned_layer = layer.materialize(QgsFeatureRequest())
    if cloned_layer is None or not cloned_layer.isValid():
        raise Exception(f"Failed to materialize vector layer copy: {layer.id()}")
    cloned_layer.setName(self._copy_layer_name(layer, suffix))
    QgsProject.instance().addMapLayer(cloned_layer)
    return cloned_layer

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

@staticmethod
def _copy_layer_name(layer: QgsMapLayer, suffix: str = "copy") -> str:
    """
    作用：封装内部辅助步骤 `_copy_layer_name`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_copy_layer_name`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `layer`（`QgsMapLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `suffix`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"copy"`。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    base_name = (layer.name() or "layer").strip() or "layer"
    clean_suffix = suffix.strip().replace(" ", "_") if suffix else "copy"
    return f"{base_name}_{clean_suffix}"

TOOL_METHODS: dict[str, object] = {
    'vector_table_add_field': vector_table_add_field,
    '_vector_table_add_field_impl': _vector_table_add_field_impl,
    '_apply_vector_edit': _apply_vector_edit,
    '_field_index': _field_index,
    '_ok_result': _ok_result,
    '_prepare_vector_target_layer': _prepare_vector_target_layer,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_safe_int': _safe_int,
    '_to_field_type': _to_field_type,
    '_begin_vector_edit': _begin_vector_edit,
    '_finish_vector_edit': _finish_vector_edit,
    '_materialize_vector_layer': _materialize_vector_layer,
    '_resolve_layer_ref': _resolve_layer_ref,
    '_copy_layer_name': _copy_layer_name,
}
