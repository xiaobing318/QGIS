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
            "Remove QGIS project layers by id, name or source URI.",
            _remove_layers_schema(),
            "interactive_layer_remove_layers",
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
                "description": "Local path, provider URI, tile URL template or OGC/ArcGIS service URL.",
            },
            "name": {
                "type": "string",
                "description": "Optional layer name. The source filename or service type is used when omitted.",
            },
            "provider": {
                "type": "string",
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
            "layer_id": {"type": "string", "description": "Single QGIS layer id to remove."},
            "layer_ids": {
                "type": "array",
                "items": {"type": "string"},
                "description": "Multiple QGIS layer ids to remove.",
            },
            "name": {"type": "string", "description": "Single layer name to remove."},
            "names": {
                "type": "array",
                "items": {"type": "string"},
                "description": "Multiple layer names to remove.",
            },
            "source": {"type": "string", "description": "Layer source URI or path to remove."},
            "sources": {
                "type": "array",
                "items": {"type": "string"},
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
        },
        "additionalProperties": False,
    }
