"""MCP tools for configured QGIS package command line programs.

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


PUBLIC_JOB_STATES = (
    "queued",
    "running",
    "cancelling",
    "succeeded",
    "failed",
    "cancelled",
)
MAX_STDIN_CHARS = 1024 * 1024
MAX_TIMEOUT_SECONDS = 24 * 60 * 60


def build_qgis_binary_tools() -> list[McpTool]:
    bridge = BridgeClient(os.environ.get(BRIDGE_URL_ENV))

    def list_binaries(arguments: dict[str, Any]) -> dict[str, Any]:
        return bridge.call("qgis_binary_list_binaries", dict(arguments))

    def binary_details(arguments: dict[str, Any]) -> dict[str, Any]:
        return bridge.call("qgis_binary_get_binary_details", dict(arguments))

    def start_binary(arguments: dict[str, Any]) -> dict[str, Any]:
        payload = dict(arguments)
        payload.setdefault("arguments", [])
        payload.setdefault("confirmed_risk", False)
        return bridge.call("qgis_binary_start", payload)

    def get_job(arguments: dict[str, Any]) -> dict[str, Any]:
        return bridge.call("qgis_binary_get_job", dict(arguments))

    def list_jobs(arguments: dict[str, Any]) -> dict[str, Any]:
        return bridge.call("qgis_binary_list_jobs", dict(arguments))

    def cancel_job(arguments: dict[str, Any]) -> dict[str, Any]:
        return bridge.call("qgis_binary_cancel_job", dict(arguments))

    return [
        McpTool(
            "list_qgis_binaries",
            "List the explicitly configured QGIS package binaries with catalog pagination and filters.",
            _list_binaries_schema(),
            list_binaries,
        ),
        McpTool(
            "get_qgis_binary_details",
            "Get configuration and availability details for one configured QGIS package binary.",
            _binary_id_schema(),
            binary_details,
        ),
        McpTool(
            "start_qgis_binary",
            "Start an enabled configured QGIS package binary as an asynchronous QgsTask job.",
            _start_binary_schema(),
            start_binary,
        ),
        McpTool(
            "get_qgis_binary_job",
            "Get the complete snapshot for a QGIS binary job.",
            _job_id_schema(),
            get_job,
        ),
        McpTool(
            "list_qgis_binary_jobs",
            "List recent QGIS binary job summaries.",
            _list_jobs_schema(),
            list_jobs,
        ),
        McpTool(
            "cancel_qgis_binary_job",
            "Request cancellation of a QGIS binary job and its process tree.",
            _job_id_schema(),
            cancel_job,
        ),
    ]


def _list_binaries_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "cursor": {
                "type": "string",
                "minLength": 1,
                "maxLength": 2048,
                "description": "Opaque cursor returned by the previous catalog page.",
            },
            "limit": {
                "type": "integer",
                "minimum": 1,
                "maximum": 200,
                "default": 50,
            },
            "query": {"type": "string", "maxLength": 256},
            "group": {"type": "string", "minLength": 1, "maxLength": 128},
            "enabled": {"type": "boolean"},
            "risk": {
                "type": "string",
                "enum": ["R1", "R2", "R3"],
            },
        },
        "additionalProperties": False,
    }


def _binary_id_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "binary_id": {"type": "string", "minLength": 1, "maxLength": 128},
        },
        "required": ["binary_id"],
        "additionalProperties": False,
    }


def _start_binary_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "binary_id": {"type": "string", "minLength": 1, "maxLength": 128},
            "arguments": {
                "type": "array",
                "items": {"type": "string", "maxLength": 32768},
                "maxItems": 256,
                "default": [],
                "description": "Exact argv entries passed without a command shell.",
            },
            "working_directory": {
                "type": "string",
                "minLength": 1,
                "maxLength": 32768,
            },
            "stdin": {"type": "string", "maxLength": MAX_STDIN_CHARS},
            "timeout_seconds": {
                "type": "number",
                "exclusiveMinimum": 0,
                "maximum": MAX_TIMEOUT_SECONDS,
            },
            "client_request_id": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
            },
            "confirmed_risk": {
                "type": "boolean",
                "default": False,
                "description": "Must be true for configured R2 binaries.",
            },
        },
        "required": ["binary_id"],
        "additionalProperties": False,
    }


def _job_id_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "job_id": {"type": "string", "minLength": 1, "maxLength": 128},
        },
        "required": ["job_id"],
        "additionalProperties": False,
    }


def _list_jobs_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "states": {
                "type": "array",
                "items": {"type": "string", "enum": list(PUBLIC_JOB_STATES)},
                "minItems": 1,
                "uniqueItems": True,
            },
            "limit": {"type": "integer", "minimum": 1, "maximum": 200},
        },
        "additionalProperties": False,
    }
