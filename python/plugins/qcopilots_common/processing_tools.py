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

from qcopilots_common.bridge import BridgeClient
from qcopilots_common.constants import BRIDGE_URL_ENV
from qcopilots_common.interactive_layer_tools import build_interactive_layer_tools
from qcopilots_common.mcp_http import McpTool


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
    haystack = " ".join(
        str(value).lower()
        for value in [
            _call_optional(algorithm, "id"),
            _call_optional(algorithm, "displayName"),
            _call_optional(algorithm, "groupId"),
            " ".join(_call_optional(algorithm, "tags") or []),
        ]
        if value
    )
    if any(token in haystack for token in ("raster", "grid", "dem", "cell")):
        return "raster"
    if any(token in haystack for token in ("vector", "feature", "geometry", "attribute")):
        return "vector"
    return None


def filter_processing_algorithms(algorithms: list[Any], category: str) -> list[Any]:
    return [algorithm for algorithm in algorithms if classify_processing_algorithm(algorithm) == category]


def build_interactive_tools() -> list[McpTool]:
    bridge = _bridge_client()
    return [
        _bridge_tool("list_layers", "List QGIS project layers.", {}, bridge),
        _bridge_tool(
            "set_layer_visibility",
            "Set a QGIS layer visibility.",
            {"layer_id": "string", "visible": "boolean"},
            bridge,
            ["layer_id", "visible"],
        ),
        _bridge_tool(
            "zoom_to_layer",
            "Zoom the map canvas to a layer.",
            {"layer_id": "string"},
            bridge,
            ["layer_id"],
        ),
        _bridge_tool(
            "zoom_to_extent",
            "Zoom the map canvas to an extent [xmin, ymin, xmax, ymax].",
            {"extent": "array"},
            bridge,
            ["extent"],
        ),
        _bridge_tool("zoom_in", "Zoom the QGIS map canvas in.", {}, bridge),
        _bridge_tool("zoom_out", "Zoom the QGIS map canvas out.", {}, bridge),
        _bridge_tool("zoom_full", "Zoom the QGIS map canvas to the full project extent.", {}, bridge),
        _bridge_tool(
            "zoom_to_selection",
            "Zoom the QGIS map canvas to selected features.",
            {"layer_id": "string"},
            bridge,
        ),
        _bridge_tool(
            "zoom_to_native_resolution",
            "Zoom a raster layer to its native resolution.",
            {"layer_id": "string"},
            bridge,
        ),
        _bridge_tool("zoom_to_last_extent", "Zoom the QGIS map canvas to the last extent.", {}, bridge),
        _bridge_tool("zoom_to_next_extent", "Zoom the QGIS map canvas to the next extent.", {}, bridge),
        _bridge_tool("save_project", "Save the current QGIS project.", {"path": "string"}, bridge),
        _bridge_tool("refresh_canvas", "Refresh the QGIS map canvas.", {}, bridge),
        _bridge_tool(
            "export_map_image",
            "Export the current map canvas to an image.",
            {"path": "string"},
            bridge,
            ["path"],
        ),
    ] + build_interactive_layer_tools() + [
        _custom_bridge_tool(
            "create_vector_layer",
            "Create a vector layer, optionally write it to a local geospatial file and add it to the project.",
            _create_vector_layer_schema(),
            "create_vector_layer",
            bridge,
        ),
        _custom_bridge_tool(
            "add_vector_features",
            "Add vector features to an existing vector layer.",
            _add_vector_features_schema(),
            "add_vector_features",
            bridge,
        ),
        _custom_bridge_tool(
            "update_vector_features",
            "Update vector feature attributes or geometry in an existing vector layer.",
            _update_vector_features_schema(),
            "update_vector_features",
            bridge,
        ),
    ]


