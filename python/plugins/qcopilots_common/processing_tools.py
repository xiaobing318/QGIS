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
from qcopilots_common.constants import (
    BRIDGE_URL_ENV,
    DEFAULT_PROCESSING_ALGORITHM_RESULTS,
    MAX_PROCESSING_ALGORITHM_CURSOR_LENGTH,
    MAX_PROCESSING_ALGORITHM_RESULTS,
    PROCESSING_ALGORITHM_CURSOR_ERROR_PREFIX,
)
from qcopilots_common.interactive_layer_tools import build_interactive_layer_tools
from qcopilots_common.mcp_http import McpTool, ToolError
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
        _custom_bridge_tool(
            "zoom_to_extent",
            "Zoom the map canvas to an extent [xmin, ymin, xmax, ymax].",
            {
                "type": "object",
                "properties": {
                    "extent": {
                        "type": "array",
                        "items": {"type": "number"},
                        "minItems": 4,
                        "maxItems": 4,
                    }
                },
                "required": ["extent"],
                "additionalProperties": False,
            },
            "zoom_to_extent",
            bridge,
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
        _custom_bridge_tool(
            "save_project",
            "Save the current QGIS project with explicit no-overwrite behavior for a new path.",
            _save_project_schema(),
            "save_project",
            bridge,
        ),
        _bridge_tool("refresh_canvas", "Refresh the QGIS map canvas.", {}, bridge),
        _custom_bridge_tool(
            "export_map_image",
            "Export the current map canvas to an image with explicit no-overwrite behavior.",
            _export_map_image_schema(),
            "export_map_image",
            bridge,
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
        try:
            return bridge.call("processing_list_algorithms", payload)
        except RuntimeError as err:
            if str(err).startswith(PROCESSING_ALGORITHM_CURSOR_ERROR_PREFIX):
                raise ToolError(str(err)) from err
            raise

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

    return [
        McpTool(
            f"list_{category}_processing_algorithms",
            (
                f"List QGIS Processing algorithms for {category} data. "
                "General algorithms include stable audited safety classifications."
                if category == "general"
                else f"List QGIS Processing algorithms for {category} data."
            ),
            _list_processing_algorithms_schema(),
            list_algorithms,
        ),
        McpTool(
            f"get_{category}_processing_algorithm_details",
            (
                f"Inspect the parameters, outputs and availability of one {category} "
                "QGIS Processing algorithm without starting a job."
            ),
            _processing_algorithm_id_schema(),
            algorithm_details,
        ),
        McpTool(
            f"start_{category}_processing_algorithm",
            (
                f"Start an asynchronous QGIS Processing job for {category} data and "
                "return its initial snapshot for polling or cancellation. Successful "
                "outputs are added to the current project by default. Existing output "
                "destinations are never overwritten without an explicit overwrite "
                "request and the exact one-use confirmation token returned by its "
                "preview."
            ),
            _start_processing_algorithm_schema(),
            start_algorithm,
        ),
        McpTool(
            f"get_{category}_processing_job",
            f"Get the complete snapshot for a {category} Processing job.",
            _processing_job_schema(),
            get_job,
        ),
        McpTool(
            f"list_{category}_processing_jobs",
            f"List recent {category} Processing job summaries.",
            _list_processing_jobs_schema(),
            list_jobs,
        ),
        McpTool(
            f"cancel_{category}_processing_job",
            f"Request cancellation of a {category} Processing job.",
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
            if schema_type == "string":
                converted[name]["minLength"] = 1
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
            "algorithm_id": {"type": "string", "minLength": 1},
            "parameters": {
                "type": "object",
                "default": {},
                "description": "Algorithm parameter values keyed by parameter ID.",
            },
            "add_outputs_to_project": {
                "type": "boolean",
                "default": True,
                "description": (
                    "Add compatible successful outputs to the current QGIS project "
                    "during job post-processing."
                ),
            },
            "overwrite_outputs": {
                "type": "boolean",
                "default": False,
                "description": "Explicitly allow existing file or folder destinations to be overwritten.",
            },
            "overwrite_confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": (
                    "One-use token from an overwrite preview, bound to the "
                    "algorithm, parameters, input versions and output targets."
                ),
            },
            "client_request_id": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
            },
        },
        "required": ["algorithm_id"],
        "additionalProperties": False,
    }


def _processing_job_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "job_id": {"type": "string", "minLength": 1, "maxLength": 128},
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
            },
            "limit": {"type": "integer", "minimum": 1, "maximum": 200},
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
            },
            "crs": {"type": "string", "minLength": 1, "default": "EPSG:4326"},
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
                "minLength": 1,
                "description": "Optional output path. Suffix chooses GPKG, Shapefile or GeoJSON when possible.",
            },
            "driver_name": {
                "type": "string",
                "minLength": 1,
                "description": "Optional OGR driver name override.",
            },
            "add_to_project": {"type": "boolean", "default": True},
            "overwrite": {
                "type": "boolean",
                "default": False,
                "description": "Explicitly allow replacement of an existing output dataset.",
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
            "layer_id": {"type": "string", "minLength": 1},
            "features": {
                "type": "array",
                "items": _feature_schema(),
                "minItems": 1,
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
                    "Opaque continuation cursor returned by the previous page. "
                    "Do not send max_results with cursor."
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
                "properties": {"max_results": False},
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
                    "Optional .qgz project destination. Plain .qgs projects are "
                    "unsupported because their auxiliary file family cannot be "
                    "published atomically as one output."
                ),
            },
            "overwrite": {"type": "boolean", "default": False},
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
            "path": {"type": "string", "minLength": 1},
            "overwrite": {"type": "boolean", "default": False},
            "overwrite_confirmation_token": {
                "type": "string",
                "minLength": 16,
                "description": "One-use token returned by an overwrite preview.",
            },
        },
        "required": ["path"],
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
            "layer_id": {"type": "string", "minLength": 1},
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
            "name": {"type": "string", "minLength": 1},
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
            "geometry_wkt": {"type": "string", "minLength": 1},
            "attributes": {
                "type": "object",
                "additionalProperties": True,
                "default": {},
            },
        },
        "additionalProperties": False,
    }
