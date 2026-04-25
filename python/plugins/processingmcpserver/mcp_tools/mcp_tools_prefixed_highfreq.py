"""High-frequency prefixed MCP tools."""

from __future__ import annotations

from pathlib import Path
from typing import Any

TOOL_DOCS: dict[str, str] = {
    "mcp_tools_dataset_load_layers": "统一加载矢量/栅格图层，默认跳过无效数据并返回加载摘要。",
    "mcp_tools_dataset_inspect_vector_bundle": "检查矢量数据包完整性与风险并返回结构化摘要。",
    "mcp_tools_vector_export_dataset": "导出矢量数据，默认 GPKG，默认临时输出，显式 output_path 才写盘。",
    "mcp_tools_vector_buffer": "执行矢量缓冲区分析，默认输出为临时图层。",
    "mcp_tools_vector_dissolve": "执行矢量溶解，支持按字段分组，默认临时输出。",
    "mcp_tools_vector_clip": "执行矢量裁剪，默认临时输出。",
    "mcp_tools_vector_intersection": "执行矢量相交叠加，默认临时输出。",
    "mcp_tools_vector_union": "执行矢量联合叠加，默认临时输出。",
    "mcp_tools_vector_difference": "执行矢量差异叠加，默认临时输出。",
    "mcp_tools_vector_fix_geometries": "修复矢量几何，默认临时输出。",
    "mcp_tools_vector_reproject_layer": "重投影矢量图层，默认临时输出。",
    "mcp_tools_vector_join_attributes_by_location": "按空间关系连接属性，默认临时输出。",
    "mcp_tools_raster_clip_by_mask": "按掩膜裁剪栅格，默认临时输出。",
    "mcp_tools_raster_warp_reproject": "重投影栅格，默认临时输出。",
    "mcp_tools_raster_calculator": "执行栅格表达式计算，默认临时输出。",
    "mcp_tools_raster_merge": "合并多个栅格，默认临时输出。",
    "mcp_tools_raster_reclassify_by_table": "按重分类表重分类栅格，默认临时输出。",
}


def _error_payload(self, tool: str, stage: str, exc: Exception, parameters: dict[str, Any] | None = None) -> dict[str, Any]:
    return {
        "ok": False,
        "tool": tool,
        "error_stage": stage,
        "first_error": str(exc),
        "effective_parameters": self._serialize_value(parameters or {}),
    }


def _resolve_output_target(
    self,
    output_path: str,
    overwrite: bool,
    confirm_write: bool,
    confirm_destructive: bool,
) -> tuple[str, bool]:
    output_text = str(output_path or "").strip()
    if not output_text:
        return "TEMPORARY_OUTPUT", False

    self._ensure_filesystem_write_confirmed(confirm_write)
    output = Path(output_text)
    if output.exists() and not overwrite:
        raise Exception(f"Output already exists: {output}")
    if output.exists() and overwrite and not confirm_destructive:
        raise Exception("confirm_destructive must be true when overwrite is enabled")
    output.parent.mkdir(parents=True, exist_ok=True)
    return str(output), True


def _resolve_layer_id(self, layer_ref: str) -> str:
    return self._resolve_layer_ref(layer_ref).id()


def _run_processing_tool(
    self,
    *,
    tool_name: str,
    algorithm: str,
    parameters: dict[str, Any],
    load_results: bool = True,
    allow_disk_write: bool = False,
) -> dict[str, Any]:
    try:
        effective, warnings = self._sanitize_processing_parameters(
            parameters,
            allow_disk_write=allow_disk_write,
            allow_in_place_edit=False,
        )
        result = self._execute_processing_call(algorithm, effective, load_results)
        return self._ok_result(
            tool_name,
            summary={"algorithm": algorithm, "load_results": bool(load_results)},
            outputs={"result": self._serialize_value(result)},
            warnings=warnings,
            effective_parameters=self._serialize_value(effective),
        )
    except Exception as exc:
        return _error_payload(self, tool_name, "execute_processing", exc, parameters)