def build_processing_tools(category: str) -> list[McpTool]:
    bridge = _bridge_client()

    def list_algorithms(arguments: dict[str, Any]) -> dict[str, Any]:
        return bridge.call("processing_list_algorithms", {"category": category})

    def algorithm_details(arguments: dict[str, Any]) -> dict[str, Any]:
        payload = dict(arguments)
        payload["category"] = category
        return bridge.call("processing_algorithm_details", payload)

    def run_algorithm(arguments: dict[str, Any]) -> dict[str, Any]:
        payload = dict(arguments)
        payload["category"] = category
        return bridge.call("processing_run_algorithm", payload)

    return [
        McpTool(
            f"list_{category}_processing_algorithms",
            f"List QGIS Processing algorithms for {category} data.",
            {"type": "object", "properties": {}, "additionalProperties": False},
            list_algorithms,
        ),
        McpTool(
            f"get_{category}_processing_algorithm_details",
            f"Get QGIS Processing algorithm details for {category} data.",
            _schema({"algorithm_id": "string"}, ["algorithm_id"]),
            algorithm_details,
        ),
        McpTool(
            f"run_{category}_processing_algorithm",
            f"Run a QGIS Processing algorithm for {category} data.",
            _schema({"algorithm_id": "string", "parameters": "object"}, ["algorithm_id"]),
            run_algorithm,
        ),
    ]


def _bridge_tool(
    name: str,
    description: str,
    properties: dict[str, str],
    bridge: BridgeClient,
    required: list[str] | None = None,
) -> McpTool:
    def handler(arguments: dict[str, Any]) -> dict[str, Any]:
        return bridge.call(name, arguments)

    return McpTool(name, description, _schema(properties, required or []), handler)


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


def _bridge_client() -> BridgeClient:
    return BridgeClient(os.environ.get(BRIDGE_URL_ENV))


def _tool_name(algorithm_id: str) -> str:
    return "processing_" + re.sub(r"[^a-zA-Z0-9]+", "_", algorithm_id).strip("_").lower()


def _call_optional(target: Any, name: str) -> Any:
    value = getattr(target, name, None)
    if callable(value):
        return value()
    return value


def _schema(properties: dict[str, str], required: list[str]) -> dict[str, Any]:
    converted = {}
    for name, schema_type in properties.items():
        if schema_type == "array":
            converted[name] = {"type": "array", "items": {"type": "number"}}
        elif schema_type == "object":
            converted[name] = {"type": "object"}
        else:
            converted[name] = {"type": schema_type}
    return {
        "type": "object",
        "properties": converted,
        "required": required,
        "additionalProperties": False,
    }


def _create_vector_layer_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "name": {"type": "string", "description": "Layer name."},
            "geometry_type": {
                "type": "string",
                "enum": ["Point", "LineString", "Polygon", "MultiPoint", "MultiLineString", "MultiPolygon", "NoGeometry"],
                "default": "Point",
            },
            "crs": {"type": "string", "default": "EPSG:4326"},
            "fields": {
                "type": "array",
                "items": _field_schema(),
                "default": [],
            },
            "features": {
                "type": "array",
                "items": _feature_schema(),
                "default": [],
            },
            "path": {
                "type": "string",
                "description": "Optional output path. Suffix chooses GPKG, Shapefile or GeoJSON when possible.",
            },
            "driver_name": {"type": "string", "description": "Optional OGR driver name override."},
            "add_to_project": {"type": "boolean", "default": True},
        },
        "required": ["name"],
        "additionalProperties": False,
    }


def _add_vector_features_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "layer_id": {"type": "string"},
            "features": {
                "type": "array",
                "items": _feature_schema(),
                "minItems": 1,
            },
        },
        "required": ["layer_id", "features"],
        "additionalProperties": False,
    }


def _update_vector_features_schema() -> dict[str, Any]:
    update_schema = _feature_schema()
    update_schema["properties"]["feature_id"] = {"type": ["integer", "string"]}
    update_schema["required"] = ["feature_id"]
    update_schema["anyOf"] = [
        {"required": ["attributes"], "properties": {"attributes": {"type": "object", "minProperties": 1}}},
        {"required": ["geometry_wkt"], "properties": {"geometry_wkt": {"type": "string", "minLength": 1}}},
    ]
    return {
        "type": "object",
        "properties": {
            "layer_id": {"type": "string"},
            "updates": {
                "type": "array",
                "items": update_schema,
                "minItems": 1,
            },
        },
        "required": ["layer_id", "updates"],
        "additionalProperties": False,
    }


def _field_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "type": {
                "type": "string",
                "enum": ["string", "int", "integer", "long", "double", "float", "bool", "boolean", "date", "datetime"],
                "default": "string",
            },
        },
        "required": ["name"],
        "additionalProperties": False,
    }


def _feature_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "geometry_wkt": {"type": "string"},
            "attributes": {
                "type": "object",
                "additionalProperties": True,
                "default": {},
            },
        },
        "additionalProperties": False,
    }
