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

TOOL_NAME = 'project_cleanup_work_layers'
TOOL_DOC = '按 task_name 或 layer_ids 清理 shapefile 工作流创建的临时图层，并可选删除显式登记的临时文件。 task_name 用于按任务标签批量匹配工作层，layer_ids 可精确指定图层，temp_paths 可补充显式临时路径，delete_temp_files 控制是否删除这些路径，confirm_write 与 confirm_destructive 仅在删文件时生效。 至少提供可匹配的 task_name、layer_ids 或 temp_paths 中的一类才有意义；若 delete_temp_files=true 且存在路径，则必须 confirm_write=true 且 confirm_destructive=true。 会从当前工程移除匹配的临时图层；可选地删除临时文件或临时目录。 删除磁盘临时文件时需要显式双确认；仅移除工程图层时不触碰源数据文件。 返回 removed_layer_ids、deleted_temp_paths 和 missing_temp_paths，便于回收检查与日志留存。'

def project_cleanup_work_layers(self, task_name: str = "", layer_ids: list[str] | None = None, temp_paths: list[str] | None = None, delete_temp_files: bool = True, confirm_write: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
    """
    作用：处理 `project_cleanup_work_layers` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `project_cleanup_work_layers` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `""`。
    - 参数 `layer_ids`（`list[str] | None`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `None`。
    - 参数 `temp_paths`（`list[str] | None`）：路径类参数，用于定位输入或输出文件系统位置。 默认值为 `None`。
    - 参数 `delete_temp_files`（`bool`）：路径类参数，用于定位输入或输出文件系统位置。 默认值为 `True`。
    - 参数 `confirm_write`（`bool`）：布尔开关参数，用于控制是否启用特定行为。 默认值为 `False`。
    - 参数 `confirm_destructive`（`bool`）：布尔开关参数，用于控制是否启用特定行为。 默认值为 `False`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._project_cleanup_work_layers_impl, task_name, layer_ids, temp_paths, delete_temp_files, confirm_write, confirm_destructive)

def _project_cleanup_work_layers_impl(
    self,
    task_name: str,
    layer_ids: list[str] | None,
    temp_paths: list[str] | None,
    delete_temp_files: bool,
    confirm_write: bool,
    confirm_destructive: bool,
) -> dict[str, Any]:
    """
    作用：实现 `_project_cleanup_work_layers_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_project_cleanup_work_layers_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `layer_ids`（`list[str] | None`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `temp_paths`（`list[str] | None`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `delete_temp_files`（`bool`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `confirm_write`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 参数 `confirm_destructive`（`bool`）：布尔开关参数，用于控制是否启用特定行为。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    normalized_task = (task_name or "").strip()
    target_layers = self._collect_cleanup_target_layers(normalized_task, layer_ids)
    collected_temp_paths: list[str] = []
    for layer in target_layers:
        collected_temp_paths.extend(self._get_layer_temp_paths(layer))
    collected_temp_paths.extend(
        [
            str(item).strip()
            for item in (temp_paths or [])
            if str(item).strip()
        ]
    )
    unique_temp_paths = list(dict.fromkeys(collected_temp_paths))

    removed_layer_ids: list[str] = []
    for layer in target_layers:
        removed_layer_ids.append(layer.id())
        QgsProject.instance().removeMapLayer(layer.id())

    deleted_temp_paths: list[str] = []
    missing_temp_paths: list[str] = []
    if delete_temp_files and unique_temp_paths:
        self._ensure_filesystem_write_confirmed(confirm_write)
        if not confirm_destructive:
            raise Exception(
                "confirm_destructive must be true when delete_temp_files is enabled"
            )
        for raw_path in unique_temp_paths:
            candidate = self._resolve_filesystem_write_path(raw_path)
            if candidate.suffix.lower() == ".shp":
                if candidate.exists():
                    if not QgsVectorFileWriter.deleteShapeFile(str(candidate)):
                        raise Exception(f"Failed to delete shapefile bundle: {candidate}")
                    deleted_temp_paths.append(str(candidate))
                else:
                    missing_temp_paths.append(str(candidate))
                continue
            if candidate.is_dir():
                shutil.rmtree(candidate)
                deleted_temp_paths.append(str(candidate))
                continue
            if candidate.exists():
                candidate.unlink()
                deleted_temp_paths.append(str(candidate))
            else:
                missing_temp_paths.append(str(candidate))

    if normalized_task:
        self._ensure_shapefile_run_summary(normalized_task, status="cleaned")
    self._append_shapefile_run_step(
        "project_cleanup_work_layers",
        summary={
            "removed_layer_count": len(removed_layer_ids),
            "deleted_temp_path_count": len(deleted_temp_paths),
            "missing_temp_path_count": len(missing_temp_paths),
        },
        outputs={
            "removed_layer_ids": removed_layer_ids,
            "deleted_temp_paths": deleted_temp_paths,
            "missing_temp_paths": missing_temp_paths,
        },
        status="cleaned",
    )

    return self._ok_result(
        "project_cleanup_work_layers",
        summary={
            "task_name": normalized_task,
            "removed_layer_count": len(removed_layer_ids),
            "deleted_temp_path_count": len(deleted_temp_paths),
            "missing_temp_path_count": len(missing_temp_paths),
        },
        outputs={
            "removed_layer_ids": removed_layer_ids,
            "deleted_temp_paths": deleted_temp_paths,
            "missing_temp_paths": missing_temp_paths,
        },
    )

def _append_shapefile_run_step(
    self,
    step: str,
    summary: dict[str, Any] | None = None,
    outputs: dict[str, Any] | None = None,
    warnings: list[str] | None = None,
    status: str | None = None,
) -> None:
    """
    作用：封装内部辅助步骤 `_append_shapefile_run_step`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_append_shapefile_run_step`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `step`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `summary`（`dict[str, Any] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `outputs`（`dict[str, Any] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `warnings`（`list[str] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 参数 `status`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `None`。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    payload = {
        "step": step,
        "generated_at": self._utc_now_iso(),
        "summary": self._serialize_value(summary or {}),
        "outputs": self._serialize_value(outputs or {}),
    }
    if warnings:
        payload["warnings"] = list(warnings)
        existing = self._shapefile_run_summary.setdefault("warnings", [])
        existing.extend(warnings)
    self._shapefile_run_summary.setdefault("steps", []).append(payload)
    if status:
        self._shapefile_run_summary["status"] = status

def _collect_cleanup_target_layers(
    self,
    task_name: str,
    layer_ids: list[str] | None,
) -> list[QgsMapLayer]:
    """
    作用：封装内部辅助步骤 `_collect_cleanup_target_layers`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_collect_cleanup_target_layers`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `layer_ids`（`list[str] | None`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `list[QgsMapLayer]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `list[QgsMapLayer]` 类型结果，返回值语义遵循该函数实现约定。
    """
    requested_ids = {
        str(layer_id).strip()
        for layer_id in (layer_ids or [])
        if str(layer_id).strip()
    }
    normalized_task = (task_name or "").strip()
    layers: list[QgsMapLayer] = []
    for layer in QgsProject.instance().mapLayers().values():
        if requested_ids and layer.id() in requested_ids:
            layers.append(layer)
            continue
        if not normalized_task:
            continue
        if (
            self._layer_custom_property(
                layer,
                "processingmcpserver/workflow/task_name",
                "",
            )
            == normalized_task
        ):
            layers.append(layer)
    unique: dict[str, QgsMapLayer] = {layer.id(): layer for layer in layers}
    return list(unique.values())

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

def _ensure_shapefile_run_summary(
    self,
    task_name: str = "",
    status: str | None = None,
    inputs: dict[str, Any] | None = None,
) -> str:
    """
    作用：确保 `_ensure_shapefile_run_summary` 负责的前置状态可用，必要时执行初始化或修复动作。
    用途：确保 `_ensure_shapefile_run_summary` 负责的前置状态可用，必要时执行初始化或修复动作。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `""`。
    - 参数 `status`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `None`。
    - 参数 `inputs`（`dict[str, Any] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    normalized = (task_name or self._shapefile_run_summary.get("task_name") or "").strip()
    if (
        not self._shapefile_run_summary.get("task_name")
        or (
            normalized
            and normalized != self._shapefile_run_summary.get("task_name")
        )
    ):
        return self._reset_shapefile_run_summary(
            normalized,
            status or "initialized",
            inputs=inputs,
        )

    if status:
        self._shapefile_run_summary["status"] = status
    if inputs:
        current = self._shapefile_run_summary.setdefault("inputs", {})
        current.update(self._serialize_value(inputs))
    self._shapefile_run_summary["generated_at"] = self._utc_now_iso()
    return normalized

def _get_layer_temp_paths(self, layer: QgsMapLayer) -> list[str]:
    """
    作用：封装内部辅助步骤 `_get_layer_temp_paths`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_get_layer_temp_paths`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `layer`（`QgsMapLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `list[str]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `list[str]` 类型结果，返回值语义遵循该函数实现约定。
    """
    raw = self._layer_custom_property(
        layer,
        "processingmcpserver/workflow/temp_paths",
        "[]",
    )
    if isinstance(raw, str):
        try:
            parsed = json.loads(raw)
        except Exception:
            return []
        if isinstance(parsed, list):
            return [str(item) for item in parsed if str(item).strip()]
    if isinstance(raw, list):
        return [str(item) for item in raw if str(item).strip()]
    return []

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

@staticmethod
def _serialize_value(value: Any) -> Any:
    """
    作用：封装内部辅助步骤 `_serialize_value`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_serialize_value`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `value`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
    """
    if value is None:
        return None
    if isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, (date, datetime, time_value)):
        return value.isoformat()
    if isinstance(value, QDateTime):
        return value.toString(Qt.DateFormat.ISODateWithMs)
    if isinstance(value, QDate):
        return value.toString(Qt.DateFormat.ISODate)
    if isinstance(value, QTime):
        return value.toString(Qt.DateFormat.ISODateWithMs)
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, (list, tuple, set)):
        return [_serialize_value(item) for item in value]
    if isinstance(value, dict):
        return {str(k): _serialize_value(v) for k, v in value.items()}
    if hasattr(value, "id") and callable(value.id):
        try:
            return value.id()
        except Exception:
            pass
    if hasattr(value, "toString") and callable(value.toString):
        try:
            return value.toString()
        except Exception:
            pass
    return str(value)

