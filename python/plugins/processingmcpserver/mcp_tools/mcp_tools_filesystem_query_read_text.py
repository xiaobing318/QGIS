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

TOOL_NAME = 'filesystem_query_read_text'
TOOL_DOC = '???按 UTF-8 读取文本文件内容，适合让模型读取配置、脚本或日志片段。 ?????path 指向文本文件，max_chars 可选，用于限制返回字符数。 ?????目标路径必须存在且是文件，并且内容应能按 UTF-8 解码。 ??????无写操作，只读取文件内容。 ?????max_chars 为 None 时返回全文；传入数值时只读取 max_chars+1 个字符用于判断截断，并在 summary.truncated 中标记。 ?????返回 text 字段和截断摘要。'

def filesystem_query_read_text(self, path: str, max_chars: int | None = None) -> dict[str, Any]:
    """Handle text from a filesystem entry."""
    return self._run(self._filesystem_query_read_text_impl, path, max_chars)

def _filesystem_query_read_text_impl(self, path: str, max_chars: int | None) -> dict[str, Any]:
    """Build the text from a filesystem entry."""
    entry = self._resolve_filesystem_query_path(path)
    if not entry.exists() or not entry.is_file():
        raise Exception(f"File not found: {path}")
    if max_chars is None:
        text = entry.read_text(encoding="utf-8")
        return self._ok_result("filesystem_query_read_text", summary={"truncated": False, "max_chars": None}, outputs={"text": text})
    applied = max(0, self._safe_int(max_chars, 0))
    with entry.open("r", encoding="utf-8") as handle:
        text = handle.read(applied + 1)
    return self._ok_result("filesystem_query_read_text", summary={"truncated": len(text) > applied, "max_chars": applied}, outputs={"text": text[:applied]})

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """Handle ok result."""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _resolve_filesystem_query_path(self, path: str | Path) -> Path:
    """Resolve filesystem query path."""
    return self._normalize_filesystem_path(path)

@staticmethod
def _safe_int(value: Any, default: int) -> int:
    """Handle safe int."""
    try:
        return int(value)
    except (TypeError, ValueError):
        return default

@staticmethod
def _normalize_filesystem_path(path: str | Path) -> Path:
    """Handle normalize filesystem path."""
    candidate = path if isinstance(path, Path) else Path(str(path).strip()).expanduser()
    if not candidate.is_absolute():
        candidate = Path.cwd() / candidate
    return candidate.resolve(strict=False)

TOOL_METHODS: dict[str, object] = {
    'filesystem_query_read_text': filesystem_query_read_text,
    '_filesystem_query_read_text_impl': _filesystem_query_read_text_impl,
    '_ok_result': _ok_result,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_safe_int': _safe_int,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
