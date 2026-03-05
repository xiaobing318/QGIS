from __future__ import annotations

import json
import platform
import shutil
import sys
from datetime import datetime, timezone
from fnmatch import fnmatch
from pathlib import Path
from typing import Any

import processing
from processing.core.Processing import Processing
from qgis.PyQt.QtCore import QVariant
from qgis.core import (
    Qgis,
    QgsApplication,
    QgsExpression,
    QgsExpressionContext,
    QgsExpressionContextUtils,
    QgsFeature,
    QgsField,
    QgsGeometry,
    QgsLayerTreeGroup,
    QgsLayerTreeLayer,
    QgsMapLayer,
    QgsProcessingParameterDefinition,
    QgsProcessingParameterEnum,
    QgsProject,
    QgsRasterLayer,
    QgsVectorLayer,
)
from qgis.utils import active_plugins

from processingmcpserver.mcp_main_thread_runner import McpMainThreadRunner

REGISTERED_TOOL_NAMES: tuple[str, ...] = (
    "common_get_qgis_info",
    "vector_add_layer",
    "vector_add_layers",
    "raster_add_layer",
    "raster_add_layers",
    "layer_list",
    "layer_get_panel_tree",
    "layer_get_details",
    "layer_remove",
    "layer_remove_batch",
    "layer_resolve_references",
    "vector_get_layer_features",
    "vector_table_add_field",
    "vector_table_drop_fields",
    "vector_table_rename_field",
    "vector_table_calculate_field",
    "vector_table_query_records",
    "vector_table_insert_records",
    "vector_table_update_records",
    "vector_table_delete_records",
    "vector_table_truncate",
    "vector_stats_basic",
    "vector_stats_by_categories",
    "raster_stats_basic",
    "raster_stats_zonal",
    "raster_stats_cell",
    "dataset_list_files",
    "dataset_load_from_directory",
    "filesystem_query_list_entries",
    "filesystem_query_entry_info",
    "filesystem_query_read_text",
    "filesystem_edit_write_text",
    "filesystem_edit_append_text",
    "filesystem_edit_copy_entry",
    "filesystem_edit_move_entry",
    "filesystem_edit_delete_entry",
    "filesystem_stats_directory",
    "processing_list_providers",
    "processing_get_algorithms",
    "processing_get_parameter_template",
    "processing_execute_algorithm",
    "processing_execute_on_layers",
)

_PROCESSING_INITIALIZED = False


def _ensure_processing_initialized() -> None:
    global _PROCESSING_INITIALIZED
    if _PROCESSING_INITIALIZED:
        return
    Processing.initialize()
    _PROCESSING_INITIALIZED = True


