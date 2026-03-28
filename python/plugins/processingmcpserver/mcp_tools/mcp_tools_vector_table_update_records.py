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

TOOL_NAME = 'vector_table_update_records'
TOOL_DOC = '按条件批量更新矢量图层记录，支持常量赋值和表达式赋值。 layer_ref 指向矢量图层，where 是可选筛选表达式，set_literals 传常量值映射，set_expressions 传表达式映射，in_place 控制是否直接改源图层。 目标图层必须存在，至少提供 set_literals 或 set_expressions 之一，涉及字段和表达式都必须有效。 会更新匹配要素属性值；默认副本模式下生成新图层后再更新。 默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 返回 summary.mode、affected_count、output_layer_id 和更新后 feature_count。'

def vector_table_update_records(self, layer_ref: str, where: str | None = None, set_literals: dict[str, Any] | None = None, set_expressions: dict[str, str] | None = None, in_place: bool = False) -> dict[str, Any]:
    """Handle records in a vector table."""
    return self._run(self._vector_table_update_records_impl, layer_ref, where, set_literals, set_expressions, in_place)

def _vector_table_update_records_impl(self, layer_ref: str, where: str | None, set_literals: dict[str, Any] | None, set_expressions: dict[str, str] | None, in_place: bool) -> dict[str, Any]:
    """Build the records in a vector table."""
    source_layer = self._resolve_vector_layer_ref(layer_ref)
    layer, mode = self._prepare_vector_target_layer(
        source_layer, in_place, "update_records_copy"
    )
    literals = set_literals or {}
    expressions = set_expressions or {}
    if not literals and not expressions:
        raise Exception("Either set_literals or set_expressions must be provided")

    where_expr = None
    if where:
        where_expr = QgsExpression(where)
        if where_expr.hasParserError():
            raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

    compiled: dict[str, QgsExpression] = {}
    for key, expr_text in expressions.items():
        compiled_expr = QgsExpression(expr_text)
        if compiled_expr.hasParserError():
            raise Exception(f"Invalid expression for {key}: {compiled_expr.parserErrorString()}")
        compiled[key] = compiled_expr

    field_indexes: dict[str, int] = {}
    for key in set(literals.keys()) | set(expressions.keys()):
        idx = self._field_index(layer, key)
        if idx < 0:
            raise Exception(f"Field not found: {key}")
        field_indexes[key] = idx

    def op() -> int:
        """Handle op."""
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
            for key, value in literals.items():
                if not layer.changeAttributeValue(feature.id(), field_indexes[key], value):
                    raise Exception(f"Failed to update field {key}")
            for key, compiled_expr in compiled.items():
                value = compiled_expr.evaluate(context)
                if compiled_expr.hasEvalError():
                    raise Exception(f"Invalid expression for {key}: {compiled_expr.evalErrorString()}")
                if not layer.changeAttributeValue(feature.id(), field_indexes[key], value):
                    raise Exception(f"Failed to update field {key}")
            affected += 1
        return affected

    affected = self._apply_vector_edit(layer, op)
    return self._ok_result(
        "vector_table_update_records",
        summary={
            "mode": mode,
            "affected_count": affected,
            "output_layer_id": layer.id(),
        },
        outputs={"feature_count": layer.featureCount()},
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
    'vector_table_update_records': vector_table_update_records,
    '_vector_table_update_records_impl': _vector_table_update_records_impl,
    '_apply_vector_edit': _apply_vector_edit,
    '_field_index': _field_index,
    '_ok_result': _ok_result,
    '_prepare_vector_target_layer': _prepare_vector_target_layer,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_begin_vector_edit': _begin_vector_edit,
    '_finish_vector_edit': _finish_vector_edit,
    '_materialize_vector_layer': _materialize_vector_layer,
    '_resolve_layer_ref': _resolve_layer_ref,
    '_copy_layer_name': _copy_layer_name,
}
