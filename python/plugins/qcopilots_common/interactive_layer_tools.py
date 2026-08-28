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
                "Describe layer source types supported by QCopilots interactive tools, "
                "including file, tile, OGC service, ArcGIS service and 3D data sources."
            ),
            {"type": "object", "properties": {}, "additionalProperties": False},
            "interactive_layer_describe_sources",
            bridge,
        ),
        _bridge_tool(
            "list_map_layers",
            "List layers currently loaded in the QGIS project.",
            {"type": "object", "properties": {}, "additionalProperties": False},
            "interactive_layer_list_layers",
            bridge,
        ),
        _bridge_tool(
            "load_map_layer",
            (
                "Load one QGIS map layer from a file path, provider URI, tile URL "
                "or service URL, then optionally add it to the current project."
            ),
            _load_layer_schema(),
            "interactive_layer_load_layer",
            bridge,
        ),
        _bridge_tool(
            "remove_map_layers",
            (
                "Preview removal of QGIS project layers by id, name or source URI, "
                "choose how editable changes are handled, then confirm the exact "
                "version once with the returned token. Editable changes are rejected "
                "by default and are never silently discarded."
            ),
            _remove_layers_schema(),
            "interactive_layer_remove_layers",
            bridge,
        ),
        _bridge_tool(
            "get_layer_metadata",
            "Get CRS, extent, schema and type-specific metadata for one QGIS layer.",
            _layer_id_schema({"include_source": {"type": "boolean", "default": False}}),
            "interactive_layer_get_metadata",
            bridge,
        ),
        _bridge_tool(
            "query_vector_features",
            "Query a bounded page of vector feature attributes and optional geometry.",
            _query_vector_features_schema(),
            "interactive_layer_query_features",
            bridge,
        ),
        _bridge_tool(
            "set_vector_selection",
            "Replace, add, remove or clear the selected features of a vector layer.",
            _set_vector_selection_schema(),
            "interactive_layer_set_selection",
            bridge,
        ),
        _bridge_tool(
            "delete_vector_features",
            (
                "Preview an exact vector feature deletion, then stage it once with "
                "the returned short-lived confirmation_token. A successful confirmed "
                "deletion reports staged=true, committed=false and "
                "requires_user_commit=true."
            ),
            _delete_vector_features_schema(),
            "interactive_layer_delete_features",
            bridge,
        ),
        _bridge_tool(
            "get_project_crs",
            "Get the current QGIS project CRS.",
            {"type": "object", "properties": {}, "additionalProperties": False},
            "interactive_project_get_crs",
            bridge,
        ),
        _bridge_tool(
            "set_project_crs",
            "Set the current QGIS project CRS from an authority id or WKT.",
            {
                "type": "object",
                "properties": {"crs": {"type": "string", "minLength": 1}},
                "required": ["crs"],
                "additionalProperties": False,
            },
            "interactive_project_set_crs",
            bridge,
        ),
        _bridge_tool(
            "get_raster_statistics",
            "Calculate bounded QGIS raster band statistics.",
            _raster_statistics_schema(),
            "interactive_raster_statistics",
            bridge,
        ),
        _bridge_tool(
            "apply_layer_style",
            "Apply an existing local QML or SLD style file to a QGIS layer.",
            _apply_layer_style_schema(),
            "interactive_layer_apply_style",
            bridge,
        ),
        _bridge_tool(
            "configure_vector_labels",
            "Enable or disable simple field or expression labels for a vector layer.",
            _configure_vector_labels_schema(),
            "interactive_layer_configure_labels",
            bridge,
        ),
        _bridge_tool(
            "create_print_layout",
            (
                "Create a constrained one-page print layout with a title, map "
                "frame, optional legend and optional numeric scale bar."
            ),
            _create_print_layout_schema(),
            "interactive_layout_create",
            bridge,
        ),
        _bridge_tool(
            "list_print_layouts",
            "List print layouts in the current QGIS project.",
            {"type": "object", "properties": {}, "additionalProperties": False},
            "interactive_layout_list",
            bridge,
        ),
        _bridge_tool(
            "export_print_layout",
            "Export one print layout with explicit no-overwrite behavior.",
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
                "description": "Layer family to load. Use auto for local files when the exact family is unknown.",
            },
            "source": {
                "type": "string",
                "minLength": 1,
                "description": "Local path, provider URI, tile URL template or OGC/ArcGIS service URL.",
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
                    "mdal or pdal. Aliases are normalized before loading."
                ),
            },
            "uri_options": {
                "type": "object",
                "description": "Optional provider URI parts passed through QGIS provider metadata when available.",
                "additionalProperties": {
                    "type": ["string", "number", "boolean", "array"],
                    "items": {"type": ["string", "number", "boolean"]},
                },
            },
            "add_to_project": {
                "type": "boolean",
                "description": "Whether to add the loaded layer to the current QGIS project.",
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
                "description": "Single QGIS layer id to remove.",
            },
            "layer_ids": {
                "type": "array",
                "items": {"type": "string", "minLength": 1},
                "minItems": 1,
                "uniqueItems": True,
                "description": "Multiple QGIS layer ids to remove.",
            },
            "name": {
                "type": "string",
                "minLength": 1,
                "description": "Single layer name to remove.",
            },
            "names": {
                "type": "array",
                "items": {"type": "string", "minLength": 1},
                "minItems": 1,
                "uniqueItems": True,
                "description": "Multiple layer names to remove.",
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
                    "require at most one target layer to be editable."
                ),
            },
            "confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": "One-use token returned by the immediately preceding preview.",
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
                    "allow_multiple": False,
                    "editable_changes": False,
                },
            },
        ],
        "additionalProperties": False,
    }


