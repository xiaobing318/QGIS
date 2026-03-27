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

TOOL_NAME = 'filesystem_edit_delete_entry'
TOOL_DOC = '???删除单个文件或整个目录树。 ?????path 指向待删除文件或目录，confirm_destructive 必须明确确认删除，confirm_write 用于显式确认写操作。 ?????目标路径必须存在。 ??????会永久删除磁盘上的文件或目录内容。 ?????只有 confirm_write=true 且 confirm_destructive=true 才允许执行删除。 ?????返回 deleted_path 摘要。'

def filesystem_edit_delete_entry(self, path: str, confirm_destructive: bool = False, confirm_write: bool = False) -> dict[str, Any]:
    """执行文件系统相关的 edit delete entry 逻辑。"""
    return self._run(self._filesystem_edit_delete_entry_impl, path, confirm_destructive, confirm_write)

def _filesystem_edit_delete_entry_impl(self, path: str, confirm_destructive: bool, confirm_write: bool) -> dict[str, Any]:
    """执行文件系统相关的 edit delete entry impl 逻辑。"""
    self._ensure_filesystem_write_confirmed(confirm_write)
    if not confirm_destructive:
        raise Exception("confirm_destructive must be true for delete operation")
    target = self._resolve_filesystem_write_path(path)
    if not target.exists():
        raise Exception(f"Path not found: {path}")
    shutil.rmtree(target) if target.is_dir() else target.unlink()
    return self._ok_result("filesystem_edit_delete_entry", summary={"deleted_path": str(target)})

@staticmethod
def _ensure_filesystem_write_confirmed(confirm_write: bool) -> None:
    """确保写操作已显式确认。"""
    if not confirm_write:
        raise Exception(
            "confirm_write must be true for filesystem_edit_* operations"
        )

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """执行 ok result 相关逻辑。"""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _resolve_filesystem_write_path(self, path: str | Path) -> Path:
    """解析 filesystem write path。"""
    return self._resolve_filesystem_query_path(path)

def _resolve_filesystem_query_path(self, path: str | Path) -> Path:
    """解析 filesystem query path。"""
    return self._normalize_filesystem_path(path)

@staticmethod
def _normalize_filesystem_path(path: str | Path) -> Path:
    """归一化 filesystem path。"""
    candidate = path if isinstance(path, Path) else Path(str(path).strip()).expanduser()
    if not candidate.is_absolute():
        candidate = Path.cwd() / candidate
    return candidate.resolve(strict=False)

TOOL_METHODS: dict[str, object] = {
    'filesystem_edit_delete_entry': filesystem_edit_delete_entry,
    '_filesystem_edit_delete_entry_impl': _filesystem_edit_delete_entry_impl,
    '_ensure_filesystem_write_confirmed': _ensure_filesystem_write_confirmed,
    '_ok_result': _ok_result,
    '_resolve_filesystem_write_path': _resolve_filesystem_write_path,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
