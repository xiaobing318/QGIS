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

TOOL_NAME = 'dataset_inspect_shapefile_bundle'
TOOL_DOC = '扫描目录或单个 `.shp` 文件，检查 shapefile bundle 及其 sidecar 文件是否完整，并返回结构化风险摘要。 path 可以是目录或单个 `.shp` 文件，recursive 控制目录递归扫描，name_glob 过滤候选文件，limit 控制返回上限，task_name 可把结果写入最近一次运行摘要。 path 必须存在；若传文件则必须是 `.shp`。 无写操作，只读取 shapefile bundle 成员信息和矢量元数据。 limit 会被内部阈值裁剪；task_name 非空时会更新 qgis://workflow/shapefile/run-summary。 返回 bundles 数组，元素包含 `.shp/.shx/.dbf/.prj/.cpg` 完整性、大小、CRS、几何类型、字段名、命名风险和 warnings。'

def dataset_inspect_shapefile_bundle(self, path: str, recursive: bool = False, name_glob: str = "*.shp", limit: int = DEFAULT_DATASET_LIMIT, task_name: str = "") -> dict[str, Any]:
    """
    作用：处理 `dataset_inspect_shapefile_bundle` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `dataset_inspect_shapefile_bundle` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `recursive`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `False`。
    - 参数 `name_glob`（`str`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `"*.shp"`。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。 默认值为 `DEFAULT_DATASET_LIMIT`。
    - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `""`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._dataset_inspect_shapefile_bundle_impl, path, recursive, name_glob, limit, task_name)

def _dataset_inspect_shapefile_bundle_impl(
    self,
    path: str,
    recursive: bool,
    name_glob: str,
    limit: int,
    task_name: str,
) -> dict[str, Any]:
    """
    作用：实现 `_dataset_inspect_shapefile_bundle_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_dataset_inspect_shapefile_bundle_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `recursive`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `name_glob`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。
    - 参数 `task_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    target = self._resolve_filesystem_query_path(path)
    if not target.exists():
        raise Exception(f"Path not found: {path}")
    pattern = self._normalize_name_glob(name_glob or "*.shp")
    requested, applied, capped = self._normalize_dataset_limit(limit)

    if target.is_file():
        if target.suffix.lower() != ".shp":
            raise Exception(f"Shapefile path is required: {path}")
        candidates = [target]
    else:
        iterator = target.rglob("*.shp") if recursive else target.glob("*.shp")
        candidates = sorted(
            entry
            for entry in iterator
            if entry.is_file() and fnmatch(entry.name, pattern)
        )

    bundles: list[dict[str, Any]] = []
    matched_total = 0
    blocking_bundle_count = 0
    warning_bundle_count = 0
    for candidate in candidates:
        matched_total += 1
        if len(bundles) >= applied:
            continue
        bundle = self._inspect_shapefile_bundle_entry(candidate)
        if not bundle["complete_bundle"]:
            blocking_bundle_count += 1
        if bundle["warnings"]:
            warning_bundle_count += 1
        bundles.append(bundle)

    effective_task = ""
    if task_name.strip():
        effective_task = self._ensure_shapefile_run_summary(
            self._normalize_task_name(task_name, target.stem),
            status="prechecked",
            inputs={
                "input_path": str(target),
                "recursive": bool(recursive),
                "name_glob": pattern,
            },
        )
        self._append_shapefile_run_step(
            "inspect_shapefile_bundle",
            summary={
                "returned": len(bundles),
                "matched_total": matched_total,
                "blocking_bundle_count": blocking_bundle_count,
                "warning_bundle_count": warning_bundle_count,
            },
            outputs={"bundles": bundles},
            status="prechecked",
        )

    return self._ok_result(
        "dataset_inspect_shapefile_bundle",
        summary={
            "task_name": effective_task,
            "returned": len(bundles),
            "matched_total": matched_total,
            "requested_limit": requested,
            "applied_limit": applied,
            "limit_capped": capped,
            "blocking_bundle_count": blocking_bundle_count,
            "warning_bundle_count": warning_bundle_count,
        },
        outputs={
            "path": str(target),
            "bundles": bundles,
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

def _inspect_shapefile_bundle_entry(self, shp_path: Path) -> dict[str, Any]:
    """
    作用：封装内部辅助步骤 `_inspect_shapefile_bundle_entry`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_inspect_shapefile_bundle_entry`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `shp_path`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    required_suffixes = [".shp", ".shx", ".dbf"]
    optional_suffixes = [".prj", ".cpg"]
    members: dict[str, dict[str, Any]] = {}
    missing_required: list[str] = []
    missing_optional: list[str] = []
    total_size_bytes = 0

    for suffix in required_suffixes + optional_suffixes:
        member_path = self._bundle_member_path(shp_path, suffix)
        exists = member_path.exists()
        members[suffix[1:]] = {
            "path": str(member_path),
            "exists": exists,
            "size_bytes": member_path.stat().st_size if exists else 0,
        }
        if exists:
            total_size_bytes += member_path.stat().st_size
        elif suffix in required_suffixes:
            missing_required.append(suffix[1:])
        else:
            missing_optional.append(suffix[1:])

    layer = QgsVectorLayer(str(shp_path), "__shapefile_probe__", "ogr")
    geometry_type = "unknown"
    crs_authid = ""
    feature_count = 0
    field_names: list[str] = []
    encoding = ""
    if layer.isValid():
        geometry_type = self._display_geometry_type(layer)
        crs_authid = layer.crs().authid() if layer.crs().isValid() else ""
        feature_count = layer.featureCount()
        field_names = [field.name() for field in layer.fields()]
        provider = layer.dataProvider()
        if provider is not None and hasattr(provider, "encoding"):
            try:
                encoding = str(provider.encoding() or "")
            except Exception:
                encoding = ""

    warnings: list[str] = []
    if "prj" in missing_optional:
        warnings.append("Missing .prj file; CRS exchange may be unreliable.")
    if "cpg" in missing_optional:
        warnings.append("Missing .cpg file; encoding may be ambiguous.")

    return {
        "stem": shp_path.stem,
        "bundle_path": str(shp_path),
        "complete_bundle": not missing_required,
        "missing_required": missing_required,
        "missing_optional": missing_optional,
        "members": members,
        "size_bytes": total_size_bytes,
        "geometry_type": geometry_type,
        "crs": crs_authid,
        "feature_count": feature_count,
        "field_names": field_names,
        "field_name_length_gt_10": [
            name for name in field_names if len(name) > 10
        ],
        "encoding": encoding,
        "warnings": warnings,
    }

def _normalize_dataset_limit(self, limit: Any | None) -> tuple[int, int, bool]:
    """
    作用：封装内部辅助步骤 `_normalize_dataset_limit`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_normalize_dataset_limit`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `limit`（`Any | None`）：数值控制参数，用于限制范围、数量或时限。
    - 返回：返回 `tuple[int, int, bool]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[int, int, bool]` 类型结果，返回值语义遵循该函数实现约定。
    """
    requested = self._safe_int(limit, self.DEFAULT_DATASET_LIMIT)
    normalized = max(0, requested)
    applied = min(normalized, self.MAX_DATASET_LIMIT)
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
def _normalize_task_name(task_name: str | None, fallback: str = "") -> str:
    """
    作用：封装内部辅助步骤 `_normalize_task_name`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_normalize_task_name`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `task_name`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `fallback`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `""`。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    value = (task_name or "").strip()
    if value:
        return value
    return fallback.strip() or "shapefile-task"

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

@staticmethod
def _bundle_member_path(shp_path: Path, suffix: str) -> Path:
    """
    作用：封装内部辅助步骤 `_bundle_member_path`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_bundle_member_path`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `shp_path`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `suffix`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    """
    return shp_path.with_suffix(suffix)

@staticmethod
def _display_geometry_type(layer: QgsVectorLayer) -> str:
    """
    作用：封装内部辅助步骤 `_display_geometry_type`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_display_geometry_type`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `layer`（`QgsVectorLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    return {
        Qgis.GeometryType.Point: "point",
        Qgis.GeometryType.Line: "line",
        Qgis.GeometryType.Polygon: "polygon",
        Qgis.GeometryType.Null: "null",
        Qgis.GeometryType.Unknown: "unknown",
    }.get(layer.geometryType(), "unknown")

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

TOOL_METHODS: dict[str, object] = {
    'dataset_inspect_shapefile_bundle': dataset_inspect_shapefile_bundle,
    '_dataset_inspect_shapefile_bundle_impl': _dataset_inspect_shapefile_bundle_impl,
    '_append_shapefile_run_step': _append_shapefile_run_step,
    '_ensure_shapefile_run_summary': _ensure_shapefile_run_summary,
    '_inspect_shapefile_bundle_entry': _inspect_shapefile_bundle_entry,
    '_normalize_dataset_limit': _normalize_dataset_limit,
    '_normalize_name_glob': _normalize_name_glob,
    '_normalize_task_name': _normalize_task_name,
    '_ok_result': _ok_result,
    '_resolve_filesystem_query_path': _resolve_filesystem_query_path,
    '_serialize_value': _serialize_value,
    '_utc_now_iso': _utc_now_iso,
    '_reset_shapefile_run_summary': _reset_shapefile_run_summary,
    '_bundle_member_path': _bundle_member_path,
    '_display_geometry_type': _display_geometry_type,
    '_safe_int': _safe_int,
    '_normalize_filesystem_path': _normalize_filesystem_path,
    '_empty_shapefile_run_summary': _empty_shapefile_run_summary,
}