class ProcessingMCPTools:
    DEFAULT_FEATURE_LIMIT = 10
    MAX_FEATURE_LIMIT = 2000
    DEFAULT_DATASET_LIMIT = 100
    MAX_DATASET_LIMIT = 1000
    DEFAULT_FILESYSTEM_LIST_LIMIT = 100
    MAX_FILESYSTEM_LIST_LIMIT = 2000
    DEFAULT_ALGORITHM_LIST_LIMIT = 100
    MAX_ALGORITHM_LIST_LIMIT = 500

    def __init__(self, iface, runner: McpMainThreadRunner):
        self._iface = iface
        self._runner = runner

    def _run(self, func, *args, **kwargs):
        return self._runner.run(lambda: func(*args, **kwargs))

    @staticmethod
    def _safe_int(value: Any, default: int) -> int:
        try:
            return int(value)
        except (TypeError, ValueError):
            return default

    def _normalize_feature_limit(self, limit: Any) -> tuple[int, int, bool]:
        requested = self._safe_int(limit, self.DEFAULT_FEATURE_LIMIT)
        normalized = max(0, requested)
        applied = min(normalized, self.MAX_FEATURE_LIMIT)
        return requested, applied, applied != normalized

    def _normalize_algorithm_list_limit(self, limit: Any | None) -> tuple[int | None, int, bool]:
        requested = None if limit is None else self._safe_int(limit, self.DEFAULT_ALGORITHM_LIST_LIMIT)
        normalized = self.DEFAULT_ALGORITHM_LIST_LIMIT if requested is None else max(0, requested)
        applied = min(normalized, self.MAX_ALGORITHM_LIST_LIMIT)
        capped = requested is not None and normalized != applied
        return requested, applied, capped

    def _normalize_dataset_limit(self, limit: Any | None) -> tuple[int, int, bool]:
        requested = self._safe_int(limit, self.DEFAULT_DATASET_LIMIT)
        normalized = max(0, requested)
        applied = min(normalized, self.MAX_DATASET_LIMIT)
        return requested, applied, applied != normalized

    def _normalize_filesystem_limit(self, limit: Any | None) -> tuple[int, int, bool]:
        requested = self._safe_int(limit, self.DEFAULT_FILESYSTEM_LIST_LIMIT)
        normalized = max(0, requested)
        applied = min(normalized, self.MAX_FILESYSTEM_LIST_LIMIT)
        return requested, applied, applied != normalized

    @staticmethod
    def _normalize_dataset_kind(dataset_kind: str | None) -> str:
        value = (dataset_kind or "both").strip().lower()
        if value in {"both", "all", "any"}:
            return "both"
        if value in {"vector", "raster"}:
            return value
        raise Exception(f"Invalid dataset_kind: {dataset_kind}")

    @staticmethod
    def _normalize_geometry_type_filter(geometry_type: str | None) -> str:
        value = (geometry_type or "any").strip().lower()
        if value in {"any", "point", "line", "polygon", "unknown"}:
            return value
        raise Exception(f"Invalid geometry_type: {geometry_type}")

    @staticmethod
    def _normalize_layer_types_filter(layer_types: str | None) -> str:
        value = (layer_types or "both").strip().lower()
        if value in {"both", "all", "any"}:
            return "both"
        if value in {"vector", "raster"}:
            return value
        raise Exception(f"Invalid layer_types: {layer_types}")

    @staticmethod
    def _normalize_name_glob(name_glob: str | None) -> str:
        value = (name_glob or "*").strip()
        return value or "*"

    @staticmethod
    def _resource_json(payload: Any) -> str:
        return json.dumps(payload, ensure_ascii=False, indent=2)

    @staticmethod
    def _layer_type_token(layer: QgsMapLayer) -> str:
        if layer.type() == QgsMapLayer.VectorLayer:
            return f"vector_{int(layer.geometryType())}"
        if layer.type() == QgsMapLayer.RasterLayer:
            return "raster"
        return str(int(layer.type()))

    @staticmethod
    def _is_layer_visible(project: QgsProject, layer_id: str) -> bool:
        node = project.layerTreeRoot().findLayer(layer_id) if project.layerTreeRoot() else None
        if node is None:
            return False
        try:
            return bool(node.isVisible())
        except Exception:
            return False

    @staticmethod
    def _serialize_value(value: Any) -> Any:
        if value is None:
            return None
        if isinstance(value, (bool, int, float, str)):
            return value
        if isinstance(value, Path):
            return str(value)
        if isinstance(value, list):
            return [ProcessingMCPTools._serialize_value(item) for item in value]
        if isinstance(value, dict):
            return {str(k): ProcessingMCPTools._serialize_value(v) for k, v in value.items()}
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
    def _safe_call(obj: object, name: str, default: Any = None) -> Any:
        attr = getattr(obj, name, None)
        if callable(attr):
            try:
                return attr()
            except Exception:
                return default
        return attr if attr is not None else default

    def _serialize_parameter(self, param) -> dict[str, Any]:
        flags = self._safe_call(param, "flags", 0) or 0
        result = {
            "name": self._safe_call(param, "name"),
            "description": self._safe_call(param, "description"),
            "type": self._safe_call(param, "type"),
            "type_name": self._safe_call(param, "typeName"),
            "default": self._serialize_value(self._safe_call(param, "defaultValue")),
            "optional": bool(flags & QgsProcessingParameterDefinition.Flag.FlagOptional),
            "hidden": bool(flags & QgsProcessingParameterDefinition.Flag.FlagHidden),
        }
        if isinstance(param, QgsProcessingParameterEnum):
            result["options"] = list(param.options())
        if hasattr(param, "allowMultiple"):
            result["allow_multiple"] = bool(self._safe_call(param, "allowMultiple", False))
        return result

    def _serialize_output(self, output_def) -> dict[str, Any]:
        return {
            "name": self._safe_call(output_def, "name"),
            "description": self._safe_call(output_def, "description"),
            "type": self._safe_call(output_def, "type"),
            "type_name": self._safe_call(output_def, "typeName"),
        }

    def _serialize_algorithm(self, alg, include_parameters: bool, include_outputs: bool) -> dict[str, Any]:
        provider = alg.provider() if hasattr(alg, "provider") else None
        result = {
            "id": alg.id(),
            "name": alg.displayName(),
            "group": alg.group(),
            "group_id": alg.groupId(),
            "provider": provider.name() if provider else None,
            "provider_id": provider.id() if provider else None,
            "parameters_count": len(alg.parameterDefinitions()),
            "outputs_count": len(alg.outputDefinitions()),
        }
        if include_parameters:
            result["parameters"] = [self._serialize_parameter(p) for p in alg.parameterDefinitions()]
        if include_outputs:
            result["outputs"] = [self._serialize_output(o) for o in alg.outputDefinitions()]
        return result

    @staticmethod
    def _is_disk_output_key(key: str) -> bool:
        return "OUTPUT" in key.upper()

    @staticmethod
    def _is_disk_output_value(value: Any) -> bool:
        if value is None:
            return False
        if isinstance(value, Path):
            return True
        if not isinstance(value, str):
            return False
        text = value.strip()
        if not text:
            return False
        if text.upper() in {"TEMPORARY_OUTPUT", "TEMPORARY", "MEMORY:", "MEMORY"}:
            return False
        if text.startswith("memory:"):
            return False
        return True

    def _sanitize_processing_parameters(self, parameters: dict[str, Any], allow_disk_write: bool, allow_in_place_edit: bool) -> tuple[dict[str, Any], list[str]]:
        sanitized = dict(parameters)
        warnings: list[str] = []
        for key in list(sanitized.keys()):
            upper = str(key).upper()
            value = sanitized[key]
            if upper == "IN_PLACE" and bool(value) and not allow_in_place_edit:
                sanitized[key] = False
                warnings.append("IN_PLACE was forced to false by safety policy.")
            if self._is_disk_output_key(str(key)) and not allow_disk_write and self._is_disk_output_value(value):
                sanitized[key] = "TEMPORARY_OUTPUT"
                warnings.append(f"{key} was rewritten to TEMPORARY_OUTPUT by safety policy.")
        return sanitized, warnings

    @staticmethod
    def _coerce_output_identifier(value: Any) -> str:
        if value is None:
            return ""
        if isinstance(value, str):
            return value
        if hasattr(value, "id") and callable(value.id):
            try:
                return str(value.id())
            except Exception:
                pass
        return str(value)

    @staticmethod
    def _to_field_type(field_type: str) -> QVariant.Type:
        value = (field_type or "string").strip().lower()
        mapping = {
            "string": QVariant.String,
            "str": QVariant.String,
            "int": QVariant.Int,
            "integer": QVariant.Int,
            "double": QVariant.Double,
            "float": QVariant.Double,
            "real": QVariant.Double,
            "bool": QVariant.Bool,
            "boolean": QVariant.Bool,
        }
        if value not in mapping:
            raise Exception(f"Unsupported field_type: {field_type}")
        return mapping[value]

    @staticmethod
    def _ok_result(tool: str, summary: dict[str, Any] | None = None, outputs: dict[str, Any] | None = None, warnings: list[str] | None = None, **extra) -> dict[str, Any]:
        payload: dict[str, Any] = {"ok": True, "tool": tool, "summary": summary or {}, "outputs": outputs or {}}
        if warnings is not None:
            payload["warnings"] = warnings
        payload.update(extra)
        return payload

    @staticmethod
    def _path_info(path: Path) -> dict[str, Any]:
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

    @staticmethod
    def _field_index(layer: QgsVectorLayer, field_name: str) -> int:
        return layer.fields().indexFromName(field_name)

    def _resolve_layer_ref(self, layer_ref: Any) -> QgsMapLayer:
        if isinstance(layer_ref, QgsMapLayer):
            return layer_ref
        text = str(layer_ref).strip() if layer_ref is not None else ""
        if not text:
            raise Exception("Layer reference is required")
        layers = QgsProject.instance().mapLayers()
        if text in layers:
            return layers[text]
        matches = [layer for layer in layers.values() if layer.name() == text]
        if matches:
            return matches[0]
        raise Exception(f"Layer not found: {text}")

    def _resolve_vector_layer_ref(self, layer_ref: Any) -> QgsVectorLayer:
        layer = self._resolve_layer_ref(layer_ref)
        if layer.type() != QgsMapLayer.VectorLayer:
            raise Exception(f"Layer is not a vector layer: {layer_ref}")
        return layer

    def _resolve_raster_layer_ref(self, layer_ref: Any) -> QgsRasterLayer:
        layer = self._resolve_layer_ref(layer_ref)
        if layer.type() != QgsMapLayer.RasterLayer:
            raise Exception(f"Layer is not a raster layer: {layer_ref}")
        return layer

    @staticmethod
    def _begin_vector_edit(layer: QgsVectorLayer) -> bool:
        if layer.isEditable():
            return False
        if not layer.startEditing():
            raise Exception("Failed to start editing vector layer")
        return True

    @staticmethod
    def _finish_vector_edit(layer: QgsVectorLayer, started_here: bool) -> None:
        if not started_here:
            return
        if layer.commitChanges():
            return
        errors = "; ".join(layer.commitErrors()) if hasattr(layer, "commitErrors") else ""
        layer.rollBack()
        raise Exception(f"Failed to commit vector layer changes: {errors}")

    def _apply_vector_edit(self, layer: QgsVectorLayer, operation) -> Any:
        started_here = self._begin_vector_edit(layer)
        try:
            result = operation()
        except Exception:
            if started_here:
                layer.rollBack()
            raise
        self._finish_vector_edit(layer, started_here)
        return result

    def _ensure_processing_runtime(self) -> None:
        _ensure_processing_initialized()

    def _execute_processing_call(self, algorithm: str, parameters: dict[str, Any], load_results: bool) -> dict[str, Any]:
        self._ensure_processing_runtime()
        result = processing.runAndLoadResults(algorithm, parameters) if load_results else processing.run(algorithm, parameters)
        return result if isinstance(result, dict) else {"result": result}

    def get_project_snapshot(self) -> dict[str, Any]:
        return self._run(self._get_project_snapshot_impl)

    def _get_project_snapshot_impl(self) -> dict[str, Any]:
        project = QgsProject.instance()
        return {
            "file_name": project.fileName(),
            "title": project.title(),
            "crs": project.crs().authid() if project.crs().isValid() else "",
            "layer_count": len(project.mapLayers()),
        }

    def get_layers_summary(self) -> dict[str, Any]:
        return self._run(self._get_layers_summary_impl)

    def _get_layers_summary_impl(self) -> dict[str, Any]:
        layers = self.layer_list(layer_types="both")
        return {
            "count": len(layers),
            "counts": {
                "vector": len([x for x in layers if str(x["type"]).startswith("vector_")]),
                "raster": len([x for x in layers if x["type"] == "raster"]),
                "other": len([x for x in layers if not str(x["type"]).startswith("vector_") and x["type"] != "raster"]),
            },
            "layers": layers,
        }

    def common_get_qgis_info(self) -> dict[str, Any]:
        return self._run(self._common_get_qgis_info_impl)

    def _common_get_qgis_info_impl(self) -> dict[str, Any]:
        project = QgsProject.instance()
        available = False
        try:
            self._ensure_processing_runtime()
            available = True
        except Exception:
            available = False
        return {
            "qgis": {
                "version": Qgis.version(),
                "prefix_path": QgsApplication.prefixPath(),
                "settings_dir": QgsApplication.qgisSettingsDirPath(),
            },
            "python": {
                "version": platform.python_version(),
                "executable": sys.executable,
            },
            "project": {
                "file_name": project.fileName(),
                "title": project.title(),
                "layer_count": len(project.mapLayers()),
            },
            "processing_mcp": {
                "available": available,
                "registered_tools_count": len(REGISTERED_TOOL_NAMES),
                "active_plugins_count": len(active_plugins),
            },
        }

    def vector_add_layer(self, path: str, provider: str = "ogr", name: str | None = None) -> dict[str, Any]:
        return self._run(self._vector_add_layer_impl, path, provider, name)

    def _vector_add_layer_impl(self, path: str, provider: str = "ogr", name: str | None = None) -> dict[str, Any]:
        layer = QgsVectorLayer(path, name or Path(path).stem, provider)
        if not layer.isValid():
            raise Exception(f"Layer is not valid: {path}")
        QgsProject.instance().addMapLayer(layer)
        return {"id": layer.id(), "name": layer.name(), "type": self._layer_type_token(layer), "feature_count": layer.featureCount()}

    def vector_add_layers(self, paths: list[str], provider: str = "ogr", skip_invalid: bool = True) -> dict[str, Any]:
        return self._run(self._vector_add_layers_impl, paths, provider, skip_invalid)

    def _vector_add_layers_impl(self, paths: list[str], provider: str, skip_invalid: bool) -> dict[str, Any]:
        loaded: list[dict[str, Any]] = []
        failed: list[dict[str, Any]] = []
        for path in paths or []:
            try:
                loaded.append(self._vector_add_layer_impl(path, provider=provider))
            except Exception as exc:
                if not skip_invalid:
                    raise Exception(f"Failed to load vector dataset: {path}. {exc}") from exc
                failed.append({"path": path, "error": str(exc)})
        return {"requested_count": len(paths or []), "loaded_count": len(loaded), "failed_count": len(failed), "loaded": loaded, "failed": failed}

    def raster_add_layer(self, path: str, provider: str = "gdal", name: str | None = None) -> dict[str, Any]:
        return self._run(self._raster_add_layer_impl, path, provider, name)

    def _raster_add_layer_impl(self, path: str, provider: str = "gdal", name: str | None = None) -> dict[str, Any]:
        layer = QgsRasterLayer(path, name or Path(path).stem, provider)
        if not layer.isValid():
            raise Exception(f"Layer is not valid: {path}")
        QgsProject.instance().addMapLayer(layer)
        return {"id": layer.id(), "name": layer.name(), "type": "raster", "width": layer.width(), "height": layer.height(), "band_count": layer.bandCount()}

    def raster_add_layers(self, paths: list[str], provider: str = "gdal", skip_invalid: bool = True) -> dict[str, Any]:
        return self._run(self._raster_add_layers_impl, paths, provider, skip_invalid)

    def _raster_add_layers_impl(self, paths: list[str], provider: str, skip_invalid: bool) -> dict[str, Any]:
        loaded: list[dict[str, Any]] = []
        failed: list[dict[str, Any]] = []
        for path in paths or []:
            try:
                loaded.append(self._raster_add_layer_impl(path, provider=provider))
            except Exception as exc:
                if not skip_invalid:
                    raise Exception(f"Failed to load raster dataset: {path}. {exc}") from exc
                failed.append({"path": path, "error": str(exc)})
        return {"requested_count": len(paths or []), "loaded_count": len(loaded), "failed_count": len(failed), "loaded": loaded, "failed": failed}

    def layer_list(self, layer_types: str = "both", include_hidden: bool = True, name_glob: str = "*") -> list[dict[str, Any]]:
        return self._run(self._layer_list_impl, layer_types, include_hidden, name_glob)

    def _layer_list_impl(self, layer_types: str, include_hidden: bool, name_glob: str) -> list[dict[str, Any]]:
        normalized_types = self._normalize_layer_types_filter(layer_types)
        pattern = self._normalize_name_glob(name_glob)
        project = QgsProject.instance()
        entries: list[dict[str, Any]] = []
        for layer_id, layer in project.mapLayers().items():
            is_vector = layer.type() == QgsMapLayer.VectorLayer
            is_raster = layer.type() == QgsMapLayer.RasterLayer
            if normalized_types == "vector" and not is_vector:
                continue
            if normalized_types == "raster" and not is_raster:
                continue
            if not fnmatch(layer.name(), pattern):
                continue
            visible = self._is_layer_visible(project, layer_id)
            if not include_hidden and not visible:
                continue
            item: dict[str, Any] = {
                "id": layer_id,
                "name": layer.name(),
                "type": self._layer_type_token(layer),
                "visible": visible,
                "provider": layer.providerType() if hasattr(layer, "providerType") else "",
            }
            if is_vector:
                item["feature_count"] = layer.featureCount()
                item["fields"] = [f.name() for f in layer.fields()]
            elif is_raster:
                item["width"] = layer.width()
                item["height"] = layer.height()
                item["band_count"] = layer.bandCount()
            entries.append(item)
        return entries

    def layer_get_panel_tree(self, include_hidden: bool = True) -> dict[str, Any]:
        return self._run(self._layer_get_panel_tree_impl, include_hidden)

    def _layer_get_panel_tree_impl(self, include_hidden: bool) -> dict[str, Any]:
        root = QgsProject.instance().layerTreeRoot()
        groups: list[dict[str, Any]] = []

        def walk(group: QgsLayerTreeGroup, prefix: str) -> None:
            for child in group.children():
                if isinstance(child, QgsLayerTreeGroup):
                    path = f"{prefix}/{child.name()}" if prefix else child.name()
                    if include_hidden or child.isVisible():
                        groups.append({"name": child.name(), "path": path, "visible": bool(child.isVisible())})
                    walk(child, path)
                elif isinstance(child, QgsLayerTreeLayer):
                    continue

        if root is not None:
            walk(root, "")
        return {"groups": groups, "layers": self._layer_list_impl("both", include_hidden, "*")}

    def layer_get_details(self, layer_ref: str) -> dict[str, Any]:
        return self._run(self._layer_get_details_impl, layer_ref)

    def _layer_get_details_impl(self, layer_ref: str) -> dict[str, Any]:
        layer = self._resolve_layer_ref(layer_ref)
        result: dict[str, Any] = {
            "id": layer.id(),
            "name": layer.name(),
            "type": self._layer_type_token(layer),
            "provider": layer.providerType() if hasattr(layer, "providerType") else "",
            "source": layer.source() if hasattr(layer, "source") else "",
            "crs": layer.crs().authid() if hasattr(layer, "crs") and layer.crs().isValid() else "",
        }
        if layer.type() == QgsMapLayer.VectorLayer:
            result["feature_count"] = layer.featureCount()
            result["fields"] = [f.name() for f in layer.fields()]
        elif layer.type() == QgsMapLayer.RasterLayer:
            result["width"] = layer.width()
            result["height"] = layer.height()
            result["band_count"] = layer.bandCount()
        return result

    def layer_remove(self, layer_id: str) -> dict[str, str]:
        return self._run(self._layer_remove_impl, layer_id)

    def _layer_remove_impl(self, layer_id: str) -> dict[str, str]:
        project = QgsProject.instance()
        if layer_id not in project.mapLayers():
            raise Exception(f"Layer not found: {layer_id}")
        project.removeMapLayer(layer_id)
        return {"removed": layer_id}

    def layer_remove_batch(self, layer_ids: list[str]) -> dict[str, list[str]]:
        return self._run(self._layer_remove_batch_impl, layer_ids)

    def _layer_remove_batch_impl(self, layer_ids: list[str]) -> dict[str, list[str]]:
        project = QgsProject.instance()
        removed: list[str] = []
        missing: list[str] = []
        for layer_id in layer_ids or []:
            value = str(layer_id).strip()
            if not value:
                continue
            if value in project.mapLayers():
                project.removeMapLayer(value)
                removed.append(value)
            else:
                missing.append(value)
        return {"removed": removed, "missing": missing}

    def layer_resolve_references(self, refs: list[str], strict: bool = False) -> dict[str, Any]:
        return self._run(self._layer_resolve_references_impl, refs, strict)

    def _layer_resolve_references_impl(self, refs: list[str], strict: bool) -> dict[str, Any]:
        resolved: dict[str, str] = {}
        missing: list[str] = []
        for ref in refs or []:
            text = str(ref).strip()
            if not text:
                continue
            try:
                resolved[text] = self._resolve_layer_ref(text).id()
            except Exception:
                missing.append(text)
        if strict and missing:
            raise Exception(f"Layer reference resolve failed: {missing}")
        return {"resolved": resolved, "missing": missing}

    def vector_get_layer_features(self, layer_ref: str, limit: int = DEFAULT_FEATURE_LIMIT) -> dict[str, Any]:
        return self._run(self._vector_get_layer_features_impl, layer_ref, limit)

    def _vector_get_layer_features_impl(self, layer_ref: str, limit: int) -> dict[str, Any]:
        layer = self._resolve_vector_layer_ref(layer_ref)
        requested, applied, capped = self._normalize_feature_limit(limit)
        features: list[dict[str, Any]] = []
        for index, feature in enumerate(layer.getFeatures()):
            if index >= applied:
                break
            attrs = {f.name(): feature.attribute(f.name()) for f in layer.fields()}
            features.append(
                {
                    "id": feature.id(),
                    "attributes": attrs,
                    "geometry_wkt": feature.geometry().asWkt(precision=6) if feature.hasGeometry() else None,
                }
            )
        return {
            "layer_id": layer.id(),
            "feature_count": layer.featureCount(),
            "fields": [f.name() for f in layer.fields()],
            "features": features,
            "requested_limit": requested,
            "applied_limit": applied,
            "limit_capped": capped,
            "max_feature_limit": self.MAX_FEATURE_LIMIT,
        }

    def vector_table_add_field(self, layer_ref: str, field_name: str, field_type: str = "string", field_length: int = 0, field_precision: int = 0, in_place: bool = False) -> dict[str, Any]:
        return self._run(self._vector_table_add_field_impl, layer_ref, field_name, field_type, field_length, field_precision, in_place)

    def _vector_table_add_field_impl(self, layer_ref: str, field_name: str, field_type: str, field_length: int, field_precision: int, in_place: bool) -> dict[str, Any]:
        layer = self._resolve_vector_layer_ref(layer_ref)
        name = field_name.strip()
        if not name:
            raise Exception("field_name is required")
        if self._field_index(layer, name) >= 0:
            raise Exception(f"Field already exists: {name}")
        field = QgsField(name, self._to_field_type(field_type), len=max(0, self._safe_int(field_length, 0)), prec=max(0, self._safe_int(field_precision, 0)))

        def op() -> int:
            if not layer.addAttribute(field):
                raise Exception(f"Failed to add field: {name}")
            layer.updateFields()
            return 1

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result("vector_table_add_field", summary={"mode": "in_place" if in_place else "copy", "affected_count": affected, "output_layer_id": layer.id()}, outputs={"fields": [f.name() for f in layer.fields()]})

    def vector_table_drop_fields(self, layer_ref: str, fields: list[str], in_place: bool = False) -> dict[str, Any]:
        return self._run(self._vector_table_drop_fields_impl, layer_ref, fields, in_place)

    def _vector_table_drop_fields_impl(self, layer_ref: str, fields: list[str], in_place: bool) -> dict[str, Any]:
        layer = self._resolve_vector_layer_ref(layer_ref)
        if not fields:
            raise Exception("fields is required")
        indexes: list[int] = []
        missing: list[str] = []
        for field_name in fields:
            idx = self._field_index(layer, field_name)
            if idx < 0:
                missing.append(field_name)
            else:
                indexes.append(idx)

        def op() -> int:
            affected = 0
            for idx in sorted(indexes, reverse=True):
                if layer.deleteAttribute(idx):
                    affected += 1
            layer.updateFields()
            return affected

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result("vector_table_drop_fields", summary={"mode": "in_place" if in_place else "copy", "affected_count": affected, "output_layer_id": layer.id()}, outputs={"fields": [f.name() for f in layer.fields()], "missing_fields": missing})

    def vector_table_rename_field(self, layer_ref: str, field_name: str, new_field_name: str, in_place: bool = False) -> dict[str, Any]:
        return self._run(self._vector_table_rename_field_impl, layer_ref, field_name, new_field_name, in_place)

    def _vector_table_rename_field_impl(self, layer_ref: str, field_name: str, new_field_name: str, in_place: bool) -> dict[str, Any]:
        layer = self._resolve_vector_layer_ref(layer_ref)
        old_name = field_name.strip()
        new_name = new_field_name.strip()
        idx = self._field_index(layer, old_name)
        if idx < 0:
            raise Exception(f"Field not found: {old_name}")
        if self._field_index(layer, new_name) >= 0:
            raise Exception(f"Field already exists: {new_name}")

        def op() -> int:
            if not layer.renameAttribute(idx, new_name):
                raise Exception(f"Failed to rename field {old_name} -> {new_name}")
            layer.updateFields()
            return 1

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result("vector_table_rename_field", summary={"mode": "in_place" if in_place else "copy", "affected_count": affected, "output_layer_id": layer.id()}, outputs={"fields": [f.name() for f in layer.fields()]})

    def vector_table_calculate_field(self, layer_ref: str, field_name: str, expression: str, field_type: str = "string", where: str | None = None, in_place: bool = False) -> dict[str, Any]:
        return self._run(self._vector_table_calculate_field_impl, layer_ref, field_name, expression, field_type, where, in_place)

    def _vector_table_calculate_field_impl(self, layer_ref: str, field_name: str, expression: str, field_type: str, where: str | None, in_place: bool) -> dict[str, Any]:
        layer = self._resolve_vector_layer_ref(layer_ref)
        expr = QgsExpression(expression or "")
        if expr.hasParserError():
            raise Exception(f"Invalid expression: {expr.parserErrorString()}")
        where_expr = None
        if where:
            where_expr = QgsExpression(where)
            if where_expr.hasParserError():
                raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

        idx = self._field_index(layer, field_name)
        created = False

        def op() -> int:
            nonlocal idx
            nonlocal created
            if idx < 0:
                if not layer.addAttribute(QgsField(field_name, self._to_field_type(field_type))):
                    raise Exception(f"Failed to create field: {field_name}")
                layer.updateFields()
                idx = self._field_index(layer, field_name)
                created = True
            context = QgsExpressionContext()
            context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
            affected = 0
            for feature in layer.getFeatures():
                context.setFeature(feature)
                if where_expr is not None:
                    keep = where_expr.evaluate(context)
                    if where_expr.hasEvalError():
                        raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
                    if not bool(keep):
                        continue
                value = expr.evaluate(context)
                if expr.hasEvalError():
                    raise Exception(f"Invalid expression: {expr.evalErrorString()}")
                if not layer.changeAttributeValue(feature.id(), idx, value):
                    raise Exception(f"Failed to update feature: {feature.id()}")
                affected += 1
            return affected

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result("vector_table_calculate_field", summary={"mode": "in_place" if in_place else "copy", "affected_count": affected, "field_created": created, "output_layer_id": layer.id()}, outputs={"fields": [f.name() for f in layer.fields()]})

    def vector_table_query_records(self, layer_ref: str, where: str | None = None, fields: list[str] | None = None, limit: int = DEFAULT_FEATURE_LIMIT, offset: int = 0, order_by: str | None = None, include_geometry: bool = False) -> dict[str, Any]:
        return self._run(self._vector_table_query_records_impl, layer_ref, where, fields, limit, offset, order_by, include_geometry)

    def _vector_table_query_records_impl(self, layer_ref: str, where: str | None, fields: list[str] | None, limit: int, offset: int, order_by: str | None, include_geometry: bool) -> dict[str, Any]:
        if not str(layer_ref).strip():
            raise Exception("layer_ref is required")
        layer = self._resolve_vector_layer_ref(layer_ref)
        where_expr = None
        if where:
            where_expr = QgsExpression(where)
            if where_expr.hasParserError():
                raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

        requested, applied, capped = self._normalize_feature_limit(limit)
        applied_offset = max(0, self._safe_int(offset, 0))
        all_fields = [f.name() for f in layer.fields()]
        selected_fields = all_fields if not fields else fields
        for field_name in selected_fields:
            if self._field_index(layer, field_name) < 0:
                raise Exception(f"Field not found: {field_name}")

        context = QgsExpressionContext()
        context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
        matched: list[QgsFeature] = []
        for feature in layer.getFeatures():
            if where_expr is not None:
                context.setFeature(feature)
                keep = where_expr.evaluate(context)
                if where_expr.hasEvalError():
                    raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
                if not bool(keep):
                    continue
            matched.append(feature)

        if order_by:
            matched = sorted(matched, key=lambda f: str(f.attribute(order_by)))
        sliced = matched[applied_offset : applied_offset + applied]
        records: list[dict[str, Any]] = []
        for feature in sliced:
            item = {"id": feature.id(), "attributes": {name: feature.attribute(name) for name in selected_fields}}
            if include_geometry and feature.hasGeometry():
                item["geometry_wkt"] = feature.geometry().asWkt(precision=6)
            records.append(item)

        return self._ok_result("vector_table_query_records", summary={"returned": len(records), "matched_total": len(matched), "offset": applied_offset, "requested_limit": requested, "applied_limit": applied, "limit_capped": capped}, outputs={"records": records})

    def vector_table_insert_records(self, layer_ref: str, records: list[dict[str, Any]], in_place: bool = False) -> dict[str, Any]:
        return self._run(self._vector_table_insert_records_impl, layer_ref, records, in_place)

    def _vector_table_insert_records_impl(self, layer_ref: str, records: list[dict[str, Any]], in_place: bool) -> dict[str, Any]:
        layer = self._resolve_vector_layer_ref(layer_ref)
        if not records:
            raise Exception("records is required")

        def op() -> int:
            affected = 0
            for row in records:
                feature = QgsFeature(layer.fields())
                attrs = dict(row)
                geometry_wkt = attrs.pop("geometry_wkt", None)
                feature.setAttributes([attrs.get(f.name()) for f in layer.fields()])
                if geometry_wkt:
                    geometry = QgsGeometry.fromWkt(str(geometry_wkt))
                    if geometry.isNull():
                        raise Exception(f"Invalid geometry_wkt: {geometry_wkt}")
                    feature.setGeometry(geometry)
                if not layer.addFeature(feature):
                    raise Exception("Failed to insert feature")
                affected += 1
            return affected

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result("vector_table_insert_records", summary={"mode": "in_place" if in_place else "copy", "affected_count": affected, "output_layer_id": layer.id()}, outputs={"feature_count": layer.featureCount()})

    def vector_table_update_records(self, layer_ref: str, where: str | None = None, set_literals: dict[str, Any] | None = None, set_expressions: dict[str, str] | None = None, in_place: bool = False) -> dict[str, Any]:
        return self._run(self._vector_table_update_records_impl, layer_ref, where, set_literals, set_expressions, in_place)

    def _vector_table_update_records_impl(self, layer_ref: str, where: str | None, set_literals: dict[str, Any] | None, set_expressions: dict[str, str] | None, in_place: bool) -> dict[str, Any]:
        layer = self._resolve_vector_layer_ref(layer_ref)
        literals = set_literals or {}
        expressions = set_expressions or {}
        if not literals and not expressions:
            raise Exception("Either set_literals or set_expressions must be provided")

        where_expr = None
        if where:
            where_expr = QgsExpression(where)
            if where_expr.hasParserError():
                raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

        compiled: dict[str, QgsExpression] = {}
        for key, expr_text in expressions.items():
            compiled_expr = QgsExpression(expr_text)
            if compiled_expr.hasParserError():
                raise Exception(f"Invalid expression for {key}: {compiled_expr.parserErrorString()}")
            compiled[key] = compiled_expr

        field_indexes: dict[str, int] = {}
        for key in set(literals.keys()) | set(expressions.keys()):
            idx = self._field_index(layer, key)
            if idx < 0:
                raise Exception(f"Field not found: {key}")
            field_indexes[key] = idx

        def op() -> int:
            context = QgsExpressionContext()
            context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
            affected = 0
            for feature in layer.getFeatures():
                context.setFeature(feature)
                if where_expr is not None:
                    keep = where_expr.evaluate(context)
                    if where_expr.hasEvalError():
                        raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
                    if not bool(keep):
                        continue
                for key, value in literals.items():
                    if not layer.changeAttributeValue(feature.id(), field_indexes[key], value):
                        raise Exception(f"Failed to update field {key}")
                for key, compiled_expr in compiled.items():
                    value = compiled_expr.evaluate(context)
                    if compiled_expr.hasEvalError():
                        raise Exception(f"Invalid expression for {key}: {compiled_expr.evalErrorString()}")
                    if not layer.changeAttributeValue(feature.id(), field_indexes[key], value):
                        raise Exception(f"Failed to update field {key}")
                affected += 1
            return affected

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result("vector_table_update_records", summary={"mode": "in_place" if in_place else "copy", "affected_count": affected, "output_layer_id": layer.id()}, outputs={"feature_count": layer.featureCount()})

    def vector_table_delete_records(self, layer_ref: str, where: str | None = None, in_place: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
        return self._run(self._vector_table_delete_records_impl, layer_ref, where, in_place, confirm_destructive)

    def _vector_table_delete_records_impl(self, layer_ref: str, where: str | None, in_place: bool, confirm_destructive: bool) -> dict[str, Any]:
        if not confirm_destructive:
            raise Exception("confirm_destructive must be true for delete operation")
        layer = self._resolve_vector_layer_ref(layer_ref)
        where_expr = None
        if where:
            where_expr = QgsExpression(where)
            if where_expr.hasParserError():
                raise Exception(f"Invalid where expression: {where_expr.parserErrorString()}")

        def op() -> int:
            context = QgsExpressionContext()
            context.appendScopes(QgsExpressionContextUtils.globalProjectLayerScopes(layer))
            fids: list[int] = []
            for feature in layer.getFeatures():
                if where_expr is not None:
                    context.setFeature(feature)
                    keep = where_expr.evaluate(context)
                    if where_expr.hasEvalError():
                        raise Exception(f"Invalid where expression: {where_expr.evalErrorString()}")
                    if not bool(keep):
                        continue
                fids.append(int(feature.id()))
            if not fids:
                return 0
            if not layer.deleteFeatures(fids):
                raise Exception("Failed to delete records")
            return len(fids)

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result("vector_table_delete_records", summary={"mode": "in_place" if in_place else "copy", "affected_count": affected, "output_layer_id": layer.id()}, outputs={"feature_count": layer.featureCount()})

    def vector_table_truncate(self, layer_ref: str, in_place: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
        return self._run(self._vector_table_truncate_impl, layer_ref, in_place, confirm_destructive)

    def _vector_table_truncate_impl(self, layer_ref: str, in_place: bool, confirm_destructive: bool) -> dict[str, Any]:
        if not confirm_destructive:
            raise Exception("confirm_destructive must be true for truncate operation")
        layer = self._resolve_vector_layer_ref(layer_ref)

        def op() -> int:
            fids = [int(feature.id()) for feature in layer.getFeatures()]
            if not fids:
                return 0
            if not layer.deleteFeatures(fids):
                raise Exception("Failed to truncate records")
            return len(fids)

        affected = self._apply_vector_edit(layer, op)
        return self._ok_result("vector_table_truncate", summary={"mode": "in_place" if in_place else "copy", "affected_count": affected, "output_layer_id": layer.id()}, outputs={"feature_count": layer.featureCount()})

    def vector_stats_basic(self, layer_ref: str, field_name: str) -> dict[str, Any]:
        return self._run(self._vector_stats_basic_impl, layer_ref, field_name)

    def _vector_stats_basic_impl(self, layer_ref: str, field_name: str) -> dict[str, Any]:
        layer = self._resolve_vector_layer_ref(layer_ref)
        if self._field_index(layer, field_name) < 0:
            raise Exception(f"Invalid field: {field_name}")
        values: list[float] = []
        for feature in layer.getFeatures():
            value = feature.attribute(field_name)
            if value is None:
                continue
            try:
                values.append(float(value))
            except (TypeError, ValueError):
                continue
        if not values:
            raise Exception(f"No numeric values found for field: {field_name}")
        return self._ok_result("vector_stats_basic", summary={"count": len(values), "min": min(values), "max": max(values), "sum": sum(values), "mean": sum(values) / len(values)}, outputs={"field_name": field_name})

    def vector_stats_by_categories(self, layer_ref: str, category_fields: list[str], values_field: str | None = None) -> dict[str, Any]:
        return self._run(self._vector_stats_by_categories_impl, layer_ref, category_fields, values_field)

    def _vector_stats_by_categories_impl(self, layer_ref: str, category_fields: list[str], values_field: str | None) -> dict[str, Any]:
        layer = self._resolve_vector_layer_ref(layer_ref)
        if not category_fields:
            raise Exception("category_fields is required")
        for field_name in category_fields:
            if self._field_index(layer, field_name) < 0:
                raise Exception(f"Field not found: {field_name}")
        if values_field and self._field_index(layer, values_field) < 0:
            raise Exception(f"Field not found: {values_field}")

        grouped: dict[tuple[Any, ...], dict[str, Any]] = {}
        for feature in layer.getFeatures():
            key = tuple(feature.attribute(name) for name in category_fields)
            bucket = grouped.setdefault(key, {"count": 0, "sum": 0.0, "values_count": 0})
            bucket["count"] += 1
            if values_field:
                try:
                    bucket["sum"] += float(feature.attribute(values_field))
                    bucket["values_count"] += 1
                except (TypeError, ValueError):
                    pass

        groups: list[dict[str, Any]] = []
        for key, bucket in grouped.items():
            item = {"categories": {category_fields[i]: key[i] for i in range(len(category_fields))}, "count": bucket["count"]}
            if values_field and bucket["values_count"] > 0:
                item["sum"] = bucket["sum"]
                item["mean"] = bucket["sum"] / bucket["values_count"]
            groups.append(item)

        return self._ok_result("vector_stats_by_categories", summary={"group_count": len(groups), "input_feature_count": layer.featureCount()}, outputs={"groups": groups})

    def raster_stats_basic(self, layer_ref: str, band: int = 1) -> dict[str, Any]:
        return self._run(self._raster_stats_basic_impl, layer_ref, band)

    def _raster_stats_basic_impl(self, layer_ref: str, band: int) -> dict[str, Any]:
        layer = self._resolve_raster_layer_ref(layer_ref)
        band_number = max(1, self._safe_int(band, 1))
        stats = layer.dataProvider().bandStatistics(band_number)
        return self._ok_result("raster_stats_basic", summary={"band": band_number, "count": int(stats.elementCount), "min": float(stats.minimumValue), "max": float(stats.maximumValue), "mean": float(stats.mean), "std_dev": float(stats.stdDev)}, outputs={"layer_id": layer.id()})

    def raster_stats_zonal(self, vector_layer_ref: str, raster_layer_ref: str, raster_band: int = 1, column_prefix: str = "z_", in_place: bool = False) -> dict[str, Any]:
        return self._run(self._raster_stats_zonal_impl, vector_layer_ref, raster_layer_ref, raster_band, column_prefix, in_place)

    def _raster_stats_zonal_impl(self, vector_layer_ref: str, raster_layer_ref: str, raster_band: int, column_prefix: str, in_place: bool) -> dict[str, Any]:
        vector_layer = self._resolve_vector_layer_ref(vector_layer_ref)
        raster_layer = self._resolve_raster_layer_ref(raster_layer_ref)
        self._ensure_processing_runtime()
        algorithm_id = "native:zonalstatisticsfb"
        if QgsApplication.processingRegistry().algorithmById(algorithm_id) is None:
            raise Exception(f"Algorithm not available: {algorithm_id}")

        parameters = {
            "INPUT": vector_layer,
            "INPUT_RASTER": raster_layer,
            "RASTER_BAND": max(1, self._safe_int(raster_band, 1)),
            "COLUMN_PREFIX": column_prefix or "z_",
            "OUTPUT": vector_layer if in_place else "TEMPORARY_OUTPUT",
        }
        result = processing.run(algorithm_id, parameters)
        output_layer_id = self._coerce_output_identifier(result.get("OUTPUT")) or vector_layer.id()
        return self._ok_result("raster_stats_zonal", summary={"algorithm": algorithm_id, "output_layer_id": output_layer_id}, outputs={"result": self._serialize_value(result)})

    def raster_stats_cell(self, raster_layer_refs: list[str], statistic: int = 0, ignore_nodata: bool = True) -> dict[str, Any]:
        return self._run(self._raster_stats_cell_impl, raster_layer_refs, statistic, ignore_nodata)

    def _raster_stats_cell_impl(self, raster_layer_refs: list[str], statistic: int, ignore_nodata: bool) -> dict[str, Any]:
        if not raster_layer_refs:
            raise Exception("raster_layer_refs is required")
        layers = [self._resolve_raster_layer_ref(ref) for ref in raster_layer_refs]
        self._ensure_processing_runtime()
        result = processing.run(
            "native:cellstatistics",
            {
                "INPUT": layers,
                "STATISTIC": self._safe_int(statistic, 0),
                "IGNORE_NODATA": bool(ignore_nodata),
                "REFERENCE_LAYER": layers[0],
                "OUTPUT_NODATA_VALUE": -9999,
                "OUTPUT": "TEMPORARY_OUTPUT",
            },
        )
        return self._ok_result("raster_stats_cell", summary={"input_count": len(layers), "output": self._coerce_output_identifier(result.get("OUTPUT"))}, outputs={"result": self._serialize_value(result)})

    @staticmethod
    def _detect_dataset_kind(path: Path) -> str | None:
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
        layer = QgsVectorLayer(str(path), "__probe__", "ogr")
        if not layer.isValid():
            return "unknown"
        return {0: "point", 1: "line", 2: "polygon"}.get(int(layer.geometryType()), "unknown")

    def dataset_list_files(self, directory: str, recursive: bool = False, dataset_kind: str = "both", geometry_type: str = "any", name_glob: str = "*", limit: int = DEFAULT_DATASET_LIMIT) -> dict[str, Any]:
        return self._run(self._dataset_list_files_impl, directory, recursive, dataset_kind, geometry_type, name_glob, limit)

    def _dataset_list_files_impl(self, directory: str, recursive: bool, dataset_kind: str, geometry_type: str, name_glob: str, limit: int) -> dict[str, Any]:
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

    def dataset_load_from_directory(self, directory: str, recursive: bool = False, dataset_kind: str = "both", geometry_type: str = "any", name_glob: str = "*", limit: int = DEFAULT_DATASET_LIMIT, skip_invalid: bool = True) -> dict[str, Any]:
        return self._run(self._dataset_load_from_directory_impl, directory, recursive, dataset_kind, geometry_type, name_glob, limit, skip_invalid)

    def _dataset_load_from_directory_impl(self, directory: str, recursive: bool, dataset_kind: str, geometry_type: str, name_glob: str, limit: int, skip_invalid: bool) -> dict[str, Any]:
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

    def filesystem_query_list_entries(self, directory: str, recursive: bool = False, include_files: bool = True, include_directories: bool = True, name_glob: str = "*", limit: int = DEFAULT_FILESYSTEM_LIST_LIMIT) -> dict[str, Any]:
        return self._run(self._filesystem_query_list_entries_impl, directory, recursive, include_files, include_directories, name_glob, limit)

    def _filesystem_query_list_entries_impl(self, directory: str, recursive: bool, include_files: bool, include_directories: bool, name_glob: str, limit: int) -> dict[str, Any]:
        if not include_files and not include_directories:
            raise Exception("include_files and include_directories cannot both be false")
        root = Path(directory)
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

    def filesystem_query_entry_info(self, path: str) -> dict[str, Any]:
        return self._run(self._filesystem_query_entry_info_impl, path)

    def _filesystem_query_entry_info_impl(self, path: str) -> dict[str, Any]:
        entry = Path(path)
        if not entry.exists():
            raise Exception(f"Path not found: {path}")
        return self._ok_result("filesystem_query_entry_info", summary={"exists": True}, outputs={"entry": self._path_info(entry)})

    def filesystem_query_read_text(self, path: str, max_chars: int | None = None) -> dict[str, Any]:
        return self._run(self._filesystem_query_read_text_impl, path, max_chars)

    def _filesystem_query_read_text_impl(self, path: str, max_chars: int | None) -> dict[str, Any]:
        entry = Path(path)
        if not entry.exists() or not entry.is_file():
            raise Exception(f"File not found: {path}")
        text = entry.read_text(encoding="utf-8")
        if max_chars is None:
            return self._ok_result("filesystem_query_read_text", summary={"truncated": False, "max_chars": None}, outputs={"text": text})
        applied = max(0, self._safe_int(max_chars, 0))
        return self._ok_result("filesystem_query_read_text", summary={"truncated": len(text) > applied, "max_chars": applied}, outputs={"text": text[:applied]})

    def filesystem_edit_write_text(self, path: str, content: str, overwrite: bool = False, confirm_destructive: bool = False, create_parents: bool = True) -> dict[str, Any]:
        return self._run(self._filesystem_edit_write_text_impl, path, content, overwrite, confirm_destructive, create_parents)

    def _filesystem_edit_write_text_impl(self, path: str, content: str, overwrite: bool, confirm_destructive: bool, create_parents: bool) -> dict[str, Any]:
        target = Path(path)
        if target.exists():
            if overwrite:
                if not confirm_destructive:
                    raise Exception("confirm_destructive must be true when overwrite is enabled")
            else:
                raise Exception(f"File already exists: {path}")
        if not target.parent.exists():
            if create_parents:
                target.parent.mkdir(parents=True, exist_ok=True)
            else:
                raise Exception(f"Parent directory not found: {target.parent}")
        target.write_text(content, encoding="utf-8")
        return self._ok_result("filesystem_edit_write_text", summary={"written_chars": len(content), "path": str(target)})

    def filesystem_edit_append_text(self, path: str, content: str, create_parents: bool = True) -> dict[str, Any]:
        return self._run(self._filesystem_edit_append_text_impl, path, content, create_parents)

    def _filesystem_edit_append_text_impl(self, path: str, content: str, create_parents: bool) -> dict[str, Any]:
        target = Path(path)
        if not target.parent.exists():
            if create_parents:
                target.parent.mkdir(parents=True, exist_ok=True)
            else:
                raise Exception(f"Parent directory not found: {target.parent}")
        with target.open("a", encoding="utf-8") as handle:
            handle.write(content)
        return self._ok_result("filesystem_edit_append_text", summary={"appended_chars": len(content), "path": str(target)})

    def filesystem_edit_copy_entry(self, source_path: str, target_path: str, overwrite: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
        return self._run(self._filesystem_edit_copy_entry_impl, source_path, target_path, overwrite, confirm_destructive)

    def _filesystem_edit_copy_entry_impl(self, source_path: str, target_path: str, overwrite: bool, confirm_destructive: bool) -> dict[str, Any]:
        source = Path(source_path)
        target = Path(target_path)
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
        shutil.copytree(source, target) if source.is_dir() else shutil.copy2(source, target)
        return self._ok_result("filesystem_edit_copy_entry", summary={"source_path": str(source), "target_path": str(target)})

    def filesystem_edit_move_entry(self, source_path: str, target_path: str, overwrite: bool = False, confirm_destructive: bool = False) -> dict[str, Any]:
        return self._run(self._filesystem_edit_move_entry_impl, source_path, target_path, overwrite, confirm_destructive)

    def _filesystem_edit_move_entry_impl(self, source_path: str, target_path: str, overwrite: bool, confirm_destructive: bool) -> dict[str, Any]:
        source = Path(source_path)
        target = Path(target_path)
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

    def filesystem_edit_delete_entry(self, path: str, confirm_destructive: bool = False) -> dict[str, Any]:
        return self._run(self._filesystem_edit_delete_entry_impl, path, confirm_destructive)

    def _filesystem_edit_delete_entry_impl(self, path: str, confirm_destructive: bool) -> dict[str, Any]:
        if not confirm_destructive:
            raise Exception("confirm_destructive must be true for delete operation")
        target = Path(path)
        if not target.exists():
            raise Exception(f"Path not found: {path}")
        shutil.rmtree(target) if target.is_dir() else target.unlink()
        return self._ok_result("filesystem_edit_delete_entry", summary={"deleted_path": str(target)})

    def filesystem_stats_directory(self, directory: str, recursive: bool = False) -> dict[str, Any]:
        return self._run(self._filesystem_stats_directory_impl, directory, recursive)

    def _filesystem_stats_directory_impl(self, directory: str, recursive: bool) -> dict[str, Any]:
        root = Path(directory)
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

    def processing_list_providers(self) -> dict[str, Any]:
        return self._run(self._processing_list_providers_impl)

    def _processing_list_providers_impl(self) -> dict[str, Any]:
        self._ensure_processing_runtime()
        providers = list(QgsApplication.processingRegistry().providers())
        payload = []
        for provider in providers:
            active = True
            if hasattr(provider, "isActive") and callable(provider.isActive):
                try:
                    active = bool(provider.isActive())
                except Exception:
                    active = True
            payload.append({"id": provider.id(), "name": provider.name(), "active": active})
        return {"count": len(payload), "providers": payload}

    def processing_get_algorithms(self, algorithm_id: str | None = None, provider_id: str | None = None, include_parameters: bool = False, include_outputs: bool = False, limit: int | None = None) -> dict[str, Any]:
        return self._run(self._processing_get_algorithms_impl, algorithm_id, provider_id, include_parameters, include_outputs, limit)

    def _processing_get_algorithms_impl(self, algorithm_id: str | None, provider_id: str | None, include_parameters: bool, include_outputs: bool, limit: int | None) -> dict[str, Any]:
        self._ensure_processing_runtime()
        registry = QgsApplication.processingRegistry()
        if algorithm_id:
            algorithm = registry.algorithmById(algorithm_id)
            if algorithm is None:
                raise Exception(f"Algorithm not found: {algorithm_id}")
            return {"algorithm": self._serialize_algorithm(algorithm, include_parameters=True, include_outputs=True)}

        algorithms = list(registry.algorithms())
        if provider_id:
            text = provider_id.strip().lower()
            algorithms = [a for a in algorithms if a.provider() and (a.provider().id().lower() == text or a.provider().name().lower() == text)]

        requested, applied, capped = self._normalize_algorithm_list_limit(limit)
        total = len(algorithms)
        if total > applied:
            algorithms = algorithms[:applied]
        return {
            "count": total,
            "returned": len(algorithms),
            "truncated": total > len(algorithms),
            "requested_limit": requested,
            "applied_limit": applied,
            "limit_capped": capped,
            "algorithms": [self._serialize_algorithm(a, include_parameters=include_parameters, include_outputs=include_outputs) for a in algorithms],
        }

    def processing_get_parameter_template(self, algorithm_id: str) -> dict[str, Any]:
        return self._run(self._processing_get_parameter_template_impl, algorithm_id)

    def _processing_get_parameter_template_impl(self, algorithm_id: str) -> dict[str, Any]:
        self._ensure_processing_runtime()
        algorithm = QgsApplication.processingRegistry().algorithmById(algorithm_id)
        if algorithm is None:
            raise Exception(f"Algorithm not found: {algorithm_id}")
        required: list[dict[str, Any]] = []
        optional: list[dict[str, Any]] = []
        for parameter in algorithm.parameterDefinitions():
            item = self._serialize_parameter(parameter)
            (optional if item.get("optional") else required).append(item)
        return {
            "algorithm": {"id": algorithm.id(), "name": algorithm.displayName()},
            "required_parameters": required,
            "optional_parameters": optional,
            "outputs": [self._serialize_output(o) for o in algorithm.outputDefinitions()],
        }

    def processing_execute_algorithm(self, algorithm: str, parameters: dict[str, Any], load_results: bool = True, allow_disk_write: bool = False, allow_in_place_edit: bool = False) -> dict[str, Any]:
        return self._run(self._processing_execute_algorithm_impl, algorithm, parameters, load_results, allow_disk_write, allow_in_place_edit)

    def _processing_execute_algorithm_impl(self, algorithm: str, parameters: dict[str, Any], load_results: bool, allow_disk_write: bool, allow_in_place_edit: bool) -> dict[str, Any]:
        if not isinstance(parameters, dict):
            raise Exception("parameters must be an object")
        effective, warnings = self._sanitize_processing_parameters(parameters, allow_disk_write, allow_in_place_edit)
        result = self._execute_processing_call(algorithm, effective, load_results)
        return self._ok_result("processing_execute_algorithm", summary={"algorithm": algorithm, "load_results": bool(load_results)}, outputs={"result": self._serialize_value(result)}, warnings=warnings, safety_policy={"allow_disk_write": bool(allow_disk_write), "allow_in_place_edit": bool(allow_in_place_edit)}, effective_parameters=self._serialize_value(effective))

    def processing_execute_on_layers(self, algorithm: str, layer_bindings: dict[str, Any], parameters: dict[str, Any], load_results: bool = True, batch_mode: bool = False, allow_disk_write: bool = False, allow_in_place_edit: bool = False) -> dict[str, Any]:
        return self._run(self._processing_execute_on_layers_impl, algorithm, layer_bindings, parameters, load_results, batch_mode, allow_disk_write, allow_in_place_edit)

    def _normalize_layer_bindings(self, layer_bindings: dict[str, Any]) -> dict[str, list[str]]:
        if not isinstance(layer_bindings, dict):
            raise Exception("layer_bindings is required")
        normalized: dict[str, list[str]] = {}
        for key, raw_value in layer_bindings.items():
            name = str(key).strip()
            if not name:
                continue
            if isinstance(raw_value, (list, tuple)):
                refs = [str(item).strip() for item in raw_value if str(item).strip()]
            else:
                text = str(raw_value).strip()
                refs = [text] if text else []
            if refs:
                normalized[name] = refs
        if not normalized:
            raise Exception("layer_bindings is required")
        return normalized

    def _processing_execute_on_layers_impl(self, algorithm: str, layer_bindings: dict[str, Any], parameters: dict[str, Any], load_results: bool, batch_mode: bool, allow_disk_write: bool, allow_in_place_edit: bool) -> dict[str, Any]:
        if not isinstance(parameters, dict):
            raise Exception("parameters must be an object")
        normalized_bindings = self._normalize_layer_bindings(layer_bindings)
        run_count = max(len(refs) for refs in normalized_bindings.values()) if batch_mode else 1

        success_count = 0
        failure_count = 0
        runs: list[dict[str, Any]] = []
        all_warnings: list[str] = []
        last_effective = dict(parameters)

        for index in range(run_count):
            try:
                effective = dict(parameters)
                for binding_name, refs in normalized_bindings.items():
                    if batch_mode and index >= len(refs):
                        raise Exception(f"layer binding '{binding_name}' missing value for batch index {index}")
                    ref = refs[index] if batch_mode else refs[0]
                    effective[binding_name] = self._resolve_layer_ref(ref).id()

                effective, warnings = self._sanitize_processing_parameters(effective, allow_disk_write, allow_in_place_edit)
                last_effective = effective
                all_warnings.extend(warnings)
                result = self._execute_processing_call(algorithm, effective, load_results)
                success_count += 1
                runs.append({"index": index, "ok": True, "result": self._serialize_value(result)})
            except Exception as exc:
                failure_count += 1
                runs.append({"index": index, "ok": False, "error": str(exc)})
                if not batch_mode:
                    raise

        return {
            "ok": failure_count == 0,
            "tool": "processing_execute_on_layers",
            "run_count": run_count,
            "success_count": success_count,
            "failure_count": failure_count,
            "runs": runs,
            "safety_policy": {"allow_disk_write": bool(allow_disk_write), "allow_in_place_edit": bool(allow_in_place_edit)},
            "warnings": all_warnings,
            "effective_parameters": self._serialize_value(last_effective),
        }


def register_tools(mcp, tools: ProcessingMCPTools, enable_execute_code: bool = True) -> None:
    _ = enable_execute_code
    tool_factory = getattr(mcp, "tool", None)
    if not callable(tool_factory):
        return
    for tool_name in REGISTERED_TOOL_NAMES:
        method = getattr(tools, tool_name, None)
        if callable(method) and getattr(method, "__name__", "") != "<lambda>":
            tool_factory()(method)
            continue

        def _wrapper(*args, _tool_name=tool_name, **kwargs):
            return getattr(tools, _tool_name)(*args, **kwargs)

        _wrapper.__name__ = tool_name
        _wrapper.__doc__ = f"MCP tool wrapper for {tool_name}."
        tool_factory()(_wrapper)