def mcp_tools_dataset_load_layers(
    self,
    paths: list[str] | None = None,
    directory: str = "",
    recursive: bool = False,
    dataset_kind: str = "both",
    geometry_type: str = "any",
    name_glob: str = "*",
    limit: int = 50,
    skip_invalid: bool = True,
) -> dict[str, Any]:
    return self._run(
        self._mcp_tools_dataset_load_layers_impl,
        paths,
        directory,
        recursive,
        dataset_kind,
        geometry_type,
        name_glob,
        limit,
        skip_invalid,
    )


def _mcp_tools_dataset_load_layers_impl(
    self,
    paths: list[str] | None,
    directory: str,
    recursive: bool,
    dataset_kind: str,
    geometry_type: str,
    name_glob: str,
    limit: int,
    skip_invalid: bool,
) -> dict[str, Any]:
    tool_name = "mcp_tools_dataset_load_layers"
    try:
        vector_paths: list[str] = []
        raster_paths: list[str] = []

        if paths:
            if not isinstance(paths, list) or any(not isinstance(p, str) for p in paths):
                raise Exception("paths must be a list[str]")
            candidates = [Path(p) for p in paths]
        else:
            if not str(directory or "").strip():
                raise Exception("either paths or directory must be provided")
            listed = self.dataset_list_files(
                directory=directory,
                recursive=recursive,
                dataset_kind=dataset_kind,
                geometry_type=geometry_type,
                name_glob=name_glob,
                limit=limit,
            )
            candidates = [Path(item["path"]) for item in listed.get("datasets", [])]

        for entry in candidates:
            detected = self._detect_dataset_kind(entry)
            if detected == "vector":
                vector_paths.append(str(entry))
            elif detected == "raster":
                raster_paths.append(str(entry))

        loaded: list[dict[str, Any]] = []
        failed: list[dict[str, Any]] = []

        if vector_paths:
            result = self.vector_add_layers(paths=vector_paths, provider="ogr", skip_invalid=skip_invalid)
            loaded.extend(result.get("loaded", []))
            failed.extend(result.get("failed", []))
        if raster_paths:
            result = self.raster_add_layers(paths=raster_paths, provider="gdal", skip_invalid=skip_invalid)
            loaded.extend(result.get("loaded", []))
            failed.extend(result.get("failed", []))

        return self._ok_result(
            tool_name,
            summary={
                "requested_count": len(candidates),
                "loaded_count": len(loaded),
                "failed_count": len(failed),
            },
            outputs={
                "loaded": self._serialize_value(loaded),
                "failed": self._serialize_value(failed),
            },
            warnings=[],
            effective_parameters={
                "paths": [str(x) for x in candidates],
                "skip_invalid": bool(skip_invalid),
            },
        )
    except Exception as exc:
        return _error_payload(self, tool_name, "load_layers", exc, {"paths": paths, "directory": directory})


def mcp_tools_dataset_inspect_vector_bundle(
    self,
    path: str,
    recursive: bool = False,
    name_glob: str = "*.shp",
    limit: int = 50,
    task_name: str = "",
) -> dict[str, Any]:
    return self._run(self._mcp_tools_dataset_inspect_vector_bundle_impl, path, recursive, name_glob, limit, task_name)


