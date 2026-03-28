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
    global _PROCESSING_INITIALIZED
    if _PROCESSING_INITIALIZED:
        return
    Processing.initialize()
    _PROCESSING_INITIALIZED = True

TOOL_NAME = 'vector_table_calculate_field'
TOOL_DOC = '???按 QGIS 表达式批量计算字段值，可在字段不存在时自动创建字段。 ?????layer_ref 指向矢量图层，field_name 是目标字段，expression 是赋值表达式，where 可选筛选条件，field_type 只在自动建字段时生效，in_place 控制是否直接改源图层。 ?????目标图层必须存在，expression 和 where 若提供都必须能被 QGIS 表达式解析并成功求值。 ??????会更新匹配要素的属性值；字段不存在时会先新增该字段。 ?????默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 ?????返回 summary.mode、affected_count、field_created、output_layer_id 和更新后的字段列表。'

def vector_table_calculate_field(self, layer_ref: str, field_name: str, expression: str, field_type: str = "string", where: str | None = None, in_place: bool = False) -> dict[str, Any]:
    """Handle a vector table field."""
    return self._run(self._vector_table_calculate_field_impl, layer_ref, field_name, expression, field_type, where, in_place)

def _vector_table_calculate_field_impl(self, layer_ref: str, field_name: str, expression: str, field_type: str, where: str | None, in_place: bool) -> dict[str, Any]:
    """Build the vector table field."""
    source_layer = self._resolve_vector_layer_ref(layer_ref)
    layer, mode = self._prepare_vector_target_layer(
        source_layer, in_place, "calculate_field_copy"
    )
    expr = QgsExpression(expression or "")
    if expr.hasParserError():
        raise Exception(f"Invalid expression: {expr.parserErrorString()}")
    where_expr = None
    if where:
        where_expr = QgsExpression(where)
        if where_expr.hasParserError():
            raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

    idx = self._field_index(layer, field_name)
    created = False

    def op() -> int:
        """Handle op."""
        nonlocal idx
        nonlocal created
        if idx < 0:
            if not layer.addAttribute(QgsField(field_name, self._to_field_type(field_type))):
                raise Exception(f"Failed to create field: {field_name}")
            layer.updateFields()
            idx = self._field_index(layer, field_name)
            created = True
        context = QgsExpressionContext()
        context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
        affected = 0
        for feature in layer.getFeatures():
            context.setFeature(feature)
            if where_expr is not None:
                keep = where_expr.evaluate(context)
                if where_expr.hasEvalError():
                    raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
                if not bool(keep):
                    continue
            value = expr.evaluate(context)
            if expr.hasEvalError():
                raise Exception(f"Invalid expression: {expr.evalErrorString()}")
            if not layer.changeAttributeValue(feature.id(), idx, value):
                raise Exception(f"Failed to update feature: {feature.id()}")
            affected += 1
        return affected

    affected = self._apply_vector_edit(layer, op)
    return self._ok_result(
        "vector_table_calculate_field",
        summary={
            "mode": mode,
            "affected_count": affected,
            "field_created": created,
            "output_layer_id": layer.id(),
        },
        outputs={"fields": [f.name() for f in layer.fields()]},
    )

def _apply_vector_edit(self, layer: QgsVectorLayer, operation) -> Any:
    """Handle apply vector edit."""
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
    """Handle field index."""
    return layer.fields().indexFromName(field_name)

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """Handle ok result."""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _prepare_vector_target_layer(
    self, layer: QgsVectorLayer, in_place: bool, suffix: str
) -> tuple[QgsVectorLayer, str]:
    """Prepare prepare vector target layer."""
    if in_place:
        return layer, "in_place"
    return self._materialize_vector_layer(layer, suffix), "copy"

def _resolve_vector_layer_ref(self, layer_ref: Any) -> QgsVectorLayer:
    """Resolve vector layer ref."""
    layer = self._resolve_layer_ref(layer_ref)
    if layer.type() != QgsMapLayer.VectorLayer:
        raise Exception(f"Layer is not a vector layer: {layer_ref}")
    return layer

@staticmethod
def _to_field_type(field_type: str) -> QVariant.Type:
    """Handle to field type."""
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
    """Handle begin vector edit."""
    if layer.isEditable():
        return False
    if not layer.startEditing():
        raise Exception("Failed to start editing vector layer")
    return True

@staticmethod
def _finish_vector_edit(layer: QgsVectorLayer, started_here: bool) -> None:
    """Handle finish vector edit."""
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
    """Handle materialize vector layer."""
    cloned_layer = layer.materialize(QgsFeatureRequest())
    if cloned_layer is None or not cloned_layer.isValid():
        raise Exception(f"Failed to materialize vector layer copy: {layer.id()}")
    cloned_layer.setName(self._copy_layer_name(layer, suffix))
    QgsProject.instance().addMapLayer(cloned_layer)
    return cloned_layer

def _resolve_layer_ref(self, layer_ref: Any) -> QgsMapLayer:
    """Resolve layer ref."""
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
    """Handle copy layer name."""
    base_name = (layer.name() or "layer").strip() or "layer"
    clean_suffix = suffix.strip().replace(" ", "_") if suffix else "copy"
    return f"{base_name}_{clean_suffix}"

TOOL_METHODS: dict[str, object] = {
    'vector_table_calculate_field': vector_table_calculate_field,
    '_vector_table_calculate_field_impl': _vector_table_calculate_field_impl,
    '_apply_vector_edit': _apply_vector_edit,
    '_field_index': _field_index,
    '_ok_result': _ok_result,
    '_prepare_vector_target_layer': _prepare_vector_target_layer,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_to_field_type': _to_field_type,
    '_begin_vector_edit': _begin_vector_edit,
    '_finish_vector_edit': _finish_vector_edit,
    '_materialize_vector_layer': _materialize_vector_layer,
    '_resolve_layer_ref': _resolve_layer_ref,
    '_copy_layer_name': _copy_layer_name,
}
