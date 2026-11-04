"""MCP tool descriptors for QGIS interactive layer operations.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import os
from typing import Any

from qcopilots_common.bridge import BridgeClient
from qcopilots_common.bridge import MAX_VECTOR_QUERY_OFFSET
from qcopilots_common.constants import BRIDGE_URL_ENV
from qcopilots_common.mcp_http import McpTool


LAYER_TYPE_VALUES = [
    "auto",
    "vector",
    "raster",
    "mesh",
    "vector_tile",
    "point_cloud",
    "tiled_scene",
    "xyz",
    "wms",
    "wcs",
    "arcgis_feature_server",
    "arcgis_map_server",
    "arcgis_image_server",
]


def build_interactive_layer_tools() -> list[McpTool]:
    bridge = _bridge_client()
    return [
        _bridge_tool(
            "describe_layer_sources",
            (
                "List the layer source types that load_map_layer accepts and the "
                "QGIS data providers installed in this QGIS. Returns "
                "supported_layer_types (each layer_type with its provider keys) "
                "and available_providers. Call it before load_map_layer when you "
                "are unsure which layer_type or provider fits a source, for "
                "example a vector tile service, a point cloud or a 3D tiles "
                "scene. It does not list the layers of the project, use "
                "list_layers for that. Read-only, no network access."
            ),
            {"type": "object", "properties": {}, "additionalProperties": False},
            "interactive_layer_describe_sources",
            bridge,
        ),
        _bridge_tool(
            "load_map_layer",
            (
                "Load one layer into QGIS from a local file, a provider URI, an "
                "XYZ tile URL or an OGC or ArcGIS service URL, and by default add "
                "it to the current project. Returns added, layer (id, name, "
                "source, provider, valid), layer_type, provider and source. Use "
                "it to open data such as a GeoPackage, shapefile, GeoTIFF, "
                "OpenStreetMap tile URL or WMS layer. Check list_layers first "
                "to avoid loading a layer twice, and use describe_layer_sources "
                "to see layer types and installed providers. layer_type auto "
                "works only for local files and tries the vector, raster, mesh, "
                "point cloud, vector tile and tiled scene providers in turn, so "
                "it takes no provider unless the provider names a layer type "
                "such as pdal. "
                "Remote URLs and provider URIs need an explicit layer_type. With "
                "a plain service URL, WMS needs uri_options.layers and WCS needs "
                "uri_options.identifier. Relative local paths, also inside "
                "file: URIs and provider URIs, are rejected. An invalid "
                "source returns an error and adds nothing. Loading "
                "does not zoom, call zoom_to_layer with the returned layer id to "
                "show it."
            ),
            _load_layer_schema(),
            "interactive_layer_load_layer",
            bridge,
        ),
        _bridge_tool(
            "remove_map_layers",
            (
                "Remove whole layers from the current QGIS project in two calls. "
                "Files and databases are not deleted, but memory layers lose "
                "their features. Step 1 (preview): select layers with layer_id, "
                "layer_ids, name, names, source or sources, optionally with "
                "allow_multiple and editable_changes. Nothing is removed yet. The "
                "preview returns the matching layers with their edit_state and a "
                "confirmation_token. Step 2 (confirm): after the user agrees, "
                "send only confirmation_token. The token is one-use, expires "
                "after confirmation_expires_in_seconds and fails if a layer or "
                "its pending edits changed after the preview. Prefer layer ids "
                "from list_layers. When ids are given, name and source "
                "selectors are ignored, and an id that is not in the project is "
                "an error. Name and source together select only layers that "
                "match both, and a listed name that matches no layer is an "
                "error. A name or source matching several layers "
                "is an error unless allow_multiple=true. At most 64 layers per "
                "removal. Layers in an edit session follow editable_changes, "
                "chosen at preview time. Editable changes are rejected by "
                "default and are never silently discarded. Returns removed, "
                "removed_count and editable_layers_processed. Use "
                "delete_vector_features to delete features inside a layer and "
                "set_layer_visibility to only hide a layer."
            ),
            _remove_layers_schema(),
            "interactive_layer_remove_layers",
            bridge,
        ),
        _bridge_tool(
            "get_layer_metadata",
            (
                "Describe one layer without reading feature values. Returns layer, "
                "an object with id, name, type, provider, valid, crs (valid, auth_id, "
                "description, geographic, wkt, wkt_truncated, proj, "
                "proj_truncated, where long texts are cut and flagged), extent "
                "[xmin, ymin, xmax, ymax] in the "
                "layer CRS and renderer_type. Vector layers add geometry_type, "
                "feature_count, selected_feature_count, fields (name, type, "
                "alias, length, precision), editable and labels_enabled. Raster "
                "layers add band_count, width, height, pixel_size_x, pixel_size_y "
                "and band_names. Use it to learn exact field names before writing "
                "an expression, a renderer or labels, or to check the CRS of a "
                "layer. Use query_vector_features to read attribute values and "
                "get_raster_statistics for raster value statistics. The source "
                "is omitted unless include_source=true. Read-only."
            ),
            _layer_id_schema(
                {
                    "include_source": {
                        "type": "boolean",
                        "default": False,
                        "description": (
                            "Also return the layer source, which may contain "
                            "local paths or service URLs."
                        ),
                    }
                }
            ),
            "interactive_layer_get_metadata",
            bridge,
        ),
        _bridge_tool(
            "query_vector_features",
            (
                "Read a bounded page of vector feature attributes and optional "
                "geometry. Returns features (each with feature_id, attributes "
                "and, if requested, geometry_wkt), returned_count, offset, limit "
                "and truncated. Use it to answer questions about attribute "
                "values, to page through a table or to find the feature ids "
                "needed by update_vector_features, set_vector_selection or "
                "delete_vector_features. Use get_layer_metadata instead for the "
                "field list, geometry type, feature count, CRS or extent. Filter "
                "with feature_ids, filter_expression or selected_only. When "
                "several filters are given, a feature must match all of them, and "
                "listed feature_ids that do not exist are skipped. "
                "Features are ordered by feature id. When truncated is true, call "
                "again with offset increased by returned_count (offset at most "
                "10000). Geometry WKT is in the layer CRS. A page with more than "
                "1 MiB of geometry or 2 MiB in total returns an error, so request "
                "only the fields you need and a small limit when include_geometry "
                "is true. Read-only."
            ),
            _query_vector_features_schema(),
            "interactive_layer_query_features",
            bridge,
        ),
        _bridge_tool(
            "set_vector_selection",
            (
                "Change which features are selected in one vector layer and "
                "refresh the map. mode replace (default) selects exactly the "
                "matching features, add adds them to the current selection, "
                "remove unselects them and clear empties the selection. Match "
                "features with feature_ids, "
                "filter_expression or both, in which case a feature must match "
                "both. Returns matched_count, selected_count and up to 100 "
                "selected_feature_ids with selected_feature_ids_truncated. Use it "
                "to highlight features for the user or before "
                "query_vector_features with selected_only=true. Selecting does "
                "not edit data and does not move the map, call zoom_to_selection "
                "afterwards to zoom to the selected features. One call may match "
                "at most 10000 features, and add or remove need a current "
                "selection of at most 10000. Listed feature_ids that do not exist "
                "are an error unless filter_expression is also given. For clear "
                "send only layer_id and mode=clear."
            ),
            _set_vector_selection_schema(),
            "interactive_layer_set_selection",
            bridge,
        ),
        _bridge_tool(
            "delete_vector_features",
            (
                "Preview an exact vector feature deletion, then stage it once with "
                "the returned short-lived confirmation_token. Step 1 (preview): "
                "send layer_id with feature_ids, filter_expression or both. "
                "Nothing is deleted. The preview returns matched_count, up to 100 "
                "feature_ids and a confirmation_token, or confirmation_token=null "
                "when nothing matches. Show the user what will be deleted. Step 2 "
                "(confirm): after the user agrees, send only layer_id and that "
                "confirmation_token. The token expires after "
                "confirmation_expires_in_seconds and is bound to the layer and to "
                "the attributes and geometry of the previewed features, so any "
                "change in between needs a new preview. One preview may match at "
                "most 10000 features. Listed feature_ids that do not exist are "
                "an error unless filter_expression is also given, in which case "
                "a feature must match both. The deletion goes into the layer edit "
                "buffer and starts an edit session if needed. A successful "
                "confirmed deletion reports staged=true, committed=false and "
                "requires_user_commit=true: the data source changes only when "
                "the user saves the layer edits in QGIS, and rolling back the "
                "edits undoes it. Use set_vector_selection to only highlight "
                "features and remove_map_layers to remove a whole layer."
            ),
            _delete_vector_features_schema(),
            "interactive_layer_delete_features",
            bridge,
        ),
        _bridge_tool(
            "get_project_crs",
            (
                "Return the current QGIS project CRS as crs with valid, auth_id "
                "(for example EPSG:3857), description, geographic, wkt and proj. "
                "The map canvas and print layout map frames use this CRS, so "
                "check it before passing coordinates to zoom_to_extent without "
                "crs or to create_print_layout map_extent. Use get_layer_metadata "
                "for the CRS of one layer. Read-only."
            ),
            {"type": "object", "properties": {}, "additionalProperties": False},
            "interactive_project_get_crs",
            bridge,
        ),
        _bridge_tool(
            "set_project_crs",
            (
                "Set the current QGIS project CRS from an authority id or WKT and "
                "return previous_crs and crs. The map canvas is then drawn in "
                "this CRS and layers are reprojected on the fly, but layer data "
                "and each layer's own CRS do not change. Use it when the user "
                "wants the map shown in another projection, for example "
                "EPSG:3857 for web maps or a UTM zone for a local map in metres. "
                "To reproject the data itself, run a Processing algorithm such "
                "as native:reprojectlayer with start_vector_processing_algorithm. "
                "An invalid CRS returns an error and changes nothing. The project "
                "file is not saved, use save_project to keep the change."
            ),
            {
                "type": "object",
                "properties": {
                    "crs": {
                        "type": "string",
                        "minLength": 1,
                        "description": (
                            "Target CRS as an authority id such as EPSG:4326 or "
                            "EPSG:32633, or a full WKT definition."
                        ),
                    }
                },
                "required": ["crs"],
                "additionalProperties": False,
            },
            "interactive_project_set_crs",
            bridge,
        ),
        _bridge_tool(
            "get_raster_statistics",
            (
                "Calculate bounded QGIS raster band statistics for one band over "
                "the full layer extent. Returns minimum, maximum, range, mean, "
                "standard_deviation, sum, sum_of_squares and element_count, plus "
                "band, sample_size, sample_width, sample_height and extent. Use "
                "it for questions such as the elevation range or mean value of a "
                "DEM. QGIS samples about sample_size cells, so on large rasters "
                "the values come from a sample and are approximate. Raise "
                "sample_size for more precision. Non-finite values are returned "
                "as null. Only raster layers are accepted. Use get_layer_metadata "
                "for band count, size and pixel size. Read-only."
            ),
            _raster_statistics_schema(),
            "interactive_raster_statistics",
            bridge,
        ),
        _bridge_tool(
            "apply_layer_style",
            (
                "Apply an existing local QML or SLD style file to a QGIS layer, "
                "for example a style the user saved from QGIS or received with "
                "the data. The file suffix selects the format and the file is "
                "not modified. SLD files use the QGIS SLD reader. QML files are "
                "read from the file itself, never from a style stored in the "
                "data source, and only symbology and labeling are imported, so "
                "fields, forms and actions are kept. Labels are replaced only "
                "when the file declares them (an SLD TextSymbolizer, or a QML "
                "<labeling> element or labelsEnabled attribute). Otherwise the "
                "layer keeps its current labels. For vector layers a QML "
                "<renderer-v2> must use a registered renderer type and every "
                "<category> or <range> symbol attribute must match a <symbol "
                "name> in <symbols>. Invalid or empty renderers return an error "
                "and the previous renderer and labels are restored. For vector "
                "layers the result reports the applied renderer type, field, "
                "class count and graduated ranges, and the labeling state "
                "(enabled, field and source style_file or kept_previous). Use "
                "set_vector_renderer to style without a file and "
                "configure_vector_labels for simple labels."
            ),
            _apply_layer_style_schema(),
            "interactive_layer_apply_style",
            bridge,
        ),
        _bridge_tool(
            "set_vector_renderer",
            (
                "Set a single symbol, categorized or graduated renderer on a "
                "vector layer without a style file, replacing its current "
                "renderer. Labels are not changed. Use it for requests such as "
                "color the countries by continent (categorized) or shade the "
                "districts by population density (graduated). Use "
                "apply_layer_style instead when there is a QML or SLD file, and "
                "configure_vector_labels for labels. Categorized mode uses the "
                "unique values of a field or expression, at most 256 values. "
                "Graduated mode classifies a numeric field or expression with "
                "equal_interval, quantile, jenks, pretty or stddev into 2 to 12 "
                "classes, or with explicit ascending breaks. Colors come from a "
                "QGIS default color ramp. Each mode accepts only its own options, "
                "so for example color with categorized mode is an error, while "
                "method, classes and invert_ramp at their defaults are ignored. The "
                "layer must have geometry. The result reports the renderer type, "
                "field, class count and graduated ranges, and graduated mode adds "
                "value_range and warnings when breaks leave values out."
            ),
            _set_vector_renderer_schema(),
            "interactive_layer_set_renderer",
            bridge,
        ),
        _bridge_tool(
            "configure_vector_labels",
            (
                "Enable or disable simple field or expression labels for a vector "
                "layer. With enabled=true (default) it replaces the layer's "
                "labeling with one simple label style that draws "
                "field_or_expression with font_size and color, so existing "
                "rule-based or styled labels are lost. With enabled=false it just "
                "switches labels off, and the other label options must be omitted "
                "or left at their defaults. Use it for "
                "quick labels such as label the cities with the name field. Use "
                "apply_layer_style instead for labels designed in a QML or SLD "
                "file, and set_vector_renderer to change symbols. "
                "field_or_expression must be an exact field name unless "
                "is_expression=true. Every field used in an expression must "
                "exist. A color with an alpha (#RRGGBBAA) sets the text opacity. "
                "Returns labels_enabled, field_or_expression (trimmed, null when "
                "labels are switched off) and is_expression."
            ),
            _configure_vector_labels_schema(),
            "interactive_layer_configure_labels",
            bridge,
        ),
        _bridge_tool(
            "create_print_layout",
            (
                "Create a constrained one-page print layout in the current "
                "project with an optional title, one map frame, an optional "
                "legend and an optional numeric scale bar (such as 1:25000), "
                "placed automatically inside 10 mm page margins. Returns created "
                "and layout (name, page_size, orientation, title, "
                "map_extent_source, has_legend, has_scale_bar). Use it when the "
                "user wants a printable or "
                "shareable map, then call export_print_layout with the same name. "
                "It cannot change an existing layout or add other items such as "
                "a north arrow. The map frame draws the layers visible in the "
                "project. map_extent is in the project CRS (see get_project_crs) "
                "and the frame may show a larger area to keep its shape. "
                "Without map_extent the current map canvas extent is used, so "
                "zoom_to_layer or zoom_to_extent first is an easy way to frame "
                "the map. map_extent_source tells which extent was used: "
                "map_extent, canvas, or default when there was no canvas extent. "
                "Extents from get_layer_metadata are in the layer CRS, and "
                "map_extent needs xmin < xmax and ymin < ymax. "
                "The name is trimmed, must not be blank and must not exist yet "
                "(see list_print_layouts). If the "
                "legend cannot fit, choose a larger page or set "
                "include_legend=false. The layout is not opened in a designer and "
                "the project is not saved."
            ),
            _create_print_layout_schema(),
            "interactive_layout_create",
            bridge,
        ),
        _bridge_tool(
            "list_print_layouts",
            (
                "List print layouts in the current QGIS project. Returns layouts, "
                "each with name and page_count. Use it to find the layout_name "
                "for export_print_layout or for capture_qgis_screenshot with "
                "target layout_designer, or to check that a name is free before "
                "create_print_layout. Read-only."
            ),
            {"type": "object", "properties": {}, "additionalProperties": False},
            "interactive_layout_list",
            bridge,
        ),
        _bridge_tool(
            "export_print_layout",
            (
                "Export one print layout of the project to a PDF, PNG, JPEG or "
                "SVG file at the chosen dpi, with explicit no-overwrite behavior. "
                "Returns layout_name, path, format, dpi, saved and bytes_written. "
                "Use it for a finished map with the layout's title, legend and "
                "scale bar, for example after create_print_layout. Use "
                "export_map_image instead to save only the current map canvas as "
                "an image, and capture_qgis_screenshot to look at QGIS yourself "
                "without writing a file. path must be absolute (relative paths "
                "are rejected). format is required and must match the path "
                "suffix, pdf for .pdf, png for .png, jpeg for .jpg or .jpeg and "
                "svg for .svg. A mismatch or another suffix is an error. Layouts "
                "with more than one page can only be exported as PDF. The export "
                "is limited to 25 million pixels, so large pages such as A2, A1 "
                "or A0 need a lower dpi than the default 300. Missing parent "
                "folders are created. An existing file is never replaced "
                "silently: without overwrite the call fails, overwrite=true "
                "without a token only returns a preview with "
                "overwrite_confirmation_token and writes nothing, and repeating "
                "the same call with overwrite=true and that one-use token "
                "replaces the file. The token is bound to the layout name, path, "
                "format, dpi and the current file content."
            ),
            _export_print_layout_schema(),
            "interactive_layout_export",
            bridge,
        ),
    ]


def _bridge_tool(
    name: str,
    description: str,
    input_schema: dict[str, Any],
    bridge_tool_name: str,
    bridge: BridgeClient,
) -> McpTool:
    def handler(arguments: dict[str, Any]) -> dict[str, Any]:
        return bridge.call(bridge_tool_name, arguments)

    return McpTool(name, description, input_schema, handler)


def _bridge_client() -> BridgeClient:
    return BridgeClient(os.environ.get(BRIDGE_URL_ENV))


def _load_layer_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "layer_type": {
                "type": "string",
                "enum": LAYER_TYPE_VALUES,
                "description": (
                    "Layer family to load. Use auto only for local files when the "
                    "family is unknown. Remote sources need an explicit type such "
                    "as xyz, wms, wcs or arcgis_feature_server."
                ),
            },
            "source": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Absolute local path, provider URI, XYZ tile URL template with "
                    "{x}, {y} and {z}, or OGC/ArcGIS service URL. Append "
                    "|layername=NAME to pick one layer of a GeoPackage."
                ),
            },
            "name": {
                "type": "string",
                "minLength": 1,
                "description": "Optional layer name. The source filename or service type is used when omitted.",
            },
            "provider": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Optional QGIS provider key or common provider alias such as "
                    "ogr, gdal, xyz, wms, wcs, vector_tile, arcgis_map_server, "
                    "mdal or pdal. Aliases are normalized before loading. A "
                    "provider that names a more specific layer type selects it, "
                    "for example raster with xyz or wms, or vector with "
                    "arcgis_feature_server, and the result reports the "
                    "layer_type used. vector with vector_tile is an error, use "
                    "layer_type vector_tile."
                ),
            },
            "uri_options": {
                "type": "object",
                "description": (
                    "Optional provider URI parts encoded through the QGIS provider "
                    'metadata, for example {"layers": "roads", "format": '
                    '"image/png", "crs": "EPSG:3857"} for WMS or {"identifier": '
                    '"dem"} for WCS. XYZ tiles default to zmin 0 and zmax 19.'
                ),
                "additionalProperties": {
                    "type": ["string", "number", "boolean", "array"],
                    "items": {"type": ["string", "number", "boolean"]},
                },
            },
            "add_to_project": {
                "type": "boolean",
                "description": (
                    "Add the loaded layer to the current QGIS project. False only "
                    "checks that the source opens and keeps no layer."
                ),
                "default": True,
            },
            "refresh_canvas": {
                "type": "boolean",
                "description": "Whether to refresh the current map canvas after loading.",
                "default": False,
            },
        },
        "required": ["layer_type", "source"],
        "additionalProperties": False,
    }


def _remove_layers_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "layer_id": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Single QGIS layer id to remove, from list_layers. "
                    "Preview call only."
                ),
            },
            "layer_ids": {
                "type": "array",
                "items": {"type": "string", "minLength": 1},
                "minItems": 1,
                "uniqueItems": True,
                "description": "Multiple QGIS layer ids to remove. Preview call only.",
            },
            "name": {
                "type": "string",
                "minLength": 1,
                "description": "Exact, case-sensitive layer name to remove.",
            },
            "names": {
                "type": "array",
                "items": {"type": "string", "minLength": 1},
                "minItems": 1,
                "uniqueItems": True,
                "description": "Multiple exact layer names to remove.",
            },
            "source": {
                "type": "string",
                "minLength": 1,
                "description": "Layer source URI or path to remove.",
            },
            "sources": {
                "type": "array",
                "items": {"type": "string", "minLength": 1},
                "minItems": 1,
                "uniqueItems": True,
                "description": "Multiple layer source URIs or paths to remove.",
            },
            "allow_multiple": {
                "type": "boolean",
                "description": (
                    "Allow name or source selectors to remove more than one "
                    "matching layer. Layer id selectors may always remove "
                    "their explicitly listed layers."
                ),
                "default": False,
            },
            "editable_changes": {
                "type": "string",
                "enum": ["reject", "save", "discard"],
                "default": "reject",
                "description": (
                    "Preview-only policy for layers in an edit session. reject stops "
                    "the confirmed removal, save commits changes before removal and "
                    "discard rolls changes back before removal. save and discard "
                    "require at most one target layer to be editable. Use discard "
                    "only when the user explicitly agrees to lose those edits."
                ),
            },
            "confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": (
                    "One-use token returned by the immediately preceding preview. "
                    "Send it alone, without selectors or other options "
                    "(allow_multiple and editable_changes are tolerated only at "
                    "their defaults)."
                ),
            },
        },
        "oneOf": [
            {
                "properties": {"confirmation_token": False},
                "anyOf": [
                    {"required": ["layer_id"]},
                    {"required": ["layer_ids"]},
                    {"required": ["name"]},
                    {"required": ["names"]},
                    {"required": ["source"]},
                    {"required": ["sources"]},
                ],
            },
            {
                "required": ["confirmation_token"],
                "properties": {
                    "layer_id": False,
                    "layer_ids": False,
                    "name": False,
                    "names": False,
                    "source": False,
                    "sources": False,
                    # Default values are tolerated for clients that fill them in.
                    "allow_multiple": {"const": False},
                    "editable_changes": {"const": "reject"},
                },
            },
        ],
        "additionalProperties": False,
    }


def _layer_id_schema(extra_properties: dict[str, Any] | None = None) -> dict[str, Any]:
    properties = {
        "layer_id": {
            "type": "string",
            "minLength": 1,
            "description": "Layer id from list_layers, not the layer name.",
        }
    }
    properties.update(extra_properties or {})
    return {
        "type": "object",
        "properties": properties,
        "required": ["layer_id"],
        "additionalProperties": False,
    }


def _feature_selector_properties() -> dict[str, Any]:
    return {
        "feature_ids": {
            "type": "array",
            "items": {"type": ["integer", "string"]},
            "minItems": 1,
            "maxItems": 10000,
            "uniqueItems": True,
            "description": (
                "Exact QGIS feature ids, such as feature_id values returned by "
                "query_vector_features. With filter_expression, only listed "
                "features that also match the expression are used. Ids that do "
                "not exist are skipped by query_vector_features and are an error "
                "for set_vector_selection and delete_vector_features unless "
                "filter_expression is also given."
            ),
        },
        "filter_expression": {
            "type": "string",
            "minLength": 1,
            "description": (
                "QGIS expression that features must match, for example "
                '"population" > 10000 or "type" = \'river\'. Every field it '
                "references must exist in the layer."
            ),
        },
    }


def _query_vector_features_schema() -> dict[str, Any]:
    properties = {
        "layer_id": {
            "type": "string",
            "minLength": 1,
            "description": "Vector layer id from list_layers, not the layer name.",
        },
        **_feature_selector_properties(),
        "fields": {
            "type": "array",
            "items": {"type": "string", "minLength": 1},
            "minItems": 1,
            "uniqueItems": True,
            "description": (
                "Exact field names to return in attributes. Unknown names are an "
                "error. Omit to return all fields."
            ),
        },
        "include_geometry": {
            "type": "boolean",
            "default": False,
            "description": (
                "Add geometry_wkt in the layer CRS to each feature. Keep limit "
                "small because geometry is capped at 1 MiB per response."
            ),
        },
        "selected_only": {
            "type": "boolean",
            "default": False,
            "description": (
                "Return only features currently selected in QGIS, for example "
                "after set_vector_selection."
            ),
        },
        "offset": {
            "type": "integer",
            "minimum": 0,
            "maximum": MAX_VECTOR_QUERY_OFFSET,
            "default": 0,
            "description": (
                "Number of matching features to skip for paging. Use the previous "
                "offset plus returned_count."
            ),
        },
        "limit": {
            "type": "integer",
            "minimum": 1,
            "maximum": 500,
            "default": 100,
            "description": "Maximum number of features in this page.",
        },
    }
    return {
        "type": "object",
        "properties": properties,
        "required": ["layer_id"],
        "additionalProperties": False,
    }


def _set_vector_selection_schema() -> dict[str, Any]:
    schema = {
        "type": "object",
        "properties": {
            "layer_id": {
                "type": "string",
                "minLength": 1,
                "description": "Vector layer id from list_layers, not the layer name.",
            },
            **_feature_selector_properties(),
            "mode": {
                "type": "string",
                "enum": ["replace", "add", "remove", "clear"],
                "default": "replace",
                "description": (
                    "replace selects only the matches, add and remove change the "
                    "current selection, and clear unselects everything and must be "
                    "sent without feature_ids or filter_expression."
                ),
            },
        },
        "required": ["layer_id"],
        "additionalProperties": False,
    }
    schema["oneOf"] = [
        {
            "anyOf": [
                {"required": ["feature_ids"]},
                {"required": ["filter_expression"]},
            ],
            "properties": {
                "mode": {"enum": ["replace", "add", "remove"]},
            },
        },
        {
            "required": ["mode"],
            "properties": {
                "mode": {"const": "clear"},
                "feature_ids": False,
                "filter_expression": False,
            },
        },
    ]
    return schema


def _delete_vector_features_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "layer_id": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Vector layer id from list_layers. Send the same id in the "
                    "preview and the confirm call."
                ),
            },
            **_feature_selector_properties(),
            "confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": (
                    "One-use token returned by the immediately preceding preview. "
                    "Send it with layer_id only, without feature_ids or "
                    "filter_expression."
                ),
            },
        },
        "required": ["layer_id"],
        "oneOf": [
            {
                "properties": {"confirmation_token": False},
                "anyOf": [
                    {"required": ["feature_ids"]},
                    {"required": ["filter_expression"]},
                ],
            },
            {
                "required": ["confirmation_token"],
                "properties": {
                    "feature_ids": False,
                    "filter_expression": False,
                },
            },
        ],
        "additionalProperties": False,
    }


def _raster_statistics_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "layer_id": {
                "type": "string",
                "minLength": 1,
                "description": "Raster layer id from list_layers, not the layer name.",
            },
            "band": {
                "type": "integer",
                "minimum": 1,
                "default": 1,
                "description": (
                    "1-based band number. It must not exceed band_count from "
                    "get_layer_metadata."
                ),
            },
            "sample_size": {
                "type": "integer",
                "minimum": 1,
                "maximum": 250000,
                "default": 100000,
                "description": (
                    "Maximum number of raster cells sampled. Larger values are "
                    "more precise but slower."
                ),
            },
        },
        "required": ["layer_id"],
        "additionalProperties": False,
    }


def _apply_layer_style_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "layer_id": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Vector or raster layer id from list_layers, not the "
                    "layer name."
                ),
            },
            "style_path": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Absolute path of an existing .qml or .sld file. The suffix "
                    "selects the format."
                ),
            },
        },
        "required": ["layer_id", "style_path"],
        "additionalProperties": False,
    }


def _set_vector_renderer_schema() -> dict[str, Any]:
    color_pattern = "^#[0-9a-fA-F]{6}$"
    return {
        "type": "object",
        "properties": {
            "layer_id": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Vector layer id from list_layers. The layer must have "
                    "geometry."
                ),
            },
            "mode": {
                "type": "string",
                "enum": ["single", "categorized", "graduated"],
                "description": (
                    "single draws every feature with one symbol, categorized "
                    "gives each unique value its own color and graduated "
                    "classifies numeric values into ranges."
                ),
            },
            "field": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Field name or QGIS expression to classify. Required for "
                    "categorized and graduated modes. Graduated mode needs a "
                    "numeric field or an expression that returns numbers."
                ),
            },
            "method": {
                "type": "string",
                "enum": ["equal_interval", "quantile", "jenks", "pretty", "stddev"],
                "default": "equal_interval",
                "description": (
                    "Graduated classification method. Ignored when breaks is given."
                ),
            },
            "classes": {
                "type": "integer",
                "minimum": 2,
                "maximum": 12,
                "default": 5,
                "description": (
                    "Requested graduated class count. quantile, jenks, pretty "
                    "and stddev may return a different count, check "
                    "renderer.class_count in the result."
                ),
            },
            "breaks": {
                "type": "array",
                "items": {"type": "number"},
                "minItems": 3,
                "maxItems": 13,
                "description": (
                    "Explicit strictly ascending graduated class boundaries, "
                    "including the lower bound of the first class and the upper "
                    "bound of the last class, so N+1 numbers make N classes. "
                    "Overrides method and classes. Features outside the first "
                    "and last boundary are not drawn."
                ),
            },
            "color_ramp": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Color ramp name from the QGIS default style, for example "
                    "Greens, Blues, Reds, YlOrRd, Viridis, Magma, Spectral or "
                    "RdYlGn. Defaults to Viridis for graduated and Spectral for "
                    "categorized mode."
                ),
            },
            "invert_ramp": {
                "type": "boolean",
                "default": False,
                "description": (
                    "Reverse the color ramp. Categorized and graduated modes only."
                ),
            },
            "color": {
                "type": "string",
                "pattern": color_pattern,
                "description": "Symbol color for single mode, as #RRGGBB.",
            },
            "outline_color": {
                "type": "string",
                "pattern": color_pattern,
                "description": (
                    "Stroke color for point and polygon symbols, as #RRGGBB. Line "
                    "layers reject it."
                ),
            },
            "opacity": {
                "type": "number",
                "minimum": 0,
                "maximum": 1,
                "description": "Symbol opacity from 0 (transparent) to 1 (opaque).",
            },
        },
        "required": ["layer_id", "mode"],
        "additionalProperties": False,
    }


def _configure_vector_labels_schema() -> dict[str, Any]:
    schema = {
        "type": "object",
        "properties": {
            "layer_id": {
                "type": "string",
                "minLength": 1,
                "description": "Vector layer id from list_layers, not the layer name.",
            },
            "enabled": {
                "type": "boolean",
                "default": True,
                "description": (
                    "True shows labels and needs field_or_expression. False "
                    "switches labels off and must be sent without the other label "
                    "options."
                ),
            },
            "field_or_expression": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Exact field name to label with, or a QGIS expression when "
                    'is_expression is true, for example upper("name") or '
                    'round("area", 1).'
                ),
            },
            "is_expression": {
                "type": "boolean",
                "default": False,
                "description": (
                    "Treat field_or_expression as a QGIS expression instead of a "
                    "field name."
                ),
            },
            "font_size": {
                "type": "number",
                "minimum": 4,
                "maximum": 96,
                "default": 10,
                "description": "Label text size in points.",
            },
            "color": {
                "type": "string",
                "pattern": "^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$",
                "default": "#202020",
                "description": (
                    "Label text color as #RRGGBB, or #RRGGBBAA with the alpha "
                    "last, for example #20202080 for half transparent text. The "
                    "alpha becomes the label text opacity."
                ),
            },
        },
        "required": ["layer_id"],
        "additionalProperties": False,
    }
    schema["oneOf"] = [
        {
            "required": ["field_or_expression"],
            "properties": {"enabled": {"const": True}},
        },
        {
            # Clients that fill in schema defaults may send the default
            # options with enabled=false, so only other values are refused.
            "required": ["enabled"],
            "properties": {
                "enabled": {"const": False},
                "field_or_expression": False,
                "is_expression": {"const": False},
                "font_size": {"const": 10},
                "color": {"const": "#202020"},
            },
        },
    ]
    return schema


def _export_print_layout_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "layout_name": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Exact name of an existing print layout, from "
                    "list_print_layouts or create_print_layout."
                ),
            },
            "path": {
                "type": "string",
                "minLength": 1,
                "description": (
                    "Absolute output file path ending in .pdf, .png, .jpg, .jpeg "
                    "or .svg, matching format. Relative paths are rejected."
                ),
            },
            "format": {
                "type": "string",
                "enum": ["pdf", "png", "jpeg", "svg"],
                "description": (
                    "Output format. It must match the path suffix, jpeg for .jpg "
                    "or .jpeg."
                ),
            },
            "dpi": {
                "type": "number",
                "minimum": 72,
                "maximum": 1200,
                "default": 300,
                "description": (
                    "Resolution in dots per inch, used for every format. The page "
                    "size at this dpi must stay within 25 million pixels."
                ),
            },
            "overwrite": {
                "type": "boolean",
                "default": False,
                "description": (
                    "Allow replacing an existing file. True without a token only "
                    "returns a preview with overwrite_confirmation_token."
                ),
            },
            "overwrite_confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": (
                    "One-use token returned by an overwrite preview. Repeat the "
                    "same call with overwrite=true and this token."
                ),
            },
        },
        "required": ["layout_name", "path", "format"],
        "additionalProperties": False,
    }


def _create_print_layout_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "name": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
                "pattern": "\\S",
                "description": (
                    "Unique non-blank name of the new layout, used later as "
                    "layout_name by export_print_layout."
                ),
            },
            "title": {
                "type": "string",
                "maxLength": 200,
                "description": (
                    "Optional title text placed above the map. Omit for no title."
                ),
            },
            "page_size": {
                "type": "string",
                "enum": ["A4", "A3", "A2", "A1", "A0", "Letter", "Legal"],
                "default": "A4",
                "description": "Paper size of the single page.",
            },
            "orientation": {
                "type": "string",
                "enum": ["portrait", "landscape"],
                "default": "landscape",
                "description": "Page orientation.",
            },
            "map_extent": {
                "type": "array",
                "items": {"type": "number"},
                "minItems": 4,
                "maxItems": 4,
                "description": (
                    "Optional [xmin, ymin, xmax, ymax] in the project CRS. Omit to "
                    "use the current map canvas extent."
                ),
            },
            "include_legend": {
                "type": "boolean",
                "default": True,
                "description": "Add a legend linked to the map, right of the map frame.",
            },
            "include_scale_bar": {
                "type": "boolean",
                "default": True,
                "description": "Add a numeric scale bar such as 1:25000 below the map.",
            },
        },
        "required": ["name"],
        "additionalProperties": False,
    }
