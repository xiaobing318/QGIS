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

TOOL_NAME = 'filesystem_query_entry_info'
TOOL_DOC = '???读取单个文件或目录的基础元数据。 ?????path 指向文件或目录。 ?????目标路径必须存在。 ??????无写操作，只读取文件系统元数据。 ?????无。 ?????返回 entry 对象，包含类型、大小、时间戳和可用路径信息。'

def filesystem_query_entry_info(self, path: str) -> dict[str, Any]:
    """Handle filesystem entry information."""
    return self._run(self._filesystem_query_entry_info_impl, path)

def _filesystem_query_entry_info_impl(self, path: str) -> dict[str, Any]:
    """Build the filesystem entry information."""
    entry = self._resolve_filesystem_query_path(path)
    if not entry.exists():
        raise Exception(f"Path not found: {path}")
    return self._ok_result("filesystem_query_entry_info", summary={"exists": True}, outputs={"entry": self._path_info(entry)})

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
def _normalize_filesystem_path(path: str | Path) -> Path:
    """Handle normalize filesystem path."""
    candidate = path if isinstance(path, Path) else Path(str(path).strip()).expanduser()
    if not candidate.is_absolute():
        candidate = Path.cwd() / candidate
    return candidate.resolve(strict=False)

TOOL_METHODS: dict[str, object] = {
    'filesystem_query_entry_info': filesystem_query_entry_info,
    '_filesystem_query_entry_info_impl': _filesystem_query_entry_info_impl,
    '_ok_result': _ok_result,
    '_path_info': _path_info,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
