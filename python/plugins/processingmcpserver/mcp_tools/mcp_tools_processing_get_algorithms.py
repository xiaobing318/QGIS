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

TOOL_NAME = 'processing_get_algorithms'
TOOL_DOC = '查询 Processing 算法目录，支持按 provider 过滤，或按 algorithm_id 精确读取单个算法。 algorithm_id 提供时返回单个算法完整定义；provider_id 提供时按 provider 过滤；include_parameters 和 include_outputs 控制列表模式下是否展开参数和输出；limit 控制列表返回上限。 Processing 运行时必须可用；若指定 algorithm_id，则该算法必须存在。 无写操作，只读取 Processing 算法元数据。 列表模式的 limit 会被内部阈值裁剪；单算法模式会始终返回该算法的完整参数和输出定义。 返回单个 algorithm 对象或 algorithms 数组，以及 count、returned、truncated 等摘要。'

def processing_get_algorithms(self, algorithm_id: str | None = None, provider_id: str | None = None, include_parameters: bool = False, include_outputs: bool = False, limit: int | None = None) -> dict[str, Any]:
    """Handle available processing algorithms."""
    return self._run(self._processing_get_algorithms_impl, algorithm_id, provider_id, include_parameters, include_outputs, limit)

def _processing_get_algorithms_impl(self, algorithm_id: str | None, provider_id: str | None, include_parameters: bool, include_outputs: bool, limit: int | None) -> dict[str, Any]:
    """Build the available processing algorithms."""
    self._ensure_processing_runtime()
    registry = QgsApplication.processingRegistry()
    if algorithm_id:
        algorithm = registry.algorithmById(algorithm_id)
        if algorithm is None:
            raise Exception(f"Algorithm not found: {algorithm_id}")
        return {"algorithm": self._serialize_algorithm(algorithm, include_parameters=True, include_outputs=True)}

    algorithms = list(registry.algorithms())
    if provider_id:
        text = provider_id.strip().lower()
        algorithms = [a for a in algorithms if a.provider() and (a.provider().id().lower() == text or a.provider().name().lower() == text)]

    requested, applied, capped = self._normalize_algorithm_list_limit(limit)
    total = len(algorithms)
    if total > applied:
        algorithms = algorithms[:applied]
    return {
        "count": total,
        "returned": len(algorithms),
        "truncated": total > len(algorithms),
        "requested_limit": requested,
        "applied_limit": applied,
        "limit_capped": capped,
        "algorithms": [self._serialize_algorithm(a, include_parameters=include_parameters, include_outputs=include_outputs) for a in algorithms],
    }

def _ensure_processing_runtime(self) -> None:
    """Handle ensure processing runtime."""
    _ensure_processing_initialized()

def _normalize_algorithm_list_limit(self, limit: Any | None) -> tuple[int | None, int, bool]:
    """Handle normalize algorithm list limit."""
    requested = None if limit is None else self._safe_int(limit, self.DEFAULT_ALGORITHM_LIST_LIMIT)
    normalized = self.DEFAULT_ALGORITHM_LIST_LIMIT if requested is None else max(0, requested)
    applied = min(normalized, self.MAX_ALGORITHM_LIST_LIMIT)
    capped = requested is not None and normalized != applied
    return requested, applied, capped

def _serialize_algorithm(self, alg, include_parameters: bool, include_outputs: bool) -> dict[str, Any]:
    """Serialize serialize algorithm."""
    provider = alg.provider() if hasattr(alg, "provider") else None
    result = {
        "id": alg.id(),
        "name": alg.displayName(),
        "group": alg.group(),
        "group_id": alg.groupId(),
        "provider": provider.name() if provider else None,
        "provider_id": provider.id() if provider else None,
        "parameters_count": len(alg.parameterDefinitions()),
        "outputs_count": len(alg.outputDefinitions()),
    }
    if include_parameters:
        result["parameters"] = [self._serialize_parameter(p) for p in alg.parameterDefinitions()]
    if include_outputs:
        result["outputs"] = [self._serialize_output(o) for o in alg.outputDefinitions()]
    return result

@staticmethod
def _safe_int(value: Any, default: int) -> int:
    """Handle safe int."""
    try:
        return int(value)
    except (TypeError, ValueError):
        return default

def _serialize_output(self, output_def) -> dict[str, Any]:
    """Serialize serialize output."""
    return {
        "name": self._safe_call(output_def, "name"),
        "description": self._safe_call(output_def, "description"),
        "type": self._safe_call(output_def, "type"),
        "type_name": self._safe_call(output_def, "typeName"),
    }

def _serialize_parameter(self, param) -> dict[str, Any]:
    """Serialize serialize parameter."""
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
    """Handle safe call."""
    attr = getattr(obj, name, None)
    if callable(attr):
        try:
            return attr()
        except Exception:
            return default
    return attr if attr is not None else default

@staticmethod
def _serialize_value(value: Any) -> Any:
    """Serialize values into JSON-friendly representations."""
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
    'processing_get_algorithms': processing_get_algorithms,
    '_processing_get_algorithms_impl': _processing_get_algorithms_impl,
    '_ensure_processing_runtime': _ensure_processing_runtime,
    '_normalize_algorithm_list_limit': _normalize_algorithm_list_limit,
    '_serialize_algorithm': _serialize_algorithm,
    '_safe_int': _safe_int,
    '_serialize_output': _serialize_output,
    '_serialize_parameter': _serialize_parameter,
    '_safe_call': _safe_call,
    '_serialize_value': _serialize_value,
}