@staticmethod
def _utc_now_iso() -> str:
    """
    作用：封装内部辅助步骤 `_utc_now_iso`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_utc_now_iso`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数：无。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")

@staticmethod
def _layer_custom_property(layer: QgsMapLayer, key: str, default: Any = None) -> Any:
    """
    作用：封装内部辅助步骤 `_layer_custom_property`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_layer_custom_property`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `layer`（`QgsMapLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `key`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `default`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 返回：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Any` 类型结果，返回值语义遵循该函数实现约定。
    """
    if hasattr(layer, "customProperty"):
        try:
            return layer.customProperty(key, default)
        except Exception:
            return default
    return default

def _reset_shapefile_run_summary(
    self,
    task_name: str,
    status: str,
    inputs: dict[str, Any] | None = None,
) -> str:
    """
    作用：封装内部辅助步骤 `_reset_shapefile_run_summary`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_reset_shapefile_run_summary`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `status`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `inputs`（`dict[str, Any] | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    normalized = (task_name or "").strip()
    self._shapefile_run_summary = self._empty_shapefile_run_summary()
    self._shapefile_run_summary["task_name"] = normalized
    self._shapefile_run_summary["status"] = status
    if inputs:
        self._shapefile_run_summary["inputs"] = self._serialize_value(inputs)
    return normalized

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

def _empty_shapefile_run_summary(self) -> dict[str, Any]:
    """
    作用：封装内部辅助步骤 `_empty_shapefile_run_summary`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_empty_shapefile_run_summary`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return {
        "schema_version": "1.0.0",
        "generated_at": self._utc_now_iso(),
        "task_name": "",
        "status": "idle",
        "inputs": {},
        "steps": [],
        "warnings": [],
        "outputs": {
            "work_layer_ids": [],
            "final_shapefiles": [],
            "removed_layer_ids": [],
            "removed_temp_files": [],
        },
    }

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
    'project_cleanup_work_layers': project_cleanup_work_layers,
    '_project_cleanup_work_layers_impl': _project_cleanup_work_layers_impl,
    '_append_shapefile_run_step': _append_shapefile_run_step,
    '_collect_cleanup_target_layers': _collect_cleanup_target_layers,
    '_ensure_filesystem_write_confirmed': _ensure_filesystem_write_confirmed,
    '_ensure_shapefile_run_summary': _ensure_shapefile_run_summary,
    '_get_layer_temp_paths': _get_layer_temp_paths,
    '_ok_result': _ok_result,
    '_resolve_filesystem_write_path': _resolve_filesystem_write_path,
    '_serialize_value': _serialize_value,
    '_utc_now_iso': _utc_now_iso,
    '_layer_custom_property': _layer_custom_property,
    '_reset_shapefile_run_summary': _reset_shapefile_run_summary,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_empty_shapefile_run_summary': _empty_shapefile_run_summary,
    '_normalize_filesystem_path': _normalize_filesystem_path,
}
