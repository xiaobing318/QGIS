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

TOOL_NAME = 'filesystem_edit_move_entry'
TOOL_DOC = '把文件或目录移动到新位置。 source_path 是源路径，target_path 是目标路径，overwrite 控制是否允许覆盖目标，confirm_destructive 用于确认覆盖已存在目标，confirm_write 用于显式确认写操作。 源路径必须存在；目标若已存在且 overwrite=false 会失败。 会修改磁盘目录结构，源路径在成功后会消失。 所有 filesystem_edit_* 调用都要求 confirm_write=true；删除或覆盖时还必须显式设置 confirm_destructive=true。 返回 source_path 和 target_path 摘要，表示移动已完成。'

def filesystem_edit_move_entry(self, source_path: str, target_path: str, overwrite: bool = False, confirm_destructive: bool = False, confirm_write: bool = False) -> dict[str, Any]:
    """Handle a filesystem entry."""
    return self._run(self._filesystem_edit_move_entry_impl, source_path, target_path, overwrite, confirm_destructive, confirm_write)

def _filesystem_edit_move_entry_impl(self, source_path: str, target_path: str, overwrite: bool, confirm_destructive: bool, confirm_write: bool) -> dict[str, Any]:
    """Build the filesystem entry."""
    self._ensure_filesystem_write_confirmed(confirm_write)
    source = self._resolve_filesystem_write_path(source_path)
    target = self._resolve_filesystem_write_path(target_path)
    if not source.exists():
        raise Exception(f"Source path not found: {source_path}")
    if target.exists():
        if not overwrite:
            raise Exception(f"Target already exists: {target_path}")
        if not confirm_destructive:
            raise Exception("confirm_destructive must be true when overwrite is enabled")
        if target.is_dir():
            shutil.rmtree(target)
        else:
            target.unlink()
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.move(str(source), str(target))
    return self._ok_result("filesystem_edit_move_entry", summary={"source_path": str(source), "target_path": str(target)})

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
    'filesystem_edit_move_entry': filesystem_edit_move_entry,
    '_filesystem_edit_move_entry_impl': _filesystem_edit_move_entry_impl,
    '_ensure_filesystem_write_confirmed': _ensure_filesystem_write_confirmed,
    '_ok_result': _ok_result,
    '_resolve_filesystem_write_path': _resolve_filesystem_write_path,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
