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
            (
                "Discover the explicitly configured QGIS package binaries without "
                "starting a process. Returns a filtered, cursor-paginated page of "
                "binary summaries, including availability, risk, environment, and "
                "disabled reasons, so a returned binary_id can be used with the "
                "details or start tools."
            ),
            _list_binaries_schema(),
            list_binaries,
        ),
        McpTool(
            "get_qgis_binary_details",
            (
                "Inspect one explicitly configured QGIS package binary without "
                "running it. Given a binary_id from list_qgis_binaries, returns its "
                "effective catalog policy, executable path, availability, risk, "
                "environment, probe, exit-code rules, and resource limits."
            ),
            _binary_id_schema(),
            binary_details,
        ),
        McpTool(
            "start_qgis_binary",
            (
                "Start one enabled, explicitly configured QGIS package binary as an "
                "asynchronous QgsTask job. The service passes argv without a command "
                "shell, applies the catalog environment and resource limits, requires "
                "explicit confirmation for R2 binaries, blocks R3 binaries, and "
                "returns a job snapshot for later polling or cancellation."
            ),
            _start_binary_schema(),
            start_binary,
        ),
        McpTool(
            "get_qgis_binary_job",
            (
                "Read the current complete snapshot of one QGIS binary job without "
                "waiting for or changing it. Returns lifecycle state, progress, "
                "timestamps, binary metadata, and a terminal result or structured "
                "error, including bounded stdout and stderr tails when available."
            ),
            _job_id_schema(),
            get_job,
        ),
        McpTool(
            "list_qgis_binary_jobs",
            (
                "List recent QGIS binary job summaries without changing or waiting "
                "for jobs. Optionally filters by lifecycle state and limits the result "
                "count. Use get_qgis_binary_job with a returned job_id for complete "
                "progress, output, result, or error details."
            ),
            _list_jobs_schema(),
            list_jobs,
        ),
        McpTool(
            "cancel_qgis_binary_job",
            (
                "Request cancellation of a queued or running QGIS binary job and its "
                "spawned process tree. Returns the current job snapshot after the "
                "request. Cancellation may complete asynchronously, so poll "
                "get_qgis_binary_job to confirm the terminal state, while already "
                "terminal jobs remain recorded."
            ),
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
                "description": (
                    "Pagination cursor returned by the previous "
                    "list_qgis_binaries page. Omit it for the first page."
                ),
            },
            "limit": {
                "type": "integer",
                "minimum": 1,
                "maximum": 200,
                "default": 50,
                "description": (
                    "Maximum number of binary summaries to return on this page."
                ),
            },
            "query": {
                "type": "string",
                "maxLength": 256,
                "description": (
                    "Case-insensitive substring matched against binary ID, name, "
                    "description, package-relative path, and group."
                ),
            },
            "group": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
                "description": "Exact catalog group ID to include in the result.",
            },
            "enabled": {
                "type": "boolean",
                "description": (
                    "When supplied, include only enabled binaries or only disabled "
                    "binaries."
                ),
            },
            "risk": {
                "type": "string",
                "enum": ["R1", "R2", "R3"],
                "description": "Exact catalog risk level to include in the result.",
            },
        },
        "additionalProperties": False,
    }


def _binary_id_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "binary_id": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
                "description": (
                    "Stable binary ID returned by list_qgis_binaries for the "
                    "configured package executable to inspect."
                ),
            },
        },
        "required": ["binary_id"],
        "additionalProperties": False,
    }


def _start_binary_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "binary_id": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
                "description": (
                    "Stable binary ID returned by list_qgis_binaries. The selected "
                    "binary must be enabled and permitted by its risk policy."
                ),
            },
            "arguments": {
                "type": "array",
                "items": {"type": "string", "maxLength": 32768},
                "maxItems": 256,
                "default": [],
                "description": (
                    "Exact argv entries passed to the configured executable without "
                    "a command shell or argument rewriting."
                ),
            },
            "working_directory": {
                "type": "string",
                "minLength": 1,
                "maxLength": 32768,
                "description": (
                    "Existing process working directory. Relative paths resolve from "
                    "the QGIS package root; absolute paths are used as supplied."
                ),
            },
            "stdin": {
                "type": "string",
                "maxLength": MAX_STDIN_CHARS,
                "description": (
                    "UTF-8 text written to the process standard input. The selected "
                    "binary may prohibit stdin or impose a smaller byte limit."
                ),
            },
            "timeout_seconds": {
                "type": "number",
                "exclusiveMinimum": 0,
                "maximum": MAX_TIMEOUT_SECONDS,
                "description": (
                    "Requested runtime timeout in seconds. It cannot exceed the "
                    "selected binary's configured timeout; omission uses that limit."
                ),
            },
            "client_request_id": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
                "description": (
                    "Optional idempotency key. Repeating an identical start request "
                    "returns its existing job instead of creating another one."
                ),
            },
            "confirmed_risk": {
                "type": "boolean",
                "default": False,
                "description": (
                    "Explicit per-request risk confirmation. It must be true for an "
                    "R2 binary and does not allow an R3 binary to run."
                ),
            },
        },
        "required": ["binary_id"],
        "additionalProperties": False,
    }


def _job_id_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "job_id": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
                "description": (
                    "QGIS binary job ID returned by start_qgis_binary or "
                    "list_qgis_binary_jobs."
                ),
            },
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
                "description": (
                    "Optional non-empty set of lifecycle states to include. Omit it "
                    "to include jobs in every public state."
                ),
            },
            "limit": {
                "type": "integer",
                "minimum": 1,
                "maximum": 200,
                "description": "Maximum number of recent job summaries to return.",
            },
        },
        "additionalProperties": False,
    }
