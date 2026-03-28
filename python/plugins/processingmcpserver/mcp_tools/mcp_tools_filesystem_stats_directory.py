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
    """
    作用：确保 `_ensure_processing_initialized` 负责的前置状态可用，必要时执行初始化或修复动作。
    用途：确保 `_ensure_processing_initialized` 负责的前置状态可用，必要时执行初始化或修复动作。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数：无。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    global _PROCESSING_INITIALIZED
    if _PROCESSING_INITIALIZED:
        return
    Processing.initialize()
    _PROCESSING_INITIALIZED = True

TOOL_NAME = 'filesystem_stats_directory'
TOOL_DOC = '统计目录中的文件数、目录数和累计大小，适合在批处理前估算工作量。 directory 是根目录，recursive 控制是否递归统计。 目录必须存在。 无写操作，只遍历文件系统做统计。 无。 返回文件数、目录数、总字节数等目录统计摘要。'

def filesystem_stats_directory(self, directory: str, recursive: bool = False) -> dict[str, Any]:
    """
    作用：处理 `filesystem_stats_directory` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `filesystem_stats_directory` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `directory`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `recursive`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `False`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._filesystem_stats_directory_impl, directory, recursive)

def _filesystem_stats_directory_impl(self, directory: str, recursive: bool) -> dict[str, Any]:
    """
    作用：实现 `_filesystem_stats_directory_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_filesystem_stats_directory_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `directory`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `recursive`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
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
    """
    作用：封装内部辅助步骤 `_ok_result`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_ok_result`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `tool`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `summary`（`dict[str, Any] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `outputs`（`dict[str, Any] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `warnings`（`list[str] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `**extra`：可变关键字参数，用于扩展命名输入。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
    if warnings is not None:
        payload["warnings"] = warnings
    payload.update(extra)
    return payload

def _resolve_filesystem_query_path(self, path: str | Path) -> Path:
    """
    作用：封装内部辅助步骤 `_resolve_filesystem_query_path`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_resolve_filesystem_query_path`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `path`（`str | Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._normalize_filesystem_path(path)

@staticmethod
def _normalize_filesystem_path(path: str | Path) -> Path:
    """
    作用：封装内部辅助步骤 `_normalize_filesystem_path`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_normalize_filesystem_path`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `path`（`str | Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    """
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
