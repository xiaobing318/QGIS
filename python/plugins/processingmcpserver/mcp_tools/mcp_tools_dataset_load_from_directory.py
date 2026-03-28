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

TOOL_NAME = 'dataset_load_from_directory'
TOOL_DOC = '先扫描目录中的候选数据集，再把匹配的 vector 或 raster 图层批量加载到当前工程。 参数和 dataset_list_files 基本一致，skip_invalid 控制遇到坏数据时是跳过还是整体失败。 目录必须存在，且目录下至少有可识别的数据集文件才有意义。 会向当前工程新增多个图层，但不会改写源数据文件。 skip_invalid=true 时失败项会进入 failed；skip_invalid=false 时第一个失败就终止。 返回 requested_count、loaded_count、failed_count，以及 loaded 和 failed 明细。'

def dataset_load_from_directory(self, directory: str, recursive: bool = False, dataset_kind: str = "both", geometry_type: str = "any", name_glob: str = "*", limit: int = DEFAULT_DATASET_LIMIT, skip_invalid: bool = True) -> dict[str, Any]:
    """
    作用：处理 `dataset_load_from_directory` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `dataset_load_from_directory` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `directory`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `recursive`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `False`。
    - 参数 `dataset_kind`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"both"`。
    - 参数 `geometry_type`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"any"`。
    - 参数 `name_glob`（`str`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `"*"`。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。 默认值为 `DEFAULT_DATASET_LIMIT`。
    - 参数 `skip_invalid`（`bool`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `True`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._dataset_load_from_directory_impl, directory, recursive, dataset_kind, geometry_type, name_glob, limit, skip_invalid)

def _dataset_load_from_directory_impl(self, directory: str, recursive: bool, dataset_kind: str, geometry_type: str, name_glob: str, limit: int, skip_invalid: bool) -> dict[str, Any]:
    """
    作用：实现 `_dataset_load_from_directory_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_dataset_load_from_directory_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `directory`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `recursive`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `dataset_kind`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `geometry_type`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `name_glob`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。
    - 参数 `skip_invalid`（`bool`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    list_result = self._dataset_list_files_impl(directory, recursive, dataset_kind, geometry_type, name_glob, limit)
    loaded: list[dict[str, Any]] = []
    failed: list[dict[str, Any]] = []
    for dataset in list_result["datasets"]:
        path = dataset["path"]
        try:
            if dataset["dataset_kind"] == "vector":
                loaded.append(self._vector_add_layer_impl(path, provider="ogr"))
            else:
                loaded.append(self._raster_add_layer_impl(path, provider="gdal"))
        except Exception as exc:
            if not skip_invalid:
                raise Exception(f"Failed to load dataset from directory: {path}. {exc}") from exc
            failed.append({"path": path, "error": str(exc)})
    return {"requested_count": len(list_result["datasets"]), "loaded_count": len(loaded), "failed_count": len(failed), "loaded": loaded, "failed": failed}

def _dataset_list_files_impl(self, directory: str, recursive: bool, dataset_kind: str, geometry_type: str, name_glob: str, limit: int) -> dict[str, Any]:
    """
    作用：实现 `_dataset_list_files_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_dataset_list_files_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `directory`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `recursive`（`bool`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `dataset_kind`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `geometry_type`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 参数 `name_glob`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `limit`（`int`）：数值控制参数，用于限制范围、数量或时限。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    root = Path(directory)
    if not root.exists() or not root.is_dir():
        raise Exception(f"Directory not found: {directory}")
    kind_filter = self._normalize_dataset_kind(dataset_kind)
    geom_filter = self._normalize_geometry_type_filter(geometry_type)
    pattern = self._normalize_name_glob(name_glob)
    requested, applied, capped = self._normalize_dataset_limit(limit)

    iterator = root.rglob("*") if recursive else root.iterdir()
    datasets: list[dict[str, Any]] = []
    matched_total = 0
    for entry in iterator:
        if not entry.is_file() or not fnmatch(entry.name, pattern):
            continue
        detected_kind = self._detect_dataset_kind(entry)
        if detected_kind is None:
            continue
        if kind_filter != "both" and detected_kind != kind_filter:
            continue
        detected_geom = self._detect_vector_geometry_type(entry) if detected_kind == "vector" else "unknown"
        if geom_filter != "any" and detected_kind == "vector" and detected_geom != geom_filter:
            continue
        matched_total += 1
        if len(datasets) >= applied:
            continue
        datasets.append({"path": str(entry), "name": entry.name, "dataset_kind": detected_kind, "geometry_type": detected_geom, "size_bytes": entry.stat().st_size})

    return {"directory": str(root), "requested_limit": requested, "applied_limit": applied, "limit_capped": capped, "returned": len(datasets), "matched_total": matched_total, "truncated": matched_total > len(datasets), "datasets": datasets}