def _mcp_tools_dataset_inspect_vector_bundle_impl(
    self,
    path: str,
    recursive: bool,
    name_glob: str,
    limit: int,
    task_name: str,
) -> dict[str, Any]:
    tool_name = "mcp_tools_dataset_inspect_vector_bundle"
    try:
        result = self.dataset_inspect_shapefile_bundle(
            path=path,
            recursive=recursive,
            name_glob=name_glob,
            limit=limit,
            task_name=task_name,
        )
        if not isinstance(result, dict):
            raise Exception("unexpected result from dataset_inspect_shapefile_bundle")

        legacy_outputs = result.get("outputs", {})
        bundles = legacy_outputs.get("bundles", []) if isinstance(legacy_outputs, dict) else []
        legacy_summary = result.get("summary", {})
        summary = dict(legacy_summary) if isinstance(legacy_summary, dict) else {}
        summary["bundle_count"] = len(bundles)

        return self._ok_result(
            tool_name,
            summary=summary,
            outputs={
                "path": self._serialize_value(legacy_outputs.get("path")),
                "bundles": self._serialize_value(bundles),
            },
            warnings=self._serialize_value(result.get("warnings", [])),
            effective_parameters={
                "path": path,
                "recursive": bool(recursive),
                "name_glob": name_glob,
                "limit": int(limit),
                "task_name": task_name,
            },
        )
    except Exception as exc:
        return _error_payload(self, tool_name, "inspect_bundle", exc, {"path": path})


def mcp_tools_vector_export_dataset(
    self,
    layer_ref: str,
    output_path: str = "",
    file_format: str = "gpkg",
    overwrite: bool = False,
    confirm_write: bool = False,
    confirm_destructive: bool = False,
    load_results: bool = True,
) -> dict[str, Any]:
    return self._run(
        self._mcp_tools_vector_export_dataset_impl,
        layer_ref,
        output_path,
        file_format,
        overwrite,
        confirm_write,
        confirm_destructive,
        load_results,
    )


