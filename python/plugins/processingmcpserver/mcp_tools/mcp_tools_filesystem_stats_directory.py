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

TOOL_NAME = 'filesystem_stats_directory'
TOOL_DOC = '???统计目录中的文件数、目录数和累计大小，适合在批处理前估算工作量。 ?????directory 是根目录，recursive 控制是否递归统计。 ?????目录必须存在。 ??????无写操作，只遍历文件系统做统计。 ?????无。 ?????返回文件数、目录数、总字节数等目录统计摘要。'

def filesystem_stats_directory(self, directory: str, recursive: bool = False) -> dict[str, Any]:
    """执行文件系统相关的 stats directory 逻辑。"""
    return self._run(self._filesystem_stats_directory_impl, directory, recursive)

def _filesystem_stats_directory_impl(self, directory: str, recursive: bool) -> dict[str, Any]:
    """执行文件系统相关的 stats directory impl 逻辑。"""
    root = self._resolve_filesystem_query_path(directory)
    if not root.exists() or not root.is_dir():
        raise Exception(f"Directory not found: {directory}")
    iterator = root.rglob("*") if recursive else root.iterdir()
    file_count = 0
    directory_count = 0
    total_size_bytes = 0
    extensions: dict[str, int] = {}
    for entry in iterator:
        if entry.is_dir():
            directory_count += 1
            continue
        file_count += 1
        total_size_bytes += entry.stat().st_size
        ext = entry.suffix.lower() or "<no_ext>"
        extensions[ext] = extensions.get(ext, 0) + 1
    return self._ok_result("filesystem_stats_directory", summary={"file_count": file_count, "directory_count": directory_count, "total_size_bytes": total_size_bytes}, outputs={"extensions": extensions})

@staticmethod
def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
    """执行 ok result 相关逻辑。"""
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

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
    'filesystem_stats_directory': filesystem_stats_directory,
    '_filesystem_stats_directory_impl': _filesystem_stats_directory_impl,
    '_ok_result': _ok_result,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
