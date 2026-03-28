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

TOOL_NAME = 'filesystem_edit_append_text'
TOOL_DOC = '向文本文件尾部追加 UTF-8 内容。 path 是目标文件路径，content 是待追加文本，create_parents 控制父目录不存在时是否自动创建，confirm_write 用于显式确认写操作。 目标路径的父目录必须存在或允许自动创建；目标文件不存在时会被创建。 会在磁盘上创建文件或修改现有文件末尾内容。 该工具不会覆盖已有内容，但仍属于写盘操作，调用前应确认目标路径。 返回追加字符数和最终 path 摘要。'

def filesystem_edit_append_text(self, path: str, content: str, create_parents: bool = True, confirm_write: bool = False) -> dict[str, Any]:
    """Handle text to a filesystem entry."""
    return self._run(self._filesystem_edit_append_text_impl, path, content, create_parents, confirm_write)

def _filesystem_edit_append_text_impl(self, path: str, content: str, create_parents: bool, confirm_write: bool) -> dict[str, Any]:
    """Build the text to a filesystem entry."""
    self._ensure_filesystem_write_confirmed(confirm_write)
    target = self._resolve_filesystem_write_path(path)
    if not target.parent.exists():
        if create_parents:
            target.parent.mkdir(parents=True, exist_ok=True)
        else:
            raise Exception(f"Parent directory not found: {target.parent}")
    with target.open("a", encoding="utf-8") as handle:
        handle.write(content)
    return self._ok_result("filesystem_edit_append_text", summary={"appended_chars": len(content), "path": str(target)})

@staticmethod
def _ensure_filesystem_write_confirmed(confirm_write: bool) -> None:
    """Handle ensure filesystem write confirmed."""
    if not confirm_write:
        raise Exception(
            "confirm_write must be true for filesystem_edit_* operations"
        )

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """Handle ok result."""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _resolve_filesystem_write_path(self, path: str | Path) -> Path:
    """Resolve filesystem write path."""
    return self._resolve_filesystem_query_path(path)

def _resolve_filesystem_query_path(self, path: str | Path) -> Path:
    """Resolve filesystem query path."""
    return self._normalize_filesystem_path(path)

@staticmethod
def _normalize_filesystem_path(path: str | Path) -> Path:
    """Handle normalize filesystem path."""
    candidate = path if isinstance(path, Path) else Path(str(path).strip()).expanduser()
    if not candidate.is_absolute():
        candidate = Path.cwd() / candidate
    return candidate.resolve(strict=False)

TOOL_METHODS: dict[str, object] = {
    'filesystem_edit_append_text': filesystem_edit_append_text,
    '_filesystem_edit_append_text_impl': _filesystem_edit_append_text_impl,
    '_ensure_filesystem_write_confirmed': _ensure_filesystem_write_confirmed,
    '_ok_result': _ok_result,
    '_resolve_filesystem_write_path': _resolve_filesystem_write_path,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