def _layer_id_schema(extra_properties: dict[str, Any] | None = None) -> dict[str, Any]:
    properties = {"layer_id": {"type": "string", "minLength": 1}}
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
        },
        "filter_expression": {"type": "string", "minLength": 1},
    }


def _query_vector_features_schema() -> dict[str, Any]:
    properties = {
        "layer_id": {"type": "string", "minLength": 1},
        **_feature_selector_properties(),
        "fields": {
            "type": "array",
            "items": {"type": "string", "minLength": 1},
            "minItems": 1,
            "uniqueItems": True,
        },
        "include_geometry": {"type": "boolean", "default": False},
        "selected_only": {"type": "boolean", "default": False},
        "offset": {
            "type": "integer",
            "minimum": 0,
            "maximum": MAX_VECTOR_QUERY_OFFSET,
            "default": 0,
        },
        "limit": {"type": "integer", "minimum": 1, "maximum": 500, "default": 100},
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
            "layer_id": {"type": "string", "minLength": 1},
            **_feature_selector_properties(),
            "mode": {
                "type": "string",
                "enum": ["replace", "add", "remove", "clear"],
                "default": "replace",
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
            "layer_id": {"type": "string", "minLength": 1},
            **_feature_selector_properties(),
            "confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": "One-use token returned by the immediately preceding preview.",
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
            "layer_id": {"type": "string", "minLength": 1},
            "band": {"type": "integer", "minimum": 1, "default": 1},
            "sample_size": {
                "type": "integer",
                "minimum": 1,
                "maximum": 250000,
                "default": 100000,
            },
        },
        "required": ["layer_id"],
        "additionalProperties": False,
    }


def _apply_layer_style_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "layer_id": {"type": "string", "minLength": 1},
            "style_path": {"type": "string", "minLength": 1},
        },
        "required": ["layer_id", "style_path"],
        "additionalProperties": False,
    }


def _configure_vector_labels_schema() -> dict[str, Any]:
    schema = {
        "type": "object",
        "properties": {
            "layer_id": {"type": "string", "minLength": 1},
            "enabled": {"type": "boolean", "default": True},
            "field_or_expression": {"type": "string", "minLength": 1},
            "is_expression": {"type": "boolean", "default": False},
            "font_size": {"type": "number", "minimum": 4, "maximum": 96, "default": 10},
            "color": {
                "type": "string",
                "pattern": "^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$",
                "default": "#202020",
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
            "required": ["enabled"],
            "properties": {
                "enabled": {"const": False},
                "field_or_expression": False,
                "is_expression": False,
                "font_size": False,
                "color": False,
            },
        },
    ]
    return schema


def _export_print_layout_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "layout_name": {"type": "string", "minLength": 1},
            "path": {"type": "string", "minLength": 1},
            "format": {"type": "string", "enum": ["pdf", "png", "jpeg", "jpg", "svg"]},
            "dpi": {"type": "number", "minimum": 72, "maximum": 1200, "default": 300},
            "overwrite": {"type": "boolean", "default": False},
            "overwrite_confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": "One-use token returned by an overwrite preview.",
            },
        },
        "required": ["layout_name", "path"],
        "additionalProperties": False,
    }


def _create_print_layout_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "name": {"type": "string", "minLength": 1, "maxLength": 128},
            "title": {"type": "string", "maxLength": 200},
            "page_size": {
                "type": "string",
                "enum": ["A4", "A3", "A2", "A1", "A0", "Letter", "Legal"],
                "default": "A4",
            },
            "orientation": {
                "type": "string",
                "enum": ["portrait", "landscape"],
                "default": "landscape",
            },
            "map_extent": {
                "type": "array",
                "items": {"type": "number"},
                "minItems": 4,
                "maxItems": 4,
            },
            "include_legend": {"type": "boolean", "default": True},
            "include_scale_bar": {"type": "boolean", "default": True},
        },
        "required": ["name"],
        "additionalProperties": False,
    }
