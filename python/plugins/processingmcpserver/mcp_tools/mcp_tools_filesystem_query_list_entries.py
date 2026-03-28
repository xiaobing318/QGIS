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

TOOL_NAME = 'filesystem_query_list_entries'
TOOL_DOC = '???列出目录中的文件或子目录条目，适合在执行文件操作前先做只读探查。 ?????directory 是根目录，recursive 控制是否递归，include_files 和 include_directories 控制返回对象类型，name_glob 过滤名称，limit 控制返回上限。 ?????目录必须存在；include_files 与 include_directories 不能同时为 false。 ??????无写操作，只读取文件系统元数据。 ?????limit 会被内部阈值裁剪；结果里会明确 returned_count、matched_total 与 truncated。 ?????返回目录路径、entries 数组以及 limit 应用摘要。'

def filesystem_query_list_entries(self, directory: str, recursive: bool = False, include_files: bool = True, include_directories: bool = True, name_glob: str = "*", limit: int = DEFAULT_FILESYSTEM_LIST_LIMIT) -> dict[str, Any]:
    """Handle entries in a filesystem path."""
    return self._run(self._filesystem_query_list_entries_impl, directory, recursive, include_files, include_directories, name_glob, limit)

def _filesystem_query_list_entries_impl(self, directory: str, recursive: bool, include_files: bool, include_directories: bool, name_glob: str, limit: int) -> dict[str, Any]:
    """Build the entries in a filesystem path."""
    if not include_files and not include_directories:
        raise Exception("include_files and include_directories cannot both be false")
    root = self._resolve_filesystem_query_path(directory)
    if not root.exists() or not root.is_dir():
        raise Exception(f"Directory not found: {directory}")
    pattern = self._normalize_name_glob(name_glob)
    requested, applied, capped = self._normalize_filesystem_limit(limit)
    iterator = root.rglob("*") if recursive else root.iterdir()
    entries: list[dict[str, Any]] = []
    matched_total = 0
    for entry in iterator:
        if entry.is_file() and not include_files:
            continue
        if entry.is_dir() and not include_directories:
            continue
        if not fnmatch(entry.name, pattern):
            continue
        matched_total += 1
        if len(entries) >= applied:
            continue
        info = self._path_info(entry)
        info["relative_path"] = str(entry.relative_to(root))
        entries.append(info)
    return self._ok_result("filesystem_query_list_entries", summary={"returned_count": len(entries), "matched_total": matched_total, "requested_limit": requested, "applied_limit": applied, "limit_capped": capped, "truncated": matched_total > len(entries)}, outputs={"directory": str(root), "entries": entries})

def _normalize_filesystem_limit(self, limit: Any | None) -> tuple[int, int, bool]:
    """Handle normalize filesystem limit."""
    requested = self._safe_int(limit, self.DEFAULT_FILESYSTEM_LIST_LIMIT)
    normalized = max(0, requested)
    applied = min(normalized, self.MAX_FILESYSTEM_LIST_LIMIT)
    return requested, applied, applied != normalized

@staticmethod
def _normalize_name_glob(name_glob: str | None) -> str:
    """Handle normalize name glob."""
    value = (name_glob or "*").strip()
    return value or "*"

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """Handle ok result."""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

@staticmethod
def _path_info(path: Path) -> dict[str, Any]:
    """Handle path info."""
    stat = path.stat()
    return {
        "path": str(path),
        "name": path.name,
        "is_file": path.is_file(),
        "is_directory": path.is_dir(),
        "size_bytes": stat.st_size if path.is_file() else 0,
        "extension": path.suffix.lower(),
        "modified_at": datetime.fromtimestamp(stat.st_mtime, tz=timezone.utc).isoformat(),
    }

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
    'filesystem_query_list_entries': filesystem_query_list_entries,
    '_filesystem_query_list_entries_impl': _filesystem_query_list_entries_impl,
    '_normalize_filesystem_limit': _normalize_filesystem_limit,
    '_normalize_name_glob': _normalize_name_glob,
    '_ok_result': _ok_result,
    '_path_info': _path_info,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_safe_int': _safe_int,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
