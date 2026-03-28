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

TOOL_NAME = 'dataset_load_from_directory'
TOOL_DOC = '???先扫描目录中的候选数据集，再把匹配的 vector 或 raster 图层批量加载到当前工程。 ?????参数和 dataset_list_files 基本一致，skip_invalid 控制遇到坏数据时是跳过还是整体失败。 ?????目录必须存在，且目录下至少有可识别的数据集文件才有意义。 ??????会向当前工程新增多个图层，但不会改写源数据文件。 ?????skip_invalid=true 时失败项会进入 failed；skip_invalid=false 时第一个失败就终止。 ?????返回 requested_count、loaded_count、failed_count，以及 loaded 和 failed 明细。'

def dataset_load_from_directory(self, directory: str, recursive: bool = False, dataset_kind: str = "both", geometry_type: str = "any", name_glob: str = "*", limit: int = DEFAULT_DATASET_LIMIT, skip_invalid: bool = True) -> dict[str, Any]:
    """Handle datasets from a directory."""
    return self._run(self._dataset_load_from_directory_impl, directory, recursive, dataset_kind, geometry_type, name_glob, limit, skip_invalid)

def _dataset_load_from_directory_impl(self, directory: str, recursive: bool, dataset_kind: str, geometry_type: str, name_glob: str, limit: int, skip_invalid: bool) -> dict[str, Any]:
    """Build the datasets from a directory."""
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
    """Build the files in a dataset directory."""
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
    """Build the raster layer."""
    layer = QgsRasterLayer(path, name or Path(path).stem, provider)
    if not layer.isValid():
        raise Exception(f"Layer is not valid: {path}")
    QgsProject.instance().addMapLayer(layer)
    return {"id": layer.id(), "name": layer.name(), "type": "raster", "width": layer.width(), "height": layer.height(), "band_count": layer.bandCount()}

def _vector_add_layer_impl(self, path: str, provider: str = "ogr", name: str | None = None) -> dict[str, Any]:
    """Build the vector layer."""
    layer = QgsVectorLayer(path, name or Path(path).stem, provider)
    if not layer.isValid():
        raise Exception(f"Layer is not valid: {path}")
    QgsProject.instance().addMapLayer(layer)
    return {"id": layer.id(), "name": layer.name(), "type": self._layer_type_token(layer), "feature_count": layer.featureCount()}

@staticmethod
def _detect_dataset_kind(path: Path) -> str | None:
    """Handle detect dataset kind."""
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
    """Handle detect vector geometry type."""
    layer = QgsVectorLayer(str(path), "__probe__", "ogr")
    if not layer.isValid():
        return "unknown"
    return {0: "point", 1: "line", 2: "polygon"}.get(int(layer.geometryType()), "unknown")

@staticmethod
def _normalize_dataset_kind(dataset_kind: str | None) -> str:
    """Handle normalize dataset kind."""
    value = (dataset_kind or "both").strip().lower()
    if value in {"both", "all", "any"}:
        return "both"
    if value in {"vector", "raster"}:
        return value
    raise Exception(f"Invalid dataset_kind: {dataset_kind}")

def _normalize_dataset_limit(self, limit: Any | None) -> tuple[int, int, bool]:
    """Handle normalize dataset limit."""
    requested = self._safe_int(limit, self.DEFAULT_DATASET_LIMIT)
    normalized = max(0, requested)
    applied = min(normalized, self.MAX_DATASET_LIMIT)
    return requested, applied, applied != normalized

@staticmethod
def _normalize_geometry_type_filter(geometry_type: str | None) -> str:
    """Handle normalize geometry type filter."""
    value = (geometry_type or "any").strip().lower()
    if value in {"any", "point", "line", "polygon", "unknown"}:
        return value
    raise Exception(f"Invalid geometry_type: {geometry_type}")

@staticmethod
def _normalize_name_glob(name_glob: str | None) -> str:
    """Handle normalize name glob."""
    value = (name_glob or "*").strip()
    return value or "*"

@staticmethod
def _layer_type_token(layer: QgsMapLayer) -> str:
    """Handle layer type token."""
    if layer.type() == QgsMapLayer.VectorLayer:
        return f"vector_{int(layer.geometryType())}"
    if layer.type() == QgsMapLayer.RasterLayer:
        return "raster"
    return str(int(layer.type()))

@staticmethod
def _safe_int(value: Any, default: int) -> int:
    """Handle safe int."""
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