def _raster_add_layer_impl(self, path: str, provider: str = "gdal", name: str | None = None) -> dict[str, Any]:
    """
    作用：实现 `_raster_add_layer_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_raster_add_layer_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `provider`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"gdal"`。
    - 参数 `name`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `None`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    layer = QgsRasterLayer(path, name or Path(path).stem, provider)
    if not layer.isValid():
        raise Exception(f"Layer is not valid: {path}")
    QgsProject.instance().addMapLayer(layer)
    return {"id": layer.id(), "name": layer.name(), "type": "raster", "width": layer.width(), "height": layer.height(), "band_count": layer.bandCount()}

def _vector_add_layer_impl(self, path: str, provider: str = "ogr", name: str | None = None) -> dict[str, Any]:
    """
    作用：实现 `_vector_add_layer_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_vector_add_layer_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 参数 `path`（`str`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `provider`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"ogr"`。
    - 参数 `name`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `None`。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    layer = QgsVectorLayer(path, name or Path(path).stem, provider)
    if not layer.isValid():
        raise Exception(f"Layer is not valid: {path}")
    QgsProject.instance().addMapLayer(layer)
    return {"id": layer.id(), "name": layer.name(), "type": self._layer_type_token(layer), "feature_count": layer.featureCount()}

@staticmethod
def _detect_dataset_kind(path: Path) -> str | None:
    """
    作用：封装内部辅助步骤 `_detect_dataset_kind`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_detect_dataset_kind`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `path`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 返回：返回 `str | None` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str | None` 类型结果，返回值语义遵循该函数实现约定。
    """
    vector_exts = {".shp", ".gpkg", ".geojson", ".json", ".kml", ".gml", ".sqlite", ".csv"}
    raster_exts = {".tif", ".tiff", ".img", ".vrt", ".asc", ".jp2", ".png", ".jpg", ".jpeg"}
    suffix = path.suffix.lower()
    if suffix in vector_exts:
        return "vector"
    if suffix in raster_exts:
        return "raster"
    return None

@staticmethod
def _detect_vector_geometry_type(path: Path) -> str:
    """
    作用：封装内部辅助步骤 `_detect_vector_geometry_type`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_detect_vector_geometry_type`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `path`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    layer = QgsVectorLayer(str(path), "__probe__", "ogr")
    if not layer.isValid():
        return "unknown"
    return {0: "point", 1: "line", 2: "polygon"}.get(int(layer.geometryType()), "unknown")

@staticmethod
def _normalize_dataset_kind(dataset_kind: str | None) -> str:
    """
    作用：封装内部辅助步骤 `_normalize_dataset_kind`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_normalize_dataset_kind`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `dataset_kind`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    value = (dataset_kind or "both").strip().lower()
    if value in {"both", "all", "any"}:
        return "both"
    if value in {"vector", "raster"}:
        return value
    raise Exception(f"Invalid dataset_kind: {dataset_kind}")

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
def _normalize_geometry_type_filter(geometry_type: str | None) -> str:
    """
    作用：封装内部辅助步骤 `_normalize_geometry_type_filter`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_normalize_geometry_type_filter`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `geometry_type`（`str | None`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `Exception`。
    """
    value = (geometry_type or "any").strip().lower()
    if value in {"any", "point", "line", "polygon", "unknown"}:
        return value
    raise Exception(f"Invalid geometry_type: {geometry_type}")

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
def _layer_type_token(layer: QgsMapLayer) -> str:
    """
    作用：封装内部辅助步骤 `_layer_type_token`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_layer_type_token`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `layer`（`QgsMapLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    if layer.type() == QgsMapLayer.VectorLayer:
        return f"vector_{int(layer.geometryType())}"
    if layer.type() == QgsMapLayer.RasterLayer:
        return "raster"
    return str(int(layer.type()))

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

TOOL_METHODS: dict[str, object] = {
    'dataset_load_from_directory': dataset_load_from_directory,
    '_dataset_load_from_directory_impl': _dataset_load_from_directory_impl,
    '_dataset_list_files_impl': _dataset_list_files_impl,
    '_raster_add_layer_impl': _raster_add_layer_impl,
    '_vector_add_layer_impl': _vector_add_layer_impl,
    '_detect_dataset_kind': _detect_dataset_kind,
    '_detect_vector_geometry_type': _detect_vector_geometry_type,
    '_normalize_dataset_kind': _normalize_dataset_kind,
    '_normalize_dataset_limit': _normalize_dataset_limit,
    '_normalize_geometry_type_filter': _normalize_geometry_type_filter,
    '_normalize_name_glob': _normalize_name_glob,
    '_layer_type_token': _layer_type_token,
    '_safe_int': _safe_int,
}
