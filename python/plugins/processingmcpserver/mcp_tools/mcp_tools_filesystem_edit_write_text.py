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

TOOL_NAME = 'filesystem_edit_write_text'
TOOL_DOC = '???以 UTF-8 一次性写入文本文件，适合新建配置或覆盖写文件。 ?????path 是目标文件路径，content 是完整文本内容，overwrite 控制是否允许覆盖已存在文件，confirm_destructive 用于确认覆盖，create_parents 控制是否自动创建父目录，confirm_write 用于显式确认写操作。 ?????若目标已存在且 overwrite=false 会直接失败；父目录不存在时只有 create_parents=true 才会自动创建。 ??????会创建或覆盖磁盘文件。 ?????所有 filesystem_edit_* 调用都要求 confirm_write=true；删除或覆盖时还必须显式设置 confirm_destructive=true。 ?????返回写入字符数和最终 path 摘要。'

def filesystem_edit_write_text(self, path: str, content: str, overwrite: bool = False, confirm_destructive: bool = False, create_parents: bool = True, confirm_write: bool = False) -> dict[str, Any]:
    """执行文件系统相关的 edit write text 逻辑。"""
    return self._run(self._filesystem_edit_write_text_impl, path, content, overwrite, confirm_destructive, create_parents, confirm_write)

def _filesystem_edit_write_text_impl(self, path: str, content: str, overwrite: bool, confirm_destructive: bool, create_parents: bool, confirm_write: bool) -> dict[str, Any]:
    """执行文件系统相关的 edit write text impl 逻辑。"""
    self._ensure_filesystem_write_confirmed(confirm_write)
    target = self._resolve_filesystem_write_path(path)
    if target.exists():
        if overwrite:
            if not confirm_destructive:
                raise Exception("confirm_destructive must be true when overwrite is enabled")
        else:
            raise Exception(f"File already exists: {path}")
    if not target.parent.exists():
        if create_parents:
            target.parent.mkdir(parents=True, exist_ok=True)
        else:
            raise Exception(f"Parent directory not found: {target.parent}")
    target.write_text(content, encoding="utf-8")
    return self._ok_result("filesystem_edit_write_text", summary={"written_chars": len(content), "path": str(target)})

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
    'filesystem_edit_write_text': filesystem_edit_write_text,
    '_filesystem_edit_write_text_impl': _filesystem_edit_write_text_impl,
    '_ensure_filesystem_write_confirmed': _ensure_filesystem_write_confirmed,
    '_ok_result': _ok_result,
    '_resolve_filesystem_write_path': _resolve_filesystem_write_path,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
