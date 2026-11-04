"""MCP tools for configured QGIS package command line programs.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import os
from typing import Any

from qcopilots_common.async_jobs import PUBLIC_JOB_STATE_ORDER
from qcopilots_common.bridge import BridgeClient
from qcopilots_common.constants import (
    BRIDGE_URL_ENV,
    QGIS_BINARY_DEFAULT_LIST_LIMIT,
    QGIS_BINARY_MAX_LIST_LIMIT,
)
from qcopilots_common.mcp_http import McpTool


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
                "Discover the command line programs shipped in the current QGIS "
                "package, such as gdalinfo, ogrinfo, ogr2ogr, gdalwarp, projinfo "
                "and qgis_process, without starting a process. This is step one "
                "of the binary workflow: list_qgis_binaries, "
                "get_qgis_binary_details, start_qgis_binary, then poll "
                "get_qgis_binary_job. Returns a filtered, cursor-paginated page "
                "(binaries, total, next_cursor) of binary summaries, including "
                "catalog enablement, configured risk, environment, "
                "package-relative path and disabled reasons, so a returned "
                "binary_id can be used with the details or start tools. Search "
                "with query, for example ogr2ogr, and add enabled=true to see "
                "only runnable entries. Pass next_cursor as cursor until it is "
                "null. Package changes are discovered on each lookup. Unreviewed "
                "binaries are listed but cannot run, and missing configured "
                "binaries remain listed as unavailable. Availability and "
                "policy_status distinguish package presence from reviewed "
                "execution policy. Listing the catalog does not execute a "
                "configured probe or binary."
            ),
            _list_binaries_schema(),
            list_binaries,
        ),
        McpTool(
            "get_qgis_binary_details",
            (
                "Inspect one discovered or configured QGIS package binary without "
                "running it. Given a binary_id from list_qgis_binaries, returns its "
                "effective catalog policy, package-relative executable path, catalog "
                "enablement, risk, environment, configured probe, exit-code rules, "
                "and resource limits when reviewed. Check it before "
                "start_qgis_binary. Risk R1 runs directly, R2 needs "
                "confirmed_risk=true and R3 never runs. timeout_seconds is the "
                "largest timeout a start accepts, max_stdin_bytes bounds stdin, "
                "success_exit_codes decide whether a job succeeds, and a "
                "progress_parser means progress_percent is reported while "
                "running. It does not return the program's command line help. "
                "Newly discovered binaries have no configured probe or "
                "environment and cannot run. This inspection does not execute "
                "the probe or binary."
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
                "returns a job snapshot (job_id, state queued) for later polling or "
                "cancellation. Use it when a task needs a specific packaged program "
                "or option, for example gdalinfo -json or ogr2ogr with -sql. Prefer "
                "start_raster_processing_algorithm and the other Processing start "
                "tools when an algorithm covers the task, because they add outputs "
                "to the project. Use exec_shell_command only for programs outside "
                "this catalog, since it runs synchronously without the catalog "
                "environment. R1 binaries run directly. For R2, show the user the "
                "command and set confirmed_risk=true only after approval. For "
                "deterministic file outputs, pass absolute input and "
                "output paths in argv, select an existing absolute working directory, "
                "and create output parent directories before starting the job. "
                "The service does not protect existing outputs, so the program's "
                "flags such as -overwrite decide. Formal "
                "restricted mode still rejects unsafe path forms, unapproved network "
                "sources, and rejects non-empty stdin. Poll get_qgis_binary_job "
                "until the state is terminal, then use load_map_layer to show "
                "outputs."
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
                "error, including bounded stdout and stderr tails when available. "
                "Call it repeatedly with the job_id from start_qgis_binary until "
                "the state is succeeded, failed or cancelled. A succeeded job has "
                "result.exit_code, result.stdout, result.stderr, truncation flags "
                "and duration_seconds. An exit code outside the catalog "
                "success_exit_codes makes the job failed with error.code "
                "process_exit_failed. Other codes include process_timed_out and "
                "process_start_failed, and error.details carries any captured "
                "output. progress_percent stays null unless the binary has a "
                "progress parser."
            ),
            _job_id_schema(),
            get_job,
        ),
        McpTool(
            "list_qgis_binary_jobs",
            (
                "List recent QGIS binary job summaries, newest first, without "
                "changing or waiting for jobs. Optionally filters by lifecycle "
                "state and limits the result count, which defaults to "
                f"{QGIS_BINARY_DEFAULT_LIST_LIMIT}. "
                "Summaries include job_id, state, binary, progress, timestamps and "
                "the error code and message, but no stdout, stderr or result. Use "
                "it to recover a lost job_id or to see what is still running. "
                "Finished jobs are kept in memory by the running QGIS session for "
                "up to 24 hours. Use get_qgis_binary_job with a returned job_id "
                "for complete progress, output, result, or error details."
            ),
            _list_jobs_schema(),
            list_jobs,
        ),
        McpTool(
            "cancel_qgis_binary_job",
            (
                "Request cancellation of a queued or running QGIS binary job and its "
                "spawned process tree. Returns the current job snapshot after the "
                "request, normally in state cancelling. Cancellation may complete "
                "asynchronously, so poll get_qgis_binary_job to confirm the "
                "terminal state, while already terminal jobs remain recorded. A "
                "process that exits before it is stopped can still end succeeded "
                "or failed, and a process tree that cannot be stopped ends failed "
                "with process_termination_failed. Partial output files are not "
                "removed. A cancelled job cannot resume, so start a new job to "
                "retry."
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
                    "list_qgis_binaries page. Omit it for the first page. If the "
                    "inventory or filters changed, restart without a cursor."
                ),
            },
            "limit": {
                "type": "integer",
                "minimum": 1,
                "maximum": QGIS_BINARY_MAX_LIST_LIMIT,
                "default": QGIS_BINARY_DEFAULT_LIST_LIMIT,
                "description": (
                    "Maximum number of binary summaries to return on this page, "
                    f"{QGIS_BINARY_DEFAULT_LIST_LIMIT} when omitted."
                ),
            },
            "query": {
                "type": "string",
                "maxLength": 256,
                "description": (
                    "Case-insensitive substring matched against binary ID, name, "
                    "description, package-relative path, and group, for example "
                    "gdalwarp or qgis_process."
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
                    "a command shell or argument rewriting. Each array item is one "
                    "argv entry, so do not add shell quoting or combine a switch and "
                    "its separate value into one item. Relative input and output paths "
                    "are interpreted by the executable from the effective working "
                    "directory, not from the MCP plugin directory. Prefer absolute "
                    "paths for deterministic file locations and create output parent "
                    "directories before starting because the service does not create "
                    "them. In formal restricted mode, normal local input and output "
                    "paths, including key=value values and QGIS semicolon path lists, "
                    "may be anywhere the QGIS process account can access. HTTP(S) "
                    "sources must use an approved origin. "
                    "Remote GDAL/OGR datasource connection strings and driver-prefixed "
                    "local datasource paths are rejected. Indirect argv files, including "
                    "@response files and GDAL/OGR --optfile forms, Windows device "
                    "paths, and existing files with multiple hard links are also "
                    "rejected."
                ),
            },
            "working_directory": {
                "type": "string",
                "minLength": 1,
                "maxLength": 32768,
                "description": (
                    "Existing normal local process working directory. The service does "
                    "not create it. Omission uses the QGIS package root, relative paths "
                    "resolve from that package root, and absolute paths are used as the "
                    "process working directory when the QGIS process account can access "
                    "them. Relative input and output argv paths then resolve from this "
                    "effective directory. Prefer an existing absolute directory when "
                    "file locations must be unambiguous."
                ),
            },
            "stdin": {
                "type": "string",
                "maxLength": MAX_STDIN_CHARS,
                "description": (
                    "UTF-8 text written to the process standard input. The selected "
                    "binary may prohibit stdin or impose a smaller byte limit. Formal "
                    "restricted mode permits only omitted or empty stdin because this "
                    "untyped channel could carry paths, data sources, or external "
                    "references, so non-empty stdin is rejected before job creation."
                ),
            },
            "timeout_seconds": {
                "type": "number",
                "exclusiveMinimum": 0,
                "maximum": MAX_TIMEOUT_SECONDS,
                "description": (
                    "Requested runtime timeout in seconds. It cannot exceed the "
                    "selected binary's configured timeout, and omission uses that "
                    "limit."
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
                    "Explicit per-request risk confirmation, required as true for "
                    "an R2 binary after the user approves the command. It does not "
                    "allow an R3 binary to run."
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
                "items": {"type": "string", "enum": list(PUBLIC_JOB_STATE_ORDER)},
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
                "maximum": QGIS_BINARY_MAX_LIST_LIMIT,
                "default": QGIS_BINARY_DEFAULT_LIST_LIMIT,
                "description": (
                    "Maximum number of recent job summaries to return, "
                    f"{QGIS_BINARY_DEFAULT_LIST_LIMIT} when omitted."
                ),
            },
        },
        "additionalProperties": False,
    }
