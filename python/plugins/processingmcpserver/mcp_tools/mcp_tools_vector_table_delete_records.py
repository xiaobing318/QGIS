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

TOOL_NAME = 'vector_table_delete_records'
TOOL_DOC = '按条件删除矢量图层中的部分记录。 layer_ref 指向矢量图层，where 可选筛选表达式，in_place 控制是否直接改源图层，confirm_destructive 必须明确确认删除行为。 目标图层必须存在；若提供 where，表达式必须有效。 会删除匹配记录；默认副本模式下删除的是副本图层中的记录。 默认 in_place=false；删除类操作还要求 confirm_destructive=true，避免误删原图层或记录。 返回 summary.mode、affected_count、output_layer_id 和删除后的 feature_count。'

def vector_table_delete_records(self, layer_ref: str, where: str | None = None, in_place: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
    """Handle records from a vector table."""
    return self._run(self._vector_table_delete_records_impl, layer_ref, where, in_place, confirm_destructive)

def _vector_table_delete_records_impl(self, layer_ref: str, where: str | None, in_place: bool, confirm_destructive: bool) -> dict[str, Any]:
    """Build the records from a vector table."""
    if not confirm_destructive:
        raise Exception("confirm_destructive must be true for delete operation")
    source_layer = self._resolve_vector_layer_ref(layer_ref)
    layer, mode = self._prepare_vector_target_layer(
        source_layer, in_place, "delete_records_copy"
    )
    where_expr = None
    if where:
        where_expr = QgsExpression(where)
        if where_expr.hasParserError():
            raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

    def op() -> int:
        """Handle op."""
        context = QgsExpressionContext()
        context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
        fids: list[int] = []
        for feature in layer.getFeatures():
            if where_expr is not None:
                context.setFeature(feature)
                keep = where_expr.evaluate(context)
                if where_expr.hasEvalError():
                    raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
                if not bool(keep):
                    continue
            fids.append(int(feature.id()))
        if not fids:
            return 0
        if not layer.deleteFeatures(fids):
            raise Exception("Failed to delete records")
        return len(fids)

    affected = self._apply_vector_edit(layer, op)
    return self._ok_result(
        "vector_table_delete_records",
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
    'vector_table_delete_records': vector_table_delete_records,
    '_vector_table_delete_records_impl': _vector_table_delete_records_impl,
    '_apply_vector_edit': _apply_vector_edit,
    '_ok_result': _ok_result,
    '_prepare_vector_target_layer': _prepare_vector_target_layer,
    '_resolve_vector_layer_ref': _resolve_vector_layer_ref,
    '_begin_vector_edit': _begin_vector_edit,
    '_finish_vector_edit': _finish_vector_edit,
    '_materialize_vector_layer': _materialize_vector_layer,
    '_resolve_layer_ref': _resolve_layer_ref,
    '_copy_layer_name': _copy_layer_name,
}
