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

TOOL_NAME = 'filesystem_query_list_entries'
TOOL_DOC = '列出目录中的文件或子目录条目，适合在执行文件操作前先做只读探查。 directory 是根目录，recursive 控制是否递归，include_files 和 include_directories 控制返回对象类型，name_glob 过滤名称，limit 控制返回上限。 目录必须存在；include_files 与 include_directories 不能同时为 false。 无写操作，只读取文件系统元数据。 limit 会被内部阈值裁剪；结果里会明确 returned_count、matched_total 与 truncated。 返回目录路径、entries 数组以及 limit 应用摘要。'

def filesystem_query_list_entries(self, directory: str, recursive: bool = False, include_files: bool = True, include_directories: bool = True, name_glob: str = "*", limit: int = DEFAULT_FILESYSTEM_LIST_LIMIT) -> dict[str, Any]:
    """
    作用：处理 `filesystem_query_list_entries` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `filesystem_query_list_entries` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `directory`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `recursive`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `False`。
    - 参数 `include_files`（`bool`）：路径类参数，用于定位输入或输出文件系统位置。 默认值为 `True`。
    - 参数 `include_directories`（`bool`）：路径类参数，用于定位输入或输出文件系统位置。 默认值为 `True`。
    - 参数 `name_glob`（`str`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `"*"`。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。 默认值为 `DEFAULT_FILESYSTEM_LIST_LIMIT`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._filesystem_query_list_entries_impl, directory, recursive, include_files, include_directories, name_glob, limit)

def _filesystem_query_list_entries_impl(self, directory: str, recursive: bool, include_files: bool, include_directories: bool, name_glob: str, limit: int) -> dict[str, Any]:
    """
    作用：实现 `_filesystem_query_list_entries_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_filesystem_query_list_entries_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `directory`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `recursive`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `include_files`（`bool`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `include_directories`（`bool`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `name_glob`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
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
    """
    作用：封装内部辅助步骤 `_normalize_filesystem_limit`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_normalize_filesystem_limit`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `limit`（`Any | None`）：数值控制参数，用于限制范围、数量或时限。
    - 返回：返回 `tuple[int, int, bool]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[int, int, bool]` 类型结果，返回值语义遵循该函数实现约定。
    """
    requested = self._safe_int(limit, self.DEFAULT_FILESYSTEM_LIST_LIMIT)
    normalized = max(0, requested)
    applied = min(normalized, self.MAX_FILESYSTEM_LIST_LIMIT)
    return requested, applied, applied != normalized

@staticmethod
def _normalize_name_glob(name_glob: str | None) -> str:
    """
    作用：封装内部辅助步骤 `_normalize_name_glob`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_normalize_name_glob`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `name_glob`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    value = (name_glob or "*").strip()
    return value or "*"

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

@staticmethod
def _path_info(path: Path) -> dict[str, Any]:
    """
    作用：封装内部辅助步骤 `_path_info`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_path_info`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `path`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
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
def _safe_int(value: Any, default: int) -> int:
    """
    作用：封装内部辅助步骤 `_safe_int`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_safe_int`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `value`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `default`（`int`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `int` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `int` 类型结果，返回值语义遵循该函数实现约定。
    """
    try:
        return int(value)
    except (TypeError, ValueError):
        return default

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
