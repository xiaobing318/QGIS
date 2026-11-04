"""MCP tool descriptors for QGIS bridge backed tools.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import os
import re
from dataclasses import dataclass
from typing import Any

from qcopilots_common import screenshot
from qcopilots_common.bridge import BridgeClient
from qcopilots_common.constants import (
    BRIDGE_URL_ENV,
    DEFAULT_PROCESSING_ALGORITHM_RESULTS,
    MAX_PROCESSING_ALGORITHM_CURSOR_LENGTH,
    MAX_PROCESSING_ALGORITHM_RESULTS,
)
from qcopilots_common.interactive_layer_tools import build_interactive_layer_tools
from qcopilots_common.mcp_http import McpTool
from qcopilots_common.processing_metadata import (
    processing_algorithm_matches_domain,
    processing_algorithm_owner,
    processing_algorithm_start_policy,
)


@dataclass(frozen=True)
class ProcessingToolDescriptor:
    name: str
    description: str
    algorithm_id: str


class ProcessingToolRegistry:
    def __init__(self, category: str, algorithms: list[Any], bridge: Any):
        self.category = category
        self.bridge = bridge
        self.algorithms = {
            _tool_name(algorithm.id()): algorithm
            for algorithm in filter_processing_algorithms(algorithms, category)
        }

    def list_tools(self) -> list[ProcessingToolDescriptor]:
        descriptors = []
        for name, algorithm in sorted(self.algorithms.items()):
            description = (
                _call_optional(algorithm, "shortHelpString")
                or _call_optional(algorithm, "displayName")
                or algorithm.id()
            )
            descriptors.append(
                ProcessingToolDescriptor(
                    name=name,
                    description=description,
                    algorithm_id=algorithm.id(),
                )
            )
        return descriptors

    def call_tool(self, tool_name: str, parameters: dict[str, Any]) -> dict[str, Any]:
        if tool_name not in self.algorithms:
            raise KeyError(tool_name)
        return self.bridge.run_algorithm(self.algorithms[tool_name].id(), parameters)


def classify_processing_algorithm(algorithm: Any) -> str | None:
    return processing_algorithm_owner(algorithm)


def filter_processing_algorithms(algorithms: list[Any], category: str) -> list[Any]:
    if category not in {"vector", "raster", "general"}:
        raise ValueError("category must be vector, raster or general")
    return [
        algorithm
        for algorithm in algorithms
        if processing_algorithm_matches_domain(algorithm, category)
        and (
            category != "general"
            or processing_algorithm_start_policy(algorithm)["supported"]
        )
    ]


def build_interactive_tools() -> list[McpTool]:
    bridge = _bridge_client()
    return [
        _bridge_tool(
            "list_layers",
            (
                "List every layer in the current QGIS project. Returns layers, each "
                "with id, name, source, type, provider and valid. Use the id as "
                "layer_id in other tools, for example zoom_to_layer or "
                "set_layer_visibility (they take the id, not the name), or check "
                "whether data is already loaded before load_map_layer. It does not "
                "report visibility, drawing order, CRS, fields or feature counts. "
                "Use get_layer_metadata for those details of one layer. Read-only."
            ),
            {},
            bridge,
        ),
        _bridge_tool(
            "set_layer_visibility",
            (
                "Show or hide one layer by checking or unchecking it in the QGIS "
                "Layers panel. Returns layer_id and the resulting visible state. A "
                "layer inside an unchecked group stays hidden on the map even when "
                "its own box is checked. To remove a layer from the project use "
                "remove_map_layers instead."
            ),
            {"layer_id": "string", "visible": "boolean"},
            bridge,
            ["layer_id", "visible"],
            {
                "layer_id": "Layer id from list_layers (the id field, not the layer name).",
                "visible": "true shows the layer, false hides it.",
            },
        ),
        _bridge_tool(
            "zoom_to_layer",
            (
                "Zoom the map canvas to the full extent of one layer, with a 5 "
                "percent margin like the QGIS Zoom to Layer action. The layer extent "
                "is transformed from the layer CRS to the canvas CRS. Returns "
                "layer_crs, canvas_crs and canvas_extent [xmin, ymin, xmax, ymax] in "
                "the canvas CRS, plus warnings, which notes a layer without a valid "
                "CRS whose extent is used untransformed. Layers without geometry "
                "or without a valid extent are rejected. Use zoom_to_selection for "
                "selected features and "
                "zoom_to_extent for known coordinates."
            ),
            {"layer_id": "string"},
            bridge,
            ["layer_id"],
            {"layer_id": "Layer id from list_layers (the id field, not the layer name)."},
        ),
        _custom_bridge_tool(
            "zoom_to_extent",
            (
                "Zoom the map canvas to an extent [xmin, ymin, xmax, ymax], for "
                "example a bounding box the user gives. Without crs the numbers are "
                "in the canvas (project) CRS. With crs, for example EPSG:3035, they "
                "are transformed from that CRS to the canvas CRS. Projected values "
                "such as metres on a geographic canvas are rejected unless crs is "
                "given, and xmin must be less than xmax and ymin less than ymax. "
                "Returns crs, canvas_crs and canvas_extent. Use zoom_to_layer "
                "to frame a whole layer."
            ),
            {
                "type": "object",
                "properties": {
                    "extent": {
                        "type": "array",
                        "items": {"type": "number"},
                        "minItems": 4,
                        "maxItems": 4,
                        "description": "xmin, ymin, xmax, ymax in crs, or in the canvas CRS when crs is omitted.",
                    },
                    "crs": {
                        "type": "string",
                        "minLength": 1,
                        "description": "Optional CRS of extent as an authority id, for example EPSG:3035 or EPSG:4326.",
                    },
                },
                "required": ["extent"],
                "additionalProperties": False,
            },
            "zoom_to_extent",
            bridge,
        ),
        _bridge_tool(
            "zoom_in",
            (
                "Zoom the map canvas in by one step around the current center, using "
                "the QGIS zoom factor, like the Zoom In button. Takes no arguments "
                "and returns only zoomed=in. To reach a specific area use "
                "zoom_to_extent or zoom_to_layer, and use capture_qgis_screenshot to "
                "check the result."
            ),
            {},
            bridge,
        ),
        _bridge_tool(
            "zoom_out",
            (
                "Zoom the map canvas out by one step around the current center, "
                "using the QGIS zoom factor, like the Zoom Out button. Takes no "
                "arguments and returns only zoomed=out. Use zoom_full for an "
                "overview of the whole project or zoom_to_layer to frame one layer."
            ),
            {},
            bridge,
        ),
        _bridge_tool(
            "zoom_full",
            (
                "Zoom the map canvas to the full project extent, which by default "
                "covers all project layers, like the QGIS Zoom Full button. Takes no "
                "arguments and returns zoomed=full. Use zoom_to_layer to frame a "
                "single layer instead."
            ),
            {},
            bridge,
        ),
        _bridge_tool(
            "zoom_to_selection",
            (
                "Zoom the map canvas to the selected features of a vector layer, "
                "like the QGIS Zoom to Selection button. The layer must already have "
                "a selection, so call set_vector_selection first. If QGIS leaves the "
                "view of a single selected point completely unchanged, the tool "
                "applies one native zoom-in step centered on that point. Returns "
                "layer_id, selected_count, zoomed, zoom_mode, fallback_applied, "
                "canvas_crs and the extent, scale and magnification before and "
                "after. Use zoom_to_layer to frame the whole layer."
            ),
            {"layer_id": "string"},
            bridge,
            None,
            {
                "layer_id": (
                    "Optional vector layer id from list_layers. Without it the active "
                    "layer in the Layers panel is used."
                ),
            },
        ),
        _bridge_tool(
            "zoom_to_native_resolution",
            (
                "Zoom the map canvas so that one raster pixel covers one screen "
                "pixel, like the QGIS Zoom to Native Resolution (100%) action. Use it "
                "to inspect raster detail at full resolution. Returns layer_id and "
                "the applied zoom_factor. The layer must be a raster layer. Use "
                "zoom_to_layer to see the whole raster."
            ),
            {"layer_id": "string"},
            bridge,
            None,
            {
                "layer_id": (
                    "Raster layer id from list_layers. When it is omitted, the "
                    "active layer is used. An id that is not in the project is an "
                    "error."
                ),
            },
        ),
        _bridge_tool(
            "zoom_to_last_extent",
            (
                "Go back to the previous map canvas extent in the zoom history, like "
                "the QGIS Zoom Last button, for example to undo the last zoom. Takes "
                "no arguments and returns zoomed=last. zoom_to_next_extent goes "
                "forward again."
            ),
            {},
            bridge,
        ),
        _bridge_tool(
            "zoom_to_next_extent",
            (
                "Go forward to the next map canvas extent in the zoom history, like "
                "the QGIS Zoom Next button. It only has an effect after "
                "zoom_to_last_extent. Takes no arguments and returns zoomed=next."
            ),
            {},
            bridge,
        ),
        _custom_bridge_tool(
            "save_project",
            (
                "Save the current QGIS project as a single .qgz file, written to a "
                "staging file first and then published in one step. Without path it "
                "saves to the current project file, which must already be a .qgz. "
                "An unsaved project needs path. After saving, the project uses that "
                "path as its file name, like Save As. Returns saved, path and a "
                "cleanup report. Saving to the current project file, with path "
                "omitted or equal to it, replaces that file directly like Ctrl+S in "
                "QGIS and needs no overwrite or token. Any other existing file is "
                "never replaced silently. Such a save fails unless overwrite=true. "
                "With overwrite=true and no token nothing is saved and a preview "
                "with overwrite_targets and a one-use overwrite_confirmation_token "
                "is returned. After the user approves, call again with the same "
                "path, overwrite=true and that token before "
                "confirmation_expires_in_seconds runs out."
            ),
            _save_project_schema(),
            "save_project",
            bridge,
        ),
        _bridge_tool(
            "refresh_canvas",
            (
                "Redraw the QGIS map canvas and return refreshed=true. The zoom and "
                "feature editing tools already redraw the canvas, so use it only when "
                "the map looks stale, for example before capture_qgis_screenshot."
            ),
            {},
            bridge,
        ),
        _custom_bridge_tool(
            "export_map_image",
            (
                "Save the current map canvas view, at its current size and extent, "
                "to an image file on disk together with a world file that "
                "georeferences it (for example .pgw next to a .png). format is "
                "required and must match the path suffix, png for .png, jpeg for "
                ".jpg or .jpeg, bmp for .bmp and tiff for .tif or .tiff. A mismatch "
                "or another suffix is an error. Returns path, world_file_path, "
                "encoder, saved, bytes_written and a cleanup report. Use it when "
                "the user wants the map view saved as a file. To look at the map "
                "yourself use capture_qgis_screenshot, which returns an image you "
                "can see and writes no file. For a finished map page with a map "
                "frame, title, legend and scale bar use create_print_layout and "
                "export_print_layout. Existing files are never replaced silently. If "
                "the image or its world file exists the call fails unless "
                "overwrite=true. With overwrite=true and no token nothing is written "
                "and a preview with overwrite_targets and a one-use "
                "overwrite_confirmation_token is returned. After the user approves, "
                "call again with the same path, format, overwrite=true and that "
                "token. The token is bound to the path and format."
            ),
            _export_map_image_schema(),
            "export_map_image",
            bridge,
        ),
        _capture_qgis_screenshot_tool(bridge),
    ] + build_interactive_layer_tools() + [
        _custom_bridge_tool(
            "create_vector_layer",
            (
                "Create a new vector layer with fields and optional initial features "
                "and add it to the current project by default. Without path, or with "
                'path "memory:", it is a memory layer, which is lost when QGIS '
                "closes, and format must be left out. To write a file give an "
                "absolute path and format, either GPKG (.gpkg), ESRI Shapefile (.shp) "
                "or GeoJSON (.geojson or .json). The path suffix must match format, "
                "otherwise the call fails, and the written file is loaded. Use it "
                "for new data, for example points the user "
                "describes or a table of results. To add features to an existing "
                "layer use add_vector_features. Initial features are written "
                "directly, not staged as edits. An invalid crs is an error. "
                "Returns added, layer (with id, null when add_to_project=false), "
                "path, feature_count and a cleanup report. Existing files are never "
                "replaced silently. If the target exists the call fails unless "
                "overwrite=true. With overwrite=true and no token nothing is written "
                "and a preview with a one-use overwrite_confirmation_token is "
                "returned. After the user approves, repeat the identical call with "
                "that token."
            ),
            _create_vector_layer_schema(),
            "create_vector_layer",
            bridge,
        ),
        _custom_bridge_tool(
            "add_vector_features",
            (
                "Add new features to an existing vector layer as staged edits in its "
                "edit buffer. The layer enters edit mode if needed, and nothing is "
                "written to the data source. A successful result reports "
                "staged=true, committed=false and requires_user_commit=true, plus "
                "added_count, added_feature_ids and feature_ids_verified. The user "
                "must review and save the layer edits in QGIS, and QGIS can renumber "
                "the added ids when they are saved. Each feature takes geometry_wkt "
                "in the layer CRS and attributes keyed by existing field names. All "
                "features are added in one undoable step or none at all. Use "
                "create_vector_layer for a new layer and update_vector_features to "
                "change existing features."
            ),
            _add_vector_features_schema(),
            "add_vector_features",
            bridge,
        ),
        _custom_bridge_tool(
            "update_vector_features",
            (
                "Change attribute values or geometries of existing features in a "
                "vector layer as staged edits in its edit buffer. Every updates item "
                "must include feature_id, such as the feature_id returned by "
                "query_vector_features, plus attributes keyed by existing field "
                "names, geometry_wkt in the layer CRS, or both. A successful result "
                "reports staged=true, committed=false and requires_user_commit=true, "
                "plus updated_feature_ids and updated_count. Nothing is written to "
                "the data source until the user saves the layer edits in QGIS. All "
                "updates apply in one undoable step or none at all. Unknown fields, "
                "missing features or a repeated feature_id reject the whole call. "
                "Use add_vector_features for new features and delete_vector_features "
                "to remove features."
            ),
            _update_vector_features_schema(),
            "update_vector_features",
            bridge,
        ),
    ]


def build_processing_tools(category: str) -> list[McpTool]:
    if category not in {"vector", "raster", "general"}:
        raise ValueError("category must be vector, raster or general")
    bridge = _bridge_client()

    def list_algorithms(arguments: dict[str, Any]) -> dict[str, Any]:
        payload: dict[str, Any] = {"category": category}
        if "cursor" in arguments:
            payload["cursor"] = arguments["cursor"]
        else:
            payload["max_results"] = arguments.get(
                "max_results",
                DEFAULT_PROCESSING_ALGORITHM_RESULTS,
            )
        # A stale cursor already comes back as a ToolError with the
        # processing_algorithm_cursor_stale code from the bridge.
        return bridge.call("processing_list_algorithms", payload)

    def algorithm_details(arguments: dict[str, Any]) -> dict[str, Any]:
        payload = dict(arguments)
        payload["category"] = category
        return bridge.call("processing_algorithm_details", payload)

    def start_algorithm(arguments: dict[str, Any]) -> dict[str, Any]:
        payload = dict(arguments)
        payload.setdefault("parameters", {})
        payload.setdefault("add_outputs_to_project", True)
        payload.setdefault("overwrite_outputs", False)
        payload["category"] = category
        return bridge.call("processing_start_algorithm", payload)

    def get_job(arguments: dict[str, Any]) -> dict[str, Any]:
        payload = dict(arguments)
        payload["category"] = category
        return bridge.call("processing_get_job", payload)

    def list_jobs(arguments: dict[str, Any]) -> dict[str, Any]:
        payload = dict(arguments)
        payload["category"] = category
        return bridge.call("processing_list_jobs", payload)

    def cancel_job(arguments: dict[str, Any]) -> dict[str, Any]:
        payload = dict(arguments)
        payload["category"] = category
        return bridge.call("processing_cancel_job", payload)

    list_scope = {
        "vector": (
            "List the QGIS Processing algorithms of the vector category, whose "
            "outputs are vector layers or tables, for example buffer, clip, "
            "dissolve, reproject layer, join attributes or polygonize (raster to "
            "vector). "
        ),
        "raster": (
            "List the QGIS Processing algorithms of the raster category, whose "
            "outputs are raster layers, for example warp (reproject), clip raster "
            "by extent or mask layer, slope, hillshade, reclassify or rasterize "
            "(vector to raster). "
        ),
        "general": (
            "List the QGIS Processing algorithms of the general category that "
            "QCopilots has audited as safe to start, for example network analysis "
            "(shortest path, service area), mesh, point cloud (PDAL), 3D tiles, "
            "cartography and print layout export, plots, package layers and export "
            "to spreadsheet. "
        ),
    }[category]
    list_policy = (
        "Only startable algorithms are listed, each with a stable audited "
        "classification such as network-safe-algorithm. Processing models, "
        "scripts, file management, download and SQL algorithms are not available. "
        if category == "general"
        else "Check start_policy.supported before starting one, because some listed "
        "algorithms cannot be started, such as in-place tools without a separate "
        "output. "
    )
    other_categories = {
        "vector": (
            "Algorithms that write rasters are in list_raster_processing_algorithms. "
            "Network analysis, mesh, point cloud, cartography and plot algorithms "
            "are in list_general_processing_algorithms. "
        ),
        "raster": (
            "Algorithms that write vector layers, such as polygonize, are in "
            "list_vector_processing_algorithms. Mesh, point cloud and plot "
            "algorithms are in list_general_processing_algorithms. "
        ),
        "general": (
            "Ordinary vector and raster tools are in "
            "list_vector_processing_algorithms and "
            "list_raster_processing_algorithms. "
        ),
    }[category]
    output_forms = {
        "vector": (
            "Output parameters take an absolute file path such as "
            'C:/data/result.gpkg, "memory:" for a memory layer or '
            '"TEMPORARY_OUTPUT". '
        ),
        "raster": (
            "Output parameters take an absolute file path such as "
            'C:/data/result.tif or "TEMPORARY_OUTPUT". '
        ),
        "general": (
            'Output parameters take an absolute file path, "memory:" or '
            '"TEMPORARY_OUTPUT". '
        ),
    }[category]

    return [
        McpTool(
            f"list_{category}_processing_algorithms",
            (
                list_scope
                + "Each entry has id, display_name, group, provider, tags and "
                "start_policy. "
                + list_policy
                + "Results come in pages of up to 50. While next_cursor is not null, "
                "pass it as cursor, without max_results, to get the next page. "
                "truncated=true means max_results was reached before the end. "
                + other_categories
                + f"Next, call get_{category}_processing_algorithm_details with the "
                "chosen id."
            ),
            _list_processing_algorithms_schema(),
            list_algorithms,
        ),
        McpTool(
            f"get_{category}_processing_algorithm_details",
            (
                f"Return the full definition of one {category} QGIS Processing "
                "algorithm without starting a job: parameters (name, description, "
                "type, optional, default, destination, enum_options, minimum, "
                "maximum, help), outputs, short_help, help_url and start_policy. "
                f"Use it before start_{category}_processing_algorithm to get the "
                "exact parameter names, such as INPUT and OUTPUT, and valid values. "
                "Parameters with destination=true are the outputs to set. For an "
                "enum parameter pass a value from enum_options. Policy support does "
                "not guarantee that optional provider dependencies are installed. "
                "An algorithm of another category is rejected, so use the details "
                "tool of the category that listed it."
            ),
            _processing_algorithm_id_schema(),
            algorithm_details,
        ),
        McpTool(
            f"start_{category}_processing_algorithm",
            (
                f"Start one {category} QGIS Processing algorithm as an asynchronous "
                "job and return its snapshot with job_id at once. Read the parameter "
                f"names first with get_{category}_processing_algorithm_details. "
                "Layer and file inputs take a project layer id, a unique layer "
                "name or an absolute file path. Relative input and output paths "
                "and parameter names that are not in the algorithm definition are "
                "rejected. "
                + output_forms
                + "Successful outputs are added to the current project by default. "
                f"Poll get_{category}_processing_job with the job_id until state is "
                "succeeded, failed or cancelled, then read the outputs from result. "
                "Existing output files are never replaced silently. With the "
                "default overwrite_outputs=false the job fails if an output file "
                "already exists. To replace files, first call with "
                "overwrite_outputs=true and no token. That starts only a preview "
                "job, which never runs the algorithm. If the succeeded preview "
                "reports confirmation_required=true, show overwrite_targets to the "
                "user. After approval call again with the "
                "same algorithm_id, parameters and add_outputs_to_project, "
                "overwrite_outputs=true and the one-use confirmation token from "
                "overwrite_confirmation_token before it expires. If "
                "confirmation_required=false, nothing would be replaced, so call "
                "again with overwrite_outputs=false and no token."
            ),
            _start_processing_algorithm_schema(),
            start_algorithm,
        ),
        McpTool(
            f"get_{category}_processing_job",
            (
                f"Return the full snapshot of one {category} Processing job: state "
                "(queued, running, cancelling, succeeded, failed or cancelled), "
                "operation (preview or execute), phase (preflight, execute, "
                "verify_outputs, publish, load_outputs, cleanup or complete), "
                "progress_percent, progress_text, algorithm_executed, cancellable, "
                "result, error and cleanup. Poll it after "
                f"start_{category}_processing_algorithm until state is succeeded, "
                "failed or cancelled. For an execution, result holds the algorithm "
                "outputs keyed by output name. A succeeded preview has "
                "algorithm_executed=false and only supplies confirmation "
                "information: confirmation_required, overwrite_targets and, when "
                "needed, overwrite_confirmation_token. Algorithm progress reaching "
                "100 percent does not mean output verification, publication or "
                "cleanup has finished. On failure read error.code, error.message and "
                "error.feedback_messages. If cleanup.complete is false, polling again "
                "retries the recovery. A job_id of another category is not found."
            ),
            _processing_job_schema(),
            get_job,
        ),
        McpTool(
            f"list_{category}_processing_jobs",
            (
                f"List recent {category} Processing jobs of this QGIS session, "
                "newest first. Each summary has job_id, state, operation (preview "
                "or execute), phase, progress_percent, progress_text, algorithm, "
                "cancellable, timestamps and error. Use it to find a lost job_id or "
                "to check what is still running before starting more work. "
                "Summaries omit result and cleanup, so call "
                f"get_{category}_processing_job for outputs. Finished jobs are kept "
                "for up to 24 hours."
            ),
            _list_processing_jobs_schema(),
            list_jobs,
        ),
        McpTool(
            f"cancel_{category}_processing_job",
            (
                f"Request cancellation of one queued or running {category} "
                "Processing job. It only works while the job reports "
                "cancellable=true. Loading outputs into the project, cleanup and "
                "algorithms that cannot be interrupted are not cancellable and "
                "return a job_not_cancellable error. A finished job is returned "
                "unchanged. The job moves to cancelling. Poll "
                f"get_{category}_processing_job until it reaches a final state, "
                "which can still be succeeded or failed if the job was already "
                "finishing. Cancellation during output publication rolls back the "
                "files already published, so check that cleanup.complete is true "
                "before treating output files as removed or restored."
            ),
            _processing_job_schema(),
            cancel_job,
        ),
    ]


def _bridge_tool(
    name: str,
    description: str,
    properties: dict[str, str],
    bridge: BridgeClient,
    required: list[str] | None = None,
    property_descriptions: dict[str, str] | None = None,
) -> McpTool:
    def handler(arguments: dict[str, Any]) -> dict[str, Any]:
        return bridge.call(name, arguments)

    return McpTool(
        name,
        description,
        _schema(properties, required or [], property_descriptions),
        handler,
    )


def _custom_bridge_tool(
    name: str,
    description: str,
    input_schema: dict[str, Any],
    bridge_tool_name: str,
    bridge: BridgeClient,
) -> McpTool:
    def handler(arguments: dict[str, Any]) -> dict[str, Any]:
        return bridge.call(bridge_tool_name, arguments)

    return McpTool(name, description, input_schema, handler)


def _capture_qgis_screenshot_tool(bridge: BridgeClient) -> McpTool:
    def handler(arguments: dict[str, Any]) -> Any:
        return screenshot.capture_screenshot_via_bridge(bridge.call, arguments)

    return McpTool(
        screenshot.TOOL_NAME,
        (
            "Take a screenshot of QGIS and return it as an image you can see, "
            "with a plain-text summary of the view. Use it to look at the map, "
            "check styling, labels, layer order, print layouts and open dialogs "
            "before and after changes, then describe what you saw. Targets: "
            "map_canvas (default, only the map), main_window (the whole QGIS "
            "window with panels and toolbars), active_window (the focused dialog "
            "or window, falls back to the main window) and layout_designer (an "
            "open print layout designer window, requires layout_name). The "
            "QCopilots chat panel is included by default. Set "
            "include_qcopilots_panel=false to cover it. capture_method widget "
            "(default) draws QGIS itself and never shows other applications, but "
            "3D views can appear blank. capture_method screen copies screen "
            "pixels, so it can show 3D views, popup menus and title bars, but "
            "also any other application window that overlaps QGIS. Use screen "
            "only when the user explicitly asks for it. region crops "
            "[x, y, width, height] in logical pixels of the target, and the "
            "image is downscaled so its long edge is at most max_dimension. The "
            "tool waits up to wait_for_render_ms for the map canvas to finish "
            "rendering."
        ),
        _capture_qgis_screenshot_schema(),
        handler,
    )


def _bridge_client() -> BridgeClient:
    return BridgeClient(os.environ.get(BRIDGE_URL_ENV))


def _tool_name(algorithm_id: str) -> str:
    return "processing_" + re.sub(r"[^a-zA-Z0-9]+", "_", algorithm_id).strip("_").lower()


def _call_optional(target: Any, name: str) -> Any:
    value = getattr(target, name, None)
    if callable(value):
        return value()
    return value


def _schema(
    properties: dict[str, str],
    required: list[str],
    descriptions: dict[str, str] | None = None,
) -> dict[str, Any]:
    converted = {}
    for name, schema_type in properties.items():
        if schema_type == "array":
            converted[name] = {"type": "array", "items": {"type": "number"}}
        elif schema_type == "object":
            converted[name] = {"type": "object"}
        else:
            converted[name] = {"type": schema_type}
            if schema_type == "string":
                converted[name]["minLength"] = 1
        if descriptions and name in descriptions:
            converted[name]["description"] = descriptions[name]
    return {
        "type": "object",
        "properties": converted,
        "required": required,
        "additionalProperties": False,
    }


def _start_processing_algorithm_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "algorithm_id": {
                "type": "string",
                "minLength": 1,
                "description": "Exact algorithm ID returned by the list tool of the same category.",
            },
            "parameters": {
                "type": "object",
                "default": {},
                "description": (
                    "Algorithm parameter values keyed by the parameter names from "
                    'the details tool, for example {"INPUT": "<layer id>", '
                    '"OUTPUT": "C:/data/result.gpkg"}. Destination '
                    "parameters (outputs and feature sinks) accept an absolute file "
                    'path, "memory:" or "TEMPORARY_OUTPUT". Relative paths are '
                    "rejected because they would be written into the QGIS program "
                    "folder. GDAL virtual paths such as /vsimem/ or /vsizip/ are "
                    "rejected as destinations because QCopilots cannot stage, "
                    "publish or reopen them."
                ),
            },
            "add_outputs_to_project": {
                "type": "boolean",
                "default": True,
                "description": (
                    "Add compatible successful outputs to the current QGIS project "
                    "during job post-processing. Set false to only write the output "
                    "files, and then prefer an absolute file path as output."
                ),
            },
            "overwrite_outputs": {
                "type": "boolean",
                "default": False,
                "description": (
                    "False without a token starts execution without overwriting. "
                    "True without a token starts an asynchronous preview only. "
                    "True with the preview token starts a confirmed execution. "
                    "False with a token is invalid. Folder outputs remain unsupported."
                ),
            },
            "overwrite_confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": (
                    "One-use token obtained by polling a completed overwrite preview. "
                    "Requires overwrite_outputs=true and is bound to the category, "
                    "algorithm, parameters, project-loading option, input versions "
                    "and output targets. Submit before its expiry. Once claimed, "
                    "background verification does not restart the expiry check."
                ),
            },
            "client_request_id": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
                "description": (
                    "Optional idempotency key. Preview and execution have separate "
                    "namespaces, so the same key can be used for both steps. Replaying "
                    "a preview returns its original result and token. Use a new key "
                    "to obtain a new preview after the token expires."
                ),
            },
        },
        "required": ["algorithm_id"],
        "additionalProperties": False,
    }


def _processing_job_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "job_id": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
                "description": "job_id returned by the start tool of the same category.",
            },
        },
        "required": ["job_id"],
        "additionalProperties": False,
    }


def _list_processing_jobs_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "states": {
                "type": "array",
                "items": {
                    "type": "string",
                    "enum": [
                        "queued",
                        "running",
                        "cancelling",
                        "succeeded",
                        "failed",
                        "cancelled",
                    ],
                },
                "minItems": 1,
                "uniqueItems": True,
                "description": (
                    'Only return jobs in these states, for example ["running", '
                    '"cancelling"]. Omit for all states.'
                ),
            },
            "limit": {
                "type": "integer",
                "minimum": 1,
                "maximum": 200,
                "description": "Maximum number of jobs to return, newest first. Defaults to 100.",
            },
        },
        "additionalProperties": False,
    }


def _create_vector_layer_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "name": {
                "type": "string",
                "minLength": 1,
                "description": "Layer name.",
            },
            "geometry_type": {
                "type": "string",
                "enum": ["Point", "LineString", "Polygon", "MultiPoint", "MultiLineString", "MultiPolygon", "NoGeometry"],
                "default": "Point",
                "description": "Geometry type of the new layer. NoGeometry creates an attribute table without geometry.",
            },
            "crs": {
                "type": "string",
                "minLength": 1,
                "default": "EPSG:4326",
                "description": (
                    "Layer CRS as an authority id such as EPSG:4326 or EPSG:3857. "
                    "A value QGIS cannot read is an error. geometry_wkt "
                    "coordinates of the features must be in this CRS."
                ),
            },
            "fields": {
                "type": "array",
                "items": _field_schema(),
                "default": [],
                "description": "Attribute fields to create, each with a name and a type.",
            },
            "features": {
                "type": "array",
                "items": _feature_schema(),
                "default": [],
                "description": (
                    "Initial features, each with optional geometry_wkt and attributes "
                    "keyed by the field names above."
                ),
            },
            "path": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Optional absolute output file path. Relative paths are rejected. "
                    "Its suffix must match format: .gpkg, .shp, .geojson or .json. "
                    'Omit it or pass "memory:" for a memory layer.'
                ),
            },
            "format": {
                "type": "string",
                "enum": ["GPKG", "ESRI Shapefile", "GeoJSON"],
                "description": (
                    "OGR driver of the output file. Required when path is a file "
                    "path, and it must match the path suffix. Leave it out for a "
                    "memory layer."
                ),
            },
            "add_to_project": {
                "type": "boolean",
                "default": True,
                "description": (
                    "Add the new layer to the current project. Set false only with "
                    "a file path, to write a file without loading it. false for a "
                    "memory layer is an error."
                ),
            },
            "overwrite": {
                "type": "boolean",
                "default": False,
                "description": (
                    "Explicitly allow replacement of an existing output dataset. "
                    "Without a token it only returns an overwrite preview."
                ),
            },
            "overwrite_confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": "One-use token returned by an overwrite preview.",
            },
        },
        "required": ["name"],
        "additionalProperties": False,
    }


def _add_vector_features_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "layer_id": {
                "type": "string",
                "minLength": 1,
                "description": "Id of an existing vector layer from list_layers, not the layer name.",
            },
            "features": {
                "type": "array",
                "items": _feature_schema(),
                "minItems": 1,
                "description": (
                    "Features to stage in the layer edit buffer. The successful "
                    "result remains uncommitted and requires a user commit."
                ),
            },
        },
        "required": ["layer_id", "features"],
        "additionalProperties": False,
    }


def _processing_algorithm_id_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "algorithm_id": {
                "type": "string",
                "minLength": 1,
                "description": "Exact algorithm ID returned by the matching list tool.",
            },
        },
        "required": ["algorithm_id"],
        "additionalProperties": False,
    }


def _list_processing_algorithms_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "max_results": {
                "type": "integer",
                "minimum": 1,
                "maximum": MAX_PROCESSING_ALGORITHM_RESULTS,
                "default": DEFAULT_PROCESSING_ALGORITHM_RESULTS,
                "description": (
                    "Maximum number of algorithms to return across all pages. "
                    "Omit for the default limit."
                ),
            },
            "cursor": {
                "type": "string",
                "minLength": 1,
                "maxLength": MAX_PROCESSING_ALGORITHM_CURSOR_LENGTH,
                "description": (
                    "Opaque continuation cursor, the next_cursor of the previous "
                    "page. Do not send max_results with cursor, the cursor keeps "
                    "the limit of the first page."
                ),
            },
        },
        "oneOf": [
            {
                "title": "First page",
                "properties": {"cursor": False},
            },
            {
                "title": "Continuation page",
                "required": ["cursor"],
                # Clients that fill in schema defaults may add the default
                # max_results, which the continuation ignores.
                "properties": {
                    "max_results": {"const": DEFAULT_PROCESSING_ALGORITHM_RESULTS}
                },
            },
        ],
        "additionalProperties": False,
    }


def _save_project_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "path": {
                "type": "string",
                "minLength": 1,
                "pattern": ".*\\.[qQ][gG][zZ]$",
                "description": (
                    "Optional absolute .qgz project destination. Relative paths are "
                    "rejected. Plain .qgs projects are "
                    "unsupported because their auxiliary file family cannot be "
                    "published atomically as one output. Omit it, or pass the "
                    "current project file, to save in place without a token."
                ),
            },
            "overwrite": {
                "type": "boolean",
                "default": False,
                "description": (
                    "Allow replacing an existing file other than the current project "
                    "file. Without a token it only returns an overwrite preview. Not "
                    "needed when saving to the current project file."
                ),
            },
            "overwrite_confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": "One-use token returned by an overwrite preview.",
            },
        },
        "additionalProperties": False,
    }


def _export_map_image_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "path": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Absolute output image path ending in .png, .jpg, .jpeg, .bmp, "
                    ".tif or .tiff, matching format. Relative paths are rejected."
                ),
            },
            "format": {
                "type": "string",
                "enum": ["png", "jpeg", "bmp", "tiff"],
                "description": (
                    "Image format. It must match the path suffix, jpeg for .jpg or "
                    ".jpeg and tiff for .tif or .tiff."
                ),
            },
            "overwrite": {
                "type": "boolean",
                "default": False,
                "description": (
                    "Allow replacing an existing image or world file. Without a token "
                    "it only returns an overwrite preview."
                ),
            },
            "overwrite_confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": "One-use token returned by an overwrite preview.",
            },
        },
        "required": ["path", "format"],
        "additionalProperties": False,
    }


def _capture_qgis_screenshot_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "target": {
                "type": "string",
                "enum": list(screenshot.TARGETS),
                "default": screenshot.DEFAULT_TARGET,
                "description": (
                    "What to capture: map_canvas (only the map), main_window, "
                    "active_window or layout_designer."
                ),
            },
            "layout_name": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Print layout name. Required when target is layout_designer, "
                    "and that layout must be open in a layout designer window."
                ),
            },
            "region": {
                "type": "array",
                # items keeps a simple form for clients without prefixItems.
                "items": {"type": "integer", "minimum": 0},
                "prefixItems": [
                    {"type": "integer", "minimum": 0},
                    {"type": "integer", "minimum": 0},
                    {"type": "integer", "minimum": 1},
                    {"type": "integer", "minimum": 1},
                ],
                "minItems": 4,
                "maxItems": 4,
                "description": (
                    "Optional crop [x, y, width, height] in logical pixels "
                    "relative to the captured target. width and height must be "
                    "at least 1, and the region must lie inside the target's "
                    "logical_size."
                ),
            },
            "include_qcopilots_panel": {
                "type": "boolean",
                "default": screenshot.DEFAULT_INCLUDE_QCOPILOTS_PANEL,
                "description": (
                    "Include the docked QCopilots chat panel. False covers it "
                    "with a solid block."
                ),
            },
            "capture_method": {
                "type": "string",
                "enum": list(screenshot.CAPTURE_METHODS),
                "default": screenshot.DEFAULT_CAPTURE_METHOD,
                "description": (
                    "widget draws QGIS itself. screen copies screen pixels and "
                    "can include other applications overlapping QGIS, so use it "
                    "only when the user explicitly asks."
                ),
            },
            "max_dimension": {
                "type": "integer",
                "minimum": screenshot.MIN_MAX_DIMENSION,
                "maximum": screenshot.MAX_MAX_DIMENSION,
                "default": screenshot.DEFAULT_MAX_DIMENSION,
                "description": "Longest image edge in pixels after downscaling.",
            },
            "format": {
                "type": "string",
                "enum": list(screenshot.IMAGE_FORMATS),
                "default": screenshot.DEFAULT_IMAGE_FORMAT,
                "description": (
                    "auto uses PNG and switches to JPEG when the PNG is larger "
                    "than 1 MiB."
                ),
            },
            "jpeg_quality": {
                "type": "integer",
                "minimum": screenshot.MIN_JPEG_QUALITY,
                "maximum": screenshot.MAX_JPEG_QUALITY,
                "default": screenshot.DEFAULT_JPEG_QUALITY,
                "description": (
                    "Quality used when the image is encoded as JPEG. It can be "
                    "lowered automatically to fit the size limit."
                ),
            },
            "wait_for_render_ms": {
                "type": "integer",
                "minimum": 0,
                "maximum": screenshot.MAX_WAIT_FOR_RENDER_MS,
                "default": screenshot.DEFAULT_WAIT_FOR_RENDER_MS,
                "description": (
                    "How long to wait for the map canvas to finish rendering "
                    "before capturing it anyway."
                ),
            },
        },
        "additionalProperties": False,
    }


def _update_vector_features_schema() -> dict[str, Any]:
    feature_id_schema = {
        "type": ["integer", "string"],
        "description": (
            "Required existing feature identifier. Copy the feature_id from "
            "query_vector_features. For example, use 1 to update feature 1."
        ),
        "examples": [1],
    }
    update_properties = {
        "feature_id": feature_id_schema,
        "attributes": {
            "type": "object",
            "minProperties": 1,
            "additionalProperties": True,
            "description": "New values keyed by existing field name.",
        },
        "geometry_wkt": {
            "type": "string",
            "minLength": 1,
            "description": "Replacement geometry as WKT in the layer CRS.",
        },
    }
    update_schema = {
        "anyOf": [
            {
                "type": "object",
                "properties": update_properties,
                "required": ["feature_id", "attributes"],
                "additionalProperties": False,
            },
            {
                "type": "object",
                "properties": update_properties,
                "required": ["feature_id", "geometry_wkt"],
                "additionalProperties": False,
            },
        ]
    }
    return {
        "type": "object",
        "properties": {
            "layer_id": {
                "type": "string",
                "minLength": 1,
                "description": "Id of an existing vector layer from list_layers, not the layer name.",
            },
            "updates": {
                "type": "array",
                "items": update_schema,
                "minItems": 1,
                "description": (
                    "Updates to stage. Every item requires feature_id and at least "
                    "one non-empty attributes object or geometry_wkt value. Example: "
                    '[{"feature_id": 1, "attributes": {"name": "Roads"}}].'
                ),
                "examples": [
                    [
                        {
                            "feature_id": 1,
                            "attributes": {"name": "Roads"},
                        }
                    ]
                ],
            },
        },
        "required": ["layer_id", "updates"],
        "additionalProperties": False,
    }


def _field_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "name": {"type": "string", "minLength": 1, "description": "Field name."},
            "type": {
                "type": "string",
                "enum": ["string", "int", "integer", "long", "double", "float", "bool", "boolean", "date", "datetime"],
                "default": "string",
                "description": "Field type. long is a 64-bit integer, double and float are decimals.",
            },
        },
        "required": ["name"],
        "additionalProperties": False,
    }


def _feature_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "geometry_wkt": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Geometry as WKT in the layer CRS, for example POINT(13.4 52.5). "
                    "Omit it for a feature without geometry."
                ),
            },
            "attributes": {
                "type": "object",
                "additionalProperties": True,
                "default": {},
                "description": "Values keyed by existing field name. Unknown field names are rejected.",
            },
        },
        "additionalProperties": False,
    }