def _mcp_tools_vector_export_dataset_impl(
    self,
    layer_ref: str,
    output_path: str,
    file_format: str,
    overwrite: bool,
    confirm_write: bool,
    confirm_destructive: bool,
    load_results: bool,
) -> dict[str, Any]:
    tool_name = "mcp_tools_vector_export_dataset"
    try:
        fmt = str(file_format or "gpkg").strip().lower()
        if fmt not in {"gpkg", "shp"}:
            raise Exception("file_format must be one of: gpkg, shp")

        if fmt == "shp":
            if not str(output_path or "").strip():
                raise Exception("output_path is required when file_format=shp")
            out = Path(output_path)
            result = self.vector_export_shapefile(
                layer_ref=layer_ref,
                output_directory=str(out.parent),
                file_name=out.stem,
                overwrite=overwrite,
                confirm_write=confirm_write,
                confirm_destructive=confirm_destructive,
            )
            return self._ok_result(
                tool_name,
                summary={"format": "shp", "output_path": str(out)},
                outputs={"result": self._serialize_value(result)},
                warnings=[],
                effective_parameters={"layer_ref": layer_ref, "output_path": str(out), "file_format": fmt},
            )

        layer_id = _resolve_layer_id(self, layer_ref)
        output_value, allow_disk_write = _resolve_output_target(
            self,
            output_path,
            overwrite,
            confirm_write,
            confirm_destructive,
        )
        return _run_processing_tool(
            self,
            tool_name=tool_name,
            algorithm="native:savefeatures",
            parameters={"INPUT": layer_id, "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, tool_name, "vector_export", exc, {"layer_ref": layer_ref, "output_path": output_path})


def mcp_tools_vector_buffer(
    self,
    input_layer_ref: str,
    distance: float,
    output_path: str = "",
    dissolve: bool = False,
    overwrite: bool = False,
    confirm_write: bool = False,
    confirm_destructive: bool = False,
    load_results: bool = True,
) -> dict[str, Any]:
    return self._run(self._mcp_tools_vector_buffer_impl, input_layer_ref, distance, output_path, dissolve, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_vector_buffer_impl(self, input_layer_ref: str, distance: float, output_path: str, dissolve: bool, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_vector_buffer",
            algorithm="native:buffer",
            parameters={
                "INPUT": _resolve_layer_id(self, input_layer_ref),
                "DISTANCE": float(distance),
                "SEGMENTS": 8,
                "END_CAP_STYLE": 0,
                "JOIN_STYLE": 0,
                "MITER_LIMIT": 2.0,
                "DISSOLVE": bool(dissolve),
                "OUTPUT": output_value,
            },
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_vector_buffer", "vector_buffer", exc)


def mcp_tools_vector_dissolve(self, input_layer_ref: str, fields: list[str] | None = None, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_vector_dissolve_impl, input_layer_ref, fields or [], output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_vector_dissolve_impl(self, input_layer_ref: str, fields: list[str], output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_vector_dissolve",
            algorithm="native:dissolve",
            parameters={"INPUT": _resolve_layer_id(self, input_layer_ref), "FIELD": fields, "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_vector_dissolve", "vector_dissolve", exc)


def mcp_tools_vector_clip(self, input_layer_ref: str, overlay_layer_ref: str, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_vector_clip_impl, input_layer_ref, overlay_layer_ref, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_vector_clip_impl(self, input_layer_ref: str, overlay_layer_ref: str, output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_vector_clip",
            algorithm="native:clip",
            parameters={"INPUT": _resolve_layer_id(self, input_layer_ref), "OVERLAY": _resolve_layer_id(self, overlay_layer_ref), "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_vector_clip", "vector_clip", exc)


def mcp_tools_vector_intersection(self, input_layer_ref: str, overlay_layer_ref: str, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_vector_intersection_impl, input_layer_ref, overlay_layer_ref, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_vector_intersection_impl(self, input_layer_ref: str, overlay_layer_ref: str, output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_vector_intersection",
            algorithm="native:intersection",
            parameters={"INPUT": _resolve_layer_id(self, input_layer_ref), "OVERLAY": _resolve_layer_id(self, overlay_layer_ref), "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_vector_intersection", "vector_intersection", exc)


def mcp_tools_vector_union(self, input_layer_ref: str, overlay_layer_ref: str, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_vector_union_impl, input_layer_ref, overlay_layer_ref, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_vector_union_impl(self, input_layer_ref: str, overlay_layer_ref: str, output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_vector_union",
            algorithm="native:union",
            parameters={"INPUT": _resolve_layer_id(self, input_layer_ref), "OVERLAY": _resolve_layer_id(self, overlay_layer_ref), "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_vector_union", "vector_union", exc)


def mcp_tools_vector_difference(self, input_layer_ref: str, overlay_layer_ref: str, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_vector_difference_impl, input_layer_ref, overlay_layer_ref, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_vector_difference_impl(self, input_layer_ref: str, overlay_layer_ref: str, output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_vector_difference",
            algorithm="native:difference",
            parameters={"INPUT": _resolve_layer_id(self, input_layer_ref), "OVERLAY": _resolve_layer_id(self, overlay_layer_ref), "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_vector_difference", "vector_difference", exc)


def mcp_tools_vector_fix_geometries(self, input_layer_ref: str, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_vector_fix_geometries_impl, input_layer_ref, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_vector_fix_geometries_impl(self, input_layer_ref: str, output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_vector_fix_geometries",
            algorithm="native:fixgeometries",
            parameters={"INPUT": _resolve_layer_id(self, input_layer_ref), "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_vector_fix_geometries", "vector_fix_geometries", exc)


def mcp_tools_vector_reproject_layer(self, input_layer_ref: str, target_crs: str, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_vector_reproject_layer_impl, input_layer_ref, target_crs, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_vector_reproject_layer_impl(self, input_layer_ref: str, target_crs: str, output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_vector_reproject_layer",
            algorithm="native:reprojectlayer",
            parameters={"INPUT": _resolve_layer_id(self, input_layer_ref), "TARGET_CRS": target_crs, "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_vector_reproject_layer", "vector_reproject_layer", exc)


def mcp_tools_vector_join_attributes_by_location(self, input_layer_ref: str, join_layer_ref: str, predicate: list[int] | None = None, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_vector_join_attributes_by_location_impl, input_layer_ref, join_layer_ref, predicate or [0], output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_vector_join_attributes_by_location_impl(self, input_layer_ref: str, join_layer_ref: str, predicate: list[int], output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_vector_join_attributes_by_location",
            algorithm="native:joinattributesbylocation",
            parameters={
                "INPUT": _resolve_layer_id(self, input_layer_ref),
                "JOIN": _resolve_layer_id(self, join_layer_ref),
                "PREDICATE": predicate,
                "METHOD": 0,
                "DISCARD_NONMATCHING": False,
                "PREFIX": "",
                "OUTPUT": output_value,
            },
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_vector_join_attributes_by_location", "vector_join_attributes_by_location", exc)


def mcp_tools_raster_clip_by_mask(self, raster_layer_ref: str, mask_layer_ref: str, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_raster_clip_by_mask_impl, raster_layer_ref, mask_layer_ref, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_raster_clip_by_mask_impl(self, raster_layer_ref: str, mask_layer_ref: str, output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_raster_clip_by_mask",
            algorithm="gdal:cliprasterbymasklayer",
            parameters={"INPUT": _resolve_layer_id(self, raster_layer_ref), "MASK": _resolve_layer_id(self, mask_layer_ref), "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_raster_clip_by_mask", "raster_clip_by_mask", exc)


def mcp_tools_raster_warp_reproject(self, raster_layer_ref: str, target_crs: str, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_raster_warp_reproject_impl, raster_layer_ref, target_crs, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_raster_warp_reproject_impl(self, raster_layer_ref: str, target_crs: str, output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_raster_warp_reproject",
            algorithm="gdal:warpreproject",
            parameters={"INPUT": _resolve_layer_id(self, raster_layer_ref), "TARGET_CRS": target_crs, "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_raster_warp_reproject", "raster_warp_reproject", exc)


def mcp_tools_raster_calculator(self, raster_layer_ref: str, expression: str, band: int = 1, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_raster_calculator_impl, raster_layer_ref, expression, band, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_raster_calculator_impl(self, raster_layer_ref: str, expression: str, band: int, output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_raster_calculator",
            algorithm="gdal:rastercalculator",
            parameters={
                "INPUT_A": _resolve_layer_id(self, raster_layer_ref),
                "BAND_A": int(band),
                "FORMULA": expression,
                "OUTPUT": output_value,
            },
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_raster_calculator", "raster_calculator", exc)


def mcp_tools_raster_merge(self, raster_layer_refs: list[str], output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_raster_merge_impl, raster_layer_refs, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_raster_merge_impl(self, raster_layer_refs: list[str], output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        if not isinstance(raster_layer_refs, list) or not raster_layer_refs:
            raise Exception("raster_layer_refs must be a non-empty list[str]")
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_raster_merge",
            algorithm="gdal:merge",
            parameters={"INPUT": [_resolve_layer_id(self, ref) for ref in raster_layer_refs], "OUTPUT": output_value},
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_raster_merge", "raster_merge", exc)


def mcp_tools_raster_reclassify_by_table(self, raster_layer_ref: str, table: list[float], band: int = 1, output_path: str = "", overwrite: bool = False, confirm_write: bool = False, confirm_destructive: bool = False, load_results: bool = True) -> dict[str, Any]:
    return self._run(self._mcp_tools_raster_reclassify_by_table_impl, raster_layer_ref, table, band, output_path, overwrite, confirm_write, confirm_destructive, load_results)


def _mcp_tools_raster_reclassify_by_table_impl(self, raster_layer_ref: str, table: list[float], band: int, output_path: str, overwrite: bool, confirm_write: bool, confirm_destructive: bool, load_results: bool) -> dict[str, Any]:
    try:
        if not isinstance(table, list) or len(table) < 3:
            raise Exception("table must be a list[float] with at least one rule triplet")
        output_value, allow_disk_write = _resolve_output_target(self, output_path, overwrite, confirm_write, confirm_destructive)
        return _run_processing_tool(
            self,
            tool_name="mcp_tools_raster_reclassify_by_table",
            algorithm="native:reclassifybytable",
            parameters={
                "INPUT_RASTER": _resolve_layer_id(self, raster_layer_ref),
                "RASTER_BAND": int(band),
                "TABLE": table,
                "NO_DATA": -9999,
                "RANGE_BOUNDARIES": 0,
                "NODATA_FOR_MISSING": True,
                "DATA_TYPE": 5,
                "OUTPUT": output_value,
            },
            load_results=load_results,
            allow_disk_write=allow_disk_write,
        )
    except Exception as exc:
        return _error_payload(self, "mcp_tools_raster_reclassify_by_table", "raster_reclassify_by_table", exc)


TOOL_METHODS: dict[str, object] = {
    "mcp_tools_dataset_load_layers": mcp_tools_dataset_load_layers,
    "_mcp_tools_dataset_load_layers_impl": _mcp_tools_dataset_load_layers_impl,
    "mcp_tools_dataset_inspect_vector_bundle": mcp_tools_dataset_inspect_vector_bundle,
    "_mcp_tools_dataset_inspect_vector_bundle_impl": _mcp_tools_dataset_inspect_vector_bundle_impl,
    "mcp_tools_vector_export_dataset": mcp_tools_vector_export_dataset,
    "_mcp_tools_vector_export_dataset_impl": _mcp_tools_vector_export_dataset_impl,
    "mcp_tools_vector_buffer": mcp_tools_vector_buffer,
    "_mcp_tools_vector_buffer_impl": _mcp_tools_vector_buffer_impl,
    "mcp_tools_vector_dissolve": mcp_tools_vector_dissolve,
    "_mcp_tools_vector_dissolve_impl": _mcp_tools_vector_dissolve_impl,
    "mcp_tools_vector_clip": mcp_tools_vector_clip,
    "_mcp_tools_vector_clip_impl": _mcp_tools_vector_clip_impl,
    "mcp_tools_vector_intersection": mcp_tools_vector_intersection,
    "_mcp_tools_vector_intersection_impl": _mcp_tools_vector_intersection_impl,
    "mcp_tools_vector_union": mcp_tools_vector_union,
    "_mcp_tools_vector_union_impl": _mcp_tools_vector_union_impl,
    "mcp_tools_vector_difference": mcp_tools_vector_difference,
    "_mcp_tools_vector_difference_impl": _mcp_tools_vector_difference_impl,
    "mcp_tools_vector_fix_geometries": mcp_tools_vector_fix_geometries,
    "_mcp_tools_vector_fix_geometries_impl": _mcp_tools_vector_fix_geometries_impl,
    "mcp_tools_vector_reproject_layer": mcp_tools_vector_reproject_layer,
    "_mcp_tools_vector_reproject_layer_impl": _mcp_tools_vector_reproject_layer_impl,
    "mcp_tools_vector_join_attributes_by_location": mcp_tools_vector_join_attributes_by_location,
    "_mcp_tools_vector_join_attributes_by_location_impl": _mcp_tools_vector_join_attributes_by_location_impl,
    "mcp_tools_raster_clip_by_mask": mcp_tools_raster_clip_by_mask,
    "_mcp_tools_raster_clip_by_mask_impl": _mcp_tools_raster_clip_by_mask_impl,
    "mcp_tools_raster_warp_reproject": mcp_tools_raster_warp_reproject,
    "_mcp_tools_raster_warp_reproject_impl": _mcp_tools_raster_warp_reproject_impl,
    "mcp_tools_raster_calculator": mcp_tools_raster_calculator,
    "_mcp_tools_raster_calculator_impl": _mcp_tools_raster_calculator_impl,
    "mcp_tools_raster_merge": mcp_tools_raster_merge,
    "_mcp_tools_raster_merge_impl": _mcp_tools_raster_merge_impl,
    "mcp_tools_raster_reclassify_by_table": mcp_tools_raster_reclassify_by_table,
    "_mcp_tools_raster_reclassify_by_table_impl": _mcp_tools_raster_reclassify_by_table_impl,
    "_error_payload": _error_payload,
    "_resolve_output_target": _resolve_output_target,
    "_resolve_layer_id": _resolve_layer_id,
    "_run_processing_tool": _run_processing_tool,
}
