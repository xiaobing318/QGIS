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

TOOL_NAME = 'filesystem_edit_append_text'
TOOL_DOC = '向文本文件尾部追加 UTF-8 内容。 path 是目标文件路径，content 是待追加文本，create_parents 控制父目录不存在时是否自动创建，confirm_write 用于显式确认写操作。 目标路径的父目录必须存在或允许自动创建；目标文件不存在时会被创建。 会在磁盘上创建文件或修改现有文件末尾内容。 该工具不会覆盖已有内容，但仍属于写盘操作，调用前应确认目标路径。 返回追加字符数和最终 path 摘要。'

def filesystem_edit_append_text(self, path: str, content: str, create_parents: bool = True, confirm_write: bool = False) -> dict[str, Any]:
    """
    作用：处理 `filesystem_edit_append_text` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `filesystem_edit_append_text` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `content`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `create_parents`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `True`。
    - 参数 `confirm_write`（`bool`）：布尔开关参数，用于控制是否启用特定行为。 默认值为 `False`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._filesystem_edit_append_text_impl, path, content, create_parents, confirm_write)

def _filesystem_edit_append_text_impl(self, path: str, content: str, create_parents: bool, confirm_write: bool) -> dict[str, Any]:
    """
    作用：实现 `_filesystem_edit_append_text_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_filesystem_edit_append_text_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `content`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `create_parents`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `confirm_write`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
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
    'filesystem_edit_append_text': filesystem_edit_append_text,
    '_filesystem_edit_append_text_impl': _filesystem_edit_append_text_impl,
    '_ensure_filesystem_write_confirmed': _ensure_filesystem_write_confirmed,
    '_ok_result': _ok_result,
    '_resolve_filesystem_write_path': _resolve_filesystem_write_path,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
