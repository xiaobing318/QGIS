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

TOOL_NAME = 'filesystem_edit_move_entry'
TOOL_DOC = '把文件或目录移动到新位置。 source_path 是源路径，target_path 是目标路径，overwrite 控制是否允许覆盖目标，confirm_destructive 用于确认覆盖已存在目标，confirm_write 用于显式确认写操作。 源路径必须存在；目标若已存在且 overwrite=false 会失败。 会修改磁盘目录结构，源路径在成功后会消失。 所有 filesystem_edit_* 调用都要求 confirm_write=true；删除或覆盖时还必须显式设置 confirm_destructive=true。 返回 source_path 和 target_path 摘要，表示移动已完成。'

def filesystem_edit_move_entry(self, source_path: str, target_path: str, overwrite: bool = False, confirm_destructive: bool = False, confirm_write: bool = False) -> dict[str, Any]:
    """
    作用：处理 `filesystem_edit_move_entry` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `filesystem_edit_move_entry` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `source_path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `target_path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `overwrite`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `False`。
    - 参数 `confirm_destructive`（`bool`）：布尔开关参数，用于控制是否启用特定行为。 默认值为 `False`。
    - 参数 `confirm_write`（`bool`）：布尔开关参数，用于控制是否启用特定行为。 默认值为 `False`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._filesystem_edit_move_entry_impl, source_path, target_path, overwrite, confirm_destructive, confirm_write)

def _filesystem_edit_move_entry_impl(self, source_path: str, target_path: str, overwrite: bool, confirm_destructive: bool, confirm_write: bool) -> dict[str, Any]:
    """
    作用：实现 `_filesystem_edit_move_entry_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_filesystem_edit_move_entry_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `source_path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `target_path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `overwrite`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `confirm_destructive`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 参数 `confirm_write`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
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
    """
    作用：确保 `_ensure_filesystem_write_confirmed` 负责的前置状态可用，必要时执行初始化或修复动作。
    用途：确保 `_ensure_filesystem_write_confirmed` 负责的前置状态可用，必要时执行初始化或修复动作。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `confirm_write`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 返回：无返回值。
    返回结果：无返回值。
    异常：可能显式抛出 `Exception`。
    """
    if not confirm_write:
        raise Exception(
            "confirm_write must be true for filesystem_edit_* operations"
        )

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

def _resolve_filesystem_write_path(self, path: str | Path) -> Path:
    """
    作用：封装内部辅助步骤 `_resolve_filesystem_write_path`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_resolve_filesystem_write_path`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `path`（`str | Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._resolve_filesystem_query_path(path)

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
    'filesystem_edit_move_entry': filesystem_edit_move_entry,
    '_filesystem_edit_move_entry_impl': _filesystem_edit_move_entry_impl,
    '_ensure_filesystem_write_confirmed': _ensure_filesystem_write_confirmed,
    '_ok_result': _ok_result,
    '_resolve_filesystem_write_path': _resolve_filesystem_write_path,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
