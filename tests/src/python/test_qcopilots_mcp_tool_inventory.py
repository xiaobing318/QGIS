"""Contract and loopback HTTP tests for the complete QCopilots MCP tool inventory.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "Ethan.Yang"
__date__ = "2026-08-25"
__copyright__ = "Copyright 2026, The QGIS Project"

import copy
import hashlib
import json
import sys
import tempfile
import threading
import unittest
from dataclasses import dataclass, replace
from pathlib import Path
from unittest import mock
from urllib.request import Request, urlopen

PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))


EXPECTED_TOOLS = {
    "qcopilots_mcp_server_web_search": ["web_search", "web_open", "web_find", "web_search_status"],
    "qcopilots_mcp_server_builtin_tools": [
        "read_file",
        "get_file_metadata",
        "copy_file",
        "file_glob_search",
        "grep_search",
        "exec_shell_command",
        "write_file",
        "edit_file",
    ],
    "qcopilots_mcp_server_skills": [
        "list_skills",
        "read_skill",
        "list_skill_resources",
        "read_skill_resource",
        "qgis-skills-creator",
    ],
    "qcopilots_mcp_server_interactive_tools": [
        "list_layers",
        "set_layer_visibility",
        "zoom_to_layer",
        "zoom_to_extent",
        "zoom_in",
        "zoom_out",
        "zoom_full",
        "zoom_to_selection",
        "zoom_to_native_resolution",
        "zoom_to_last_extent",
        "zoom_to_next_extent",
        "save_project",
        "refresh_canvas",
        "export_map_image",
        "capture_qgis_screenshot",
        "describe_layer_sources",
        "load_map_layer",
        "remove_map_layers",
        "get_layer_metadata",
        "query_vector_features",
        "set_vector_selection",
        "delete_vector_features",
        "get_project_crs",
        "set_project_crs",
        "get_raster_statistics",
        "apply_layer_style",
        "set_vector_renderer",
        "configure_vector_labels",
        "create_print_layout",
        "list_print_layouts",
        "export_print_layout",
        "create_vector_layer",
        "add_vector_features",
        "update_vector_features",
    ],
    "qcopilots_mcp_server_processing_vector": [
        "list_vector_processing_algorithms",
        "get_vector_processing_algorithm_details",
        "start_vector_processing_algorithm",
        "get_vector_processing_job",
        "list_vector_processing_jobs",
        "cancel_vector_processing_job",
    ],
    "qcopilots_mcp_server_processing_raster": [
        "list_raster_processing_algorithms",
        "get_raster_processing_algorithm_details",
        "start_raster_processing_algorithm",
        "get_raster_processing_job",
        "list_raster_processing_jobs",
        "cancel_raster_processing_job",
    ],
    "qcopilots_mcp_server_processing_general": [
        "list_general_processing_algorithms",
        "get_general_processing_algorithm_details",
        "start_general_processing_algorithm",
        "get_general_processing_job",
        "list_general_processing_jobs",
        "cancel_general_processing_job",
        "file_transfer_get_config",
        "file_transfer_update_config",
        "file_transfer_test_connection",
        "file_transfer_trust_host_key",
        "file_transfer_start_upload",
        "file_transfer_start_download",
        "file_transfer_get_job",
        "file_transfer_list_jobs",
        "file_transfer_cancel_job",
        "file_transfer_resume_job",
        "file_transfer_discard",
    ],
    "qcopilots_mcp_server_qgis_binary": [
        "list_qgis_binaries",
        "get_qgis_binary_details",
        "start_qgis_binary",
        "get_qgis_binary_job",
        "list_qgis_binary_jobs",
        "cancel_qgis_binary_job",
    ],
}

EXPECTED_COUNTS = {
    "qcopilots_mcp_server_web_search": 4,
    "qcopilots_mcp_server_builtin_tools": 8,
    "qcopilots_mcp_server_skills": 5,
    "qcopilots_mcp_server_interactive_tools": 34,
    "qcopilots_mcp_server_processing_vector": 6,
    "qcopilots_mcp_server_processing_raster": 6,
    "qcopilots_mcp_server_processing_general": 17,
    "qcopilots_mcp_server_qgis_binary": 6,
}

BRIDGE_SERVICES = {
    "qcopilots_mcp_server_interactive_tools",
    "qcopilots_mcp_server_processing_vector",
    "qcopilots_mcp_server_processing_raster",
    "qcopilots_mcp_server_processing_general",
    "qcopilots_mcp_server_qgis_binary",
}

NORMALIZED_SCHEMA_TOOLS = {
    "web_open",
    "edit_file",
    "read_skill",
    "list_skill_resources",
    "read_skill_resource",
    "remove_map_layers",
    "set_vector_selection",
    "delete_vector_features",
    "configure_vector_labels",
    "list_vector_processing_algorithms",
    "list_raster_processing_algorithms",
    "list_general_processing_algorithms",
}


@dataclass(frozen=True)
class _CanvasBehavior:
    category: str
    tools: tuple[str, ...]
    expectation: str
    regression_suites: tuple[str, ...]


# Audit every public tool, including operations whose GUI effects happen later
# in a Processing completion or recovery callback. These are behavioral review
# requirements, not a claim that the recording bridge below verifies rendering.
# The named suites exercise the relevant native QGIS and lifecycle paths.
# A new registration must be classified explicitly rather than inheriting a
# default "read only" assumption. Files and subprocesses retain QGIS's native
# behavior: these wrappers do not monitor arbitrary external data changes.
CANVAS_BEHAVIOR_MATRIX = (
    _CanvasBehavior(
        "project_change",
        ("load_map_layer", "create_vector_layer"),
        "When added to the project, native layer-tree updates reach relevant 2D "
        "canvases. The first spatial layer can establish the initial extent. "
        "Hidden groups and independent themes retain their native visibility "
        "rules. load_map_layer's optional refresh_canvas also requests an extra "
        "main-canvas refresh, independently of add_to_project.",
        ("test_qcopilots_canvas_updates.py",),
    ),
    _CanvasBehavior(
        "project_change",
        ("remove_map_layers",),
        "Actual removals request lightweight refresh on all 2D canvases, including "
        "the last layer and batch removals. Preview, rejected confirmation and "
        "no-op removal do not refresh. Frozen and disabled render states persist.",
        ("test_qcopilots_canvas_updates.py", "test_qcopilots_mcp_server_interactive_tools.py"),
    ),
    _CanvasBehavior(
        "project_change",
        ("set_project_crs",),
        "An actual CRS change invalidates all 2D canvas caches so project "
        "expressions are reevaluated. Additional views keep their independent CRS.",
        ("test_qcopilots_canvas_updates.py",),
    ),
    _CanvasBehavior(
        "project_change",
        (
            "start_vector_processing_algorithm",
            "start_raster_processing_algorithm",
            "start_general_processing_algorithm",
        ),
        "Successful asynchronous result loading uses native Processing "
        "postprocessing. File-only outputs and add_outputs_to_project=false "
        "do not add layers. Postprocessing failure can roll back already drawn "
        "outputs and must refresh after actual removals.",
        ("test_qcopilots_processing_jobs.py", "test_qcopilots_processing_task_chain.py"),
    ),
    _CanvasBehavior(
        "project_change",
        (
            "get_vector_processing_job", "get_raster_processing_job",
            "get_general_processing_job", "cancel_vector_processing_job",
            "cancel_raster_processing_job", "cancel_general_processing_job",
        ),
        "Get may retry incomplete terminal cleanup. Cancel may initiate immediate "
        "or asynchronous recovery. Refresh only after actual project-layer "
        "removal, including partial removal and retry, without masking file "
        "recovery errors. Reading an ordinary job state does not refresh.",
        ("test_qcopilots_processing_jobs.py", "test_qcopilots_processing_task_chain.py"),
    ),
    _CanvasBehavior(
        "project_change",
        ("create_print_layout",),
        "Adds or replaces a layout in the project using native layout APIs. "
        "It does not change the 2D canvas view or require a canvas refresh.",
        ("test_qcopilots_mcp_server_interactive_tools.py",),
    ),
    _CanvasBehavior(
        "layer_change",
        ("set_layer_visibility",),
        "Native layer-tree visibility propagation honors hidden parents and "
        "independent map themes without overwriting additional view settings.",
        ("test_qcopilots_canvas_updates.py",),
    ),
    _CanvasBehavior(
        "layer_change",
        ("set_vector_selection",),
        "Selection signals update canvases showing the layer without changing "
        "their extents or requiring the caller to request an explicit refresh.",
        ("test_qcopilots_canvas_updates.py", "test_qcopilots_mcp_server_interactive_tools.py"),
    ),
    _CanvasBehavior(
        "layer_change",
        ("apply_layer_style", "set_vector_renderer", "configure_vector_labels"),
        "Layer repaint notifications invalidate affected cached images in all "
        "relevant canvases. Theme overrides retain native precedence.",
        ("test_qcopilots_layer_styles.py", "test_qcopilots_canvas_updates.py"),
    ),
    _CanvasBehavior(
        "layer_change",
        ("add_vector_features", "update_vector_features", "delete_vector_features"),
        "Successful edits notify every canvas showing the layer. Cover ordinary "
        "editing and GeoPackage automatic transactions with cached rendering, "
        "attribute-driven colors, geometry changes, addition and confirmed deletion. "
        "Preserve edit buffers, undo and confirmation semantics.",
        ("test_qcopilots_canvas_updates.py", "test_qcopilots_mcp_server_interactive_tools.py"),
    ),
    _CanvasBehavior(
        "view_change",
        (
            "zoom_to_layer", "zoom_to_extent", "zoom_in", "zoom_out", "zoom_full",
            "zoom_to_selection", "zoom_to_native_resolution", "zoom_to_last_extent",
            "zoom_to_next_extent",
        ),
        "Operate on the main canvas, with additional views following their native "
        "synchronization settings. Native-resolution zoom accounts for raster CRS, "
        "provider resolution levels and device pixel ratio. Keep the public "
        "single-point selection zoom fallback.",
        ("test_qcopilots_canvas_updates.py", "test_qcopilots_mcp_server_interactive_tools.py"),
    ),
    _CanvasBehavior(
        "view_change",
        ("refresh_canvas",),
        "Request the native Refresh/F5 action across all 2D canvases, including "
        "source reload and cache invalidation. The response acknowledges an "
        "asynchronous request rather than completion of rendering.",
        ("test_qcopilots_canvas_updates.py",),
    ),
    _CanvasBehavior(
        "image_read",
        ("capture_qgis_screenshot",),
        "Wait for queued tree updates and pending renders of canvases actually "
        "included in the capture. Respect crop, floating windows, frozen and "
        "disabled render states. Zero wait and timeout return current pixels with "
        "feedback, without forcing refresh or blocking the GUI event loop.",
        ("test_qcopilots_screenshot.py",),
    ),
    _CanvasBehavior(
        "image_read",
        ("export_map_image",),
        "Render an independent current-settings snapshot so pixels and the world "
        "file agree even before interactive rendering starts. Preserve rotation, "
        "DPI, themes, selection and annotations. Timeout, shutdown and late task "
        "completion must not publish abandoned output.",
        ("test_qcopilots_map_export.py",),
    ),
    _CanvasBehavior(
        "image_read",
        ("export_print_layout",),
        "Use the native layout exporter and layout map settings, independently "
        "of any stale interactive canvas frame. Keep existing output contracts.",
        ("test_qcopilots_mcp_server_interactive_tools.py",),
    ),
    _CanvasBehavior(
        "no_canvas_effect",
        (
            "list_layers", "describe_layer_sources", "get_layer_metadata",
            "query_vector_features", "get_project_crs", "get_raster_statistics",
            "list_print_layouts", "save_project",
        ),
        "Read current project or layer state, or serialize the project. No "
        "automatic view changes or additional canvas redraws are required.",
        ("test_qcopilots_mcp_server_interactive_tools.py",),
    ),
    _CanvasBehavior(
        "no_canvas_effect",
        (
            "list_vector_processing_algorithms", "get_vector_processing_algorithm_details",
            "list_vector_processing_jobs", "list_raster_processing_algorithms",
            "get_raster_processing_algorithm_details", "list_raster_processing_jobs",
            "list_general_processing_algorithms", "get_general_processing_algorithm_details",
            "list_general_processing_jobs",
        ),
        "Algorithm discovery and job-list summaries do not load outputs or retry "
        "recovery. Unlike get-job, list-jobs has no project cleanup side effect.",
        ("test_qcopilots_processing_jobs.py", "test_qcopilots_mcp_server_processing_tools.py"),
    ),
    _CanvasBehavior(
        "no_canvas_effect",
        (
            "read_file", "get_file_metadata", "copy_file", "file_glob_search",
            "grep_search", "exec_shell_command", "write_file", "edit_file",
        ),
        "Filesystem and shell tools do not inject canvas refreshes or monitor "
        "arbitrary external file modifications. QGIS retains its native provider "
        "behavior when external data changes.",
        ("test_qcopilots_mcp_server_builtin_tools.py",),
    ),
    _CanvasBehavior(
        "no_canvas_effect",
        ("list_skills", "read_skill", "list_skill_resources", "read_skill_resource", "qgis-skills-creator"),
        "Skill discovery and resource reads do not execute the described QGIS "
        "operations or require canvas synchronization.",
        ("test_qcopilots_mcp_server_skills.py",),
    ),
    _CanvasBehavior(
        "no_canvas_effect",
        (
            "file_transfer_get_config", "file_transfer_update_config",
            "file_transfer_test_connection", "file_transfer_trust_host_key",
            "file_transfer_start_upload", "file_transfer_start_download",
            "file_transfer_get_job", "file_transfer_list_jobs",
            "file_transfer_cancel_job", "file_transfer_resume_job", "file_transfer_discard",
        ),
        "Transfer configuration and asynchronous file operations do not load "
        "project layers or install external-file monitoring.",
        ("test_qcopilots_mcp_server_file_transfer_service.py",),
    ),
    _CanvasBehavior(
        "no_canvas_effect",
        (
            "list_qgis_binaries", "get_qgis_binary_details", "start_qgis_binary",
            "get_qgis_binary_job", "list_qgis_binary_jobs", "cancel_qgis_binary_job",
        ),
        "Binary discovery and subprocess jobs do not automatically add their "
        "outputs to the project or install external-file monitoring.",
        ("test_qcopilots_qgis_binary_tools.py", "test_qcopilots_qgis_binary_jobs.py"),
    ),
    _CanvasBehavior(
        "no_canvas_effect",
        ("web_search", "web_open", "web_find", "web_search_status"),
        "WebSearch reads public material through an independent service and does not modify the QGIS canvas.",
        ("test_qcopilots_mcp_server_web_search.py",),
    ),
)


class _RecordingBridge:
    calls = []

    def __init__(self, base_url):
        self.base_url = base_url

    def call(self, operation, arguments):
        self.calls.append((operation, copy.deepcopy(arguments)))
        return {"operation": operation, "arguments": arguments}


class _RecordingFileTransferManager:
    """Exercise the public wrappers without sockets or persistent user settings."""

    def __init__(self, error=None):
        self.config_store = self
        self.backend = self
        self.calls = []
        self.error = error

    def _record(self, operation, **arguments):
        self.calls.append((operation, copy.deepcopy(arguments)))
        if self.error is not None:
            raise self.error
        return {"operation": operation, "arguments": arguments}

    def load(self, require_connection=False):
        # The credentials only reach the injected backend, never a real server.
        return {
            "host": "inventory.invalid",
            "port": 22,
            "username": "inventory",
            "password": "private-inventory-password",
        }

    def public(self):
        return self._record("file_transfer_get_config")

    def update(self, changes):
        return self._record("file_transfer_update_config", changes=changes)

    def test_connection(self, config):
        return self._record("file_transfer_test_connection", config=config)

    def trust_host(self, config, fingerprint, confirmation_token):
        return self._record(
            "file_transfer_trust_host_key",
            config=config,
            fingerprint=fingerprint,
            confirmation_token=confirmation_token,
        )

    def start(
        self,
        direction,
        sources=None,
        destination=None,
        overwrite=False,
        client_request_id=None,
    ):
        return self._record(
            f"file_transfer_start_{direction}",
            sources=sources,
            destination=destination,
            overwrite=overwrite,
            client_request_id=client_request_id,
        )

    def get_job(self, job_id, files_offset=0, files_limit=100):
        return self._record("file_transfer_get_job", job_id=job_id, files_offset=files_offset, files_limit=files_limit)

    def list_jobs(self, states=None, limit=100):
        return self._record("file_transfer_list_jobs", states=states, limit=limit)

    def cancel_job(self, job_id):
        return self._record("file_transfer_cancel_job", job_id=job_id)

    def resume_job(self, job_id, client_request_id=None):
        return self._record(
            "file_transfer_resume_job", job_id=job_id, client_request_id=client_request_id
        )

    def discard_transfer(self, transfer_id):
        return self._record("file_transfer_discard", transfer_id=transfer_id)


class TestQCopilotsMcpToolInventory(unittest.TestCase):
    HTTP_AUTH_TOKEN = "qcopilots-inventory-test-token"

    def setUp(self):
        _RecordingBridge.calls = []

    def _build_surfaces(self, workspace):
        from qcopilots_common import (
            builtin_tools,
            interactive_layer_tools,
            processing_tools,
            qgis_binary_tools,
            skills,
        )
        from qcopilots_common.security_policy import filesystem_policy_from_config
        from qcopilots_mcp_server_processing_general.server import build_general_tools

        from qcopilots_mcp_server_web_search.tools import build_web_search_tools
        fake_websearch = mock.Mock()
        fake_websearch.call.return_value = {"contract_version": 1}
        self.file_transfer_manager = _RecordingFileTransferManager()
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {
                    "enabled": True,
                    "executables": [sys.executable],
                },
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        skills_plugin = PLUGINS_ROOT / "qcopilots_mcp_server_skills"
        with mock.patch.object(
            processing_tools, "BridgeClient", _RecordingBridge
        ), mock.patch.object(
            interactive_layer_tools, "BridgeClient", _RecordingBridge
        ), mock.patch.object(qgis_binary_tools, "BridgeClient", _RecordingBridge):
            return {
                "qcopilots_mcp_server_web_search": build_web_search_tools(client=fake_websearch),
                "qcopilots_mcp_server_builtin_tools": builtin_tools.build_builtin_tools(
                    workspace,
                    allowed_roots=[workspace],
                    allow_full_access=True,
                    filesystem_policy=policy,
                ),
                "qcopilots_mcp_server_skills": skills.build_skill_tools(skills_plugin),
                "qcopilots_mcp_server_interactive_tools": (
                    processing_tools.build_interactive_tools()
                ),
                "qcopilots_mcp_server_processing_vector": (
                    processing_tools.build_processing_tools("vector")
                ),
                "qcopilots_mcp_server_processing_raster": (
                    processing_tools.build_processing_tools("raster")
                ),
                "qcopilots_mcp_server_processing_general": (
                    build_general_tools(manager=self.file_transfer_manager)
                ),
                "qcopilots_mcp_server_qgis_binary": (
                    qgis_binary_tools.build_qgis_binary_tools()
                ),
            }

    def test_manifests_define_the_exact_tool_surface(self):
        with tempfile.TemporaryDirectory(prefix="qcopilots-inventory-") as workspace:
            surfaces = self._build_surfaces(Path(workspace))

            self.assertEqual(set(surfaces), set(EXPECTED_TOOLS))
            all_names = []
            for plugin_name, expected_names in EXPECTED_TOOLS.items():
                with self.subTest(plugin=plugin_name):
                    manifest_path = PLUGINS_ROOT / plugin_name / "qcopilots_service.json"
                    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                    service_id = plugin_name.replace("_", ".", 1)
                    self.assertEqual(manifest["service_id"], service_id)
                    self.assertIn("tools", manifest["capabilities"])
                    if plugin_name in BRIDGE_SERVICES:
                        self.assertIn("qgis-bridge", manifest["capabilities"])
                    if "mcp_server_processing_" in plugin_name:
                        self.assertIn("异步作业", manifest["description"])
                        self.assertIn("默认添加到当前工程", manifest["description"])
                        self.assertIn("一次性确认令牌", manifest["description"])

                    actual_names = [tool.name for tool in surfaces[plugin_name]]
                    self.assertEqual(actual_names, expected_names)
                    self.assertEqual(len(actual_names), EXPECTED_COUNTS[plugin_name])
                    self.assertEqual(len(actual_names), len(set(actual_names)))
                    for tool in surfaces[plugin_name]:
                        descriptor = tool.descriptor()
                        self.assertEqual(descriptor["name"], tool.name)
                        self.assertEqual(descriptor["inputSchema"]["type"], "object")
                    all_names.extend(actual_names)

            self.assertEqual(len(all_names), sum(EXPECTED_COUNTS.values()))
            self.assertEqual(len(all_names), len(set(all_names)))

    def test_every_registered_tool_has_one_explicit_canvas_behavior(self):
        """Require a visual-effects review when a public tool is added or removed."""
        with tempfile.TemporaryDirectory(prefix="qcopilots-canvas-inventory-") as workspace:
            surfaces = self._build_surfaces(Path(workspace))
        actual_names = {tool.name for tools in surfaces.values() for tool in tools}
        classified_names = [name for row in CANVAS_BEHAVIOR_MATRIX for name in row.tools]
        self.assertCountEqual(classified_names, actual_names)
        self.assertEqual(len(classified_names), len(set(classified_names)))
        for row in CANVAS_BEHAVIOR_MATRIX:
            with self.subTest(tools=row.tools):
                self.assertIn(row.category, {
                    "project_change", "layer_change", "view_change", "image_read", "no_canvas_effect",
                })
                for suite in row.regression_suites:
                    self.assertTrue(Path(__file__).with_name(suite).is_file(), suite)

    def test_minimal_arguments_supports_any_of_array_items(self):
        schema = {
            "type": "object",
            "properties": {
                "updates": {
                    "type": "array",
                    "minItems": 1,
                    "items": {
                        "anyOf": [
                            {
                                "type": "object",
                                "properties": {
                                    "feature_id": {"type": "integer"},
                                    "attributes": {
                                        "type": "object",
                                        "minProperties": 1,
                                    },
                                },
                                "required": ["feature_id", "attributes"],
                                "additionalProperties": False,
                            },
                            {
                                "type": "object",
                                "properties": {
                                    "feature_id": {"type": "integer"},
                                    "geometry_wkt": {
                                        "type": "string",
                                        "minLength": 1,
                                    },
                                },
                                "required": ["feature_id", "geometry_wkt"],
                                "additionalProperties": False,
                            },
                        ]
                    },
                }
            },
            "required": ["updates"],
            "additionalProperties": False,
        }

        arguments = _minimal_arguments(schema)

        self.assertEqual(arguments["updates"][0]["feature_id"], 0)
        self.assertTrue(arguments["updates"][0]["attributes"])

    def test_all_descriptors_preserve_parameters_and_canonical_schemas(self):
        """The advertised schema must not hide parameters or alter validation."""
        with tempfile.TemporaryDirectory(prefix="qcopilots-schema-inventory-") as workspace:
            surfaces = self._build_surfaces(Path(workspace))
            normalized_names = set()
            for tools in surfaces.values():
                for tool in tools:
                    with self.subTest(tool=tool.name):
                        canonical = copy.deepcopy(tool.input_schema)
                        advertised = tool.descriptor()["inputSchema"]
                        self.assertEqual(tool.input_schema, canonical)
                        self.assertEqual(advertised["type"], "object")
                        self.assertEqual(
                            list(advertised.get("properties", {})),
                            list(canonical.get("properties", {})),
                        )
                        self.assertEqual(
                            advertised.get("required"), canonical.get("required")
                        )
                        self.assertEqual(
                            advertised.get("additionalProperties"),
                            canonical.get("additionalProperties"),
                        )
                        self.assertNotIn("oneOf", advertised)
                        self.assertNotIn("anyOf", advertised)
                        if advertised != canonical:
                            normalized_names.add(tool.name)

                        # Consumer edits to one descriptor must not mutate the
                        # execution schema or a subsequently listed descriptor.
                        expected = copy.deepcopy(advertised)
                        advertised.setdefault("properties", {})["inventory_probe"] = {
                            "type": "string"
                        }
                        self.assertEqual(tool.input_schema, canonical)
                        self.assertEqual(tool.descriptor()["inputSchema"], expected)

            self.assertEqual(normalized_names, NORMALIZED_SCHEMA_TOOLS)

    def test_all_tool_contracts_complete_http_lifecycle_list_and_call(self):
        """Exercise every public wrapper over HTTP with a recording QGIS bridge.

        Functional QGIS state and subprocess behavior is covered by the focused
        integration tests and the Launcher-based live acceptance runner. This
        test intentionally isolates the MCP lifecycle, schemas, and wrapper
        routing from those external dependencies.
        """
        with tempfile.TemporaryDirectory(prefix="qcopilots-http-inventory-") as workspace:
            surfaces = self._build_surfaces(Path(workspace))
            for plugin_name, tools in surfaces.items():
                with self.subTest(plugin=plugin_name):
                    self._exercise_http_surface(
                        plugin_name,
                        tools,
                        Path(workspace),
                        reject_conflicting_arguments=True,
                    )

        self.assertEqual(len(_RecordingBridge.calls), 58)
        recorded_operations = [operation for operation, _ in _RecordingBridge.calls]
        self.assertEqual(
            recorded_operations.count("interactive_capture_screenshot"), 1
        )
        self.assertEqual(
            recorded_operations.index("interactive_capture_screenshot"),
            recorded_operations.index("export_map_image") + 1,
        )
        self.assertEqual(
            [operation for operation, _ in self.file_transfer_manager.calls],
            EXPECTED_TOOLS["qcopilots_mcp_server_processing_general"][6:],
        )

    def test_file_transfer_errors_complete_http_lifecycle_without_leaking_exception_text(self):
        from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError
        from qcopilots_mcp_server_processing_general.file_transfer_tools import build_file_transfer_tools

        with tempfile.TemporaryDirectory(prefix="qcopilots-file-transfer-http-errors-") as workspace:
            for error, expected_code in (
                (FileTransferError("connection_failed", "Cannot reach the server."), "connection_failed"),
                (
                    FileTransferError(
                        "authentication_failed",
                        "Authentication failed for private-inventory-password.",
                    ),
                    "authentication_failed",
                ),
                (RuntimeError("private-inventory-password"), "internal_error"),
            ):
                with self.subTest(code=expected_code):
                    manager = _RecordingFileTransferManager(error=error)
                    tools = build_file_transfer_tools(manager=manager)
                    self._exercise_http_surface(
                        "qcopilots_mcp_server_processing_general",
                        tools,
                        Path(workspace),
                        expected_error=expected_code,
                    )
                    self.assertEqual(
                        [operation for operation, _ in manager.calls],
                        [tool.name for tool in tools],
                    )

    def _exercise_http_surface(
        self,
        plugin_name,
        tools,
        workspace,
        expected_error=None,
        reject_conflicting_arguments=False,
    ):
        from qcopilots_common.mcp_http import McpHttpServer

        handlers = {tool.name: mock.Mock(wraps=tool.handler) for tool in tools}
        controller = McpHttpServer(
            name=plugin_name,
            version="0.1.0",
            tools=[replace(tool, handler=handlers[tool.name]) for tool in tools],
            port=0,
            auth_token=self.HTTP_AUTH_TOKEN,
        )
        httpd = None
        thread = None
        session_id = None
        cleanup_errors = []
        try:
            httpd = controller.create_http_server()
            port = httpd.server_address[1]
            thread = threading.Thread(target=httpd.serve_forever, daemon=True)
            thread.start()
            initialize_status, initialize_headers, initialize = self._post(
                port,
                {
                    "jsonrpc": "2.0",
                    "id": "initialize",
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2025-06-18",
                        "capabilities": {},
                        "clientInfo": {
                            "name": "qcopilots-inventory-test",
                            "version": "1.0",
                        },
                    },
                },
            )
            self.assertEqual(initialize_status, 200)
            self.assertEqual(
                initialize["result"]["protocolVersion"],
                "2025-06-18",
            )
            session_id = initialize_headers["mcp-session-id"]
            self.assertTrue(session_id)

            initialized_status, _, initialized = self._post(
                port,
                {
                    "jsonrpc": "2.0",
                    "method": "notifications/initialized",
                },
                session_id,
            )
            self.assertEqual(initialized_status, 202)
            self.assertIsNone(initialized)

            list_status, _, listed = self._post(
                port,
                {
                    "jsonrpc": "2.0",
                    "id": "list",
                    "method": "tools/list",
                    "params": {},
                },
                session_id,
            )
            self.assertEqual(list_status, 200)
            self.assertEqual(
                [item["name"] for item in listed["result"]["tools"]],
                [tool.name for tool in tools],
            )
            self.assertEqual(
                listed["result"]["tools"], [tool.descriptor() for tool in tools]
            )

            for index, tool in enumerate(tools):
                arguments = _tool_call_arguments(
                    plugin_name,
                    tool,
                    workspace,
                )
                call_status, _, called = self._post(
                    port,
                    {
                        "jsonrpc": "2.0",
                        "id": f"call-{index}",
                        "method": "tools/call",
                        "params": {
                            "name": tool.name,
                            "arguments": arguments,
                        },
                    },
                    session_id,
                )
                self.assertEqual(call_status, 200)
                self.assertNotIn("error", called)
                if expected_error is None:
                    self.assertFalse(called["result"].get("isError", False))
                else:
                    self.assertTrue(called["result"]["isError"])
                    self.assertEqual(
                        called["result"]["structuredContent"]["error"]["code"],
                        expected_error,
                    )
                self.assertNotIn(
                    "private-inventory-password", json.dumps(called["result"])
                )
                if reject_conflicting_arguments:
                    invalid_arguments = _conflicting_tool_arguments(tool.name, arguments)
                    if invalid_arguments is not None:
                        before_calls = handlers[tool.name].call_count
                        invalid_status, _, rejected = self._post(
                            port,
                            {
                                "jsonrpc": "2.0",
                                "id": f"conflict-{index}",
                                "method": "tools/call",
                                "params": {
                                    "name": tool.name,
                                    "arguments": invalid_arguments,
                                },
                            },
                            session_id,
                        )
                        self.assertEqual(invalid_status, 200)
                        self.assertEqual(rejected["error"]["code"], -32602)
                        self.assertIn(
                            "Invalid tool arguments", rejected["error"]["message"]
                        )
                        self.assertEqual(handlers[tool.name].call_count, before_calls)
            if plugin_name == "qcopilots_mcp_server_builtin_tools":
                self.assertEqual(
                    (workspace / "source.txt").read_text(encoding="utf-8"),
                    "alpha\nneedle\n",
                )
                self.assertEqual(
                    (workspace / "copied.txt").read_text(encoding="utf-8"),
                    "alpha\nneedle\n",
                )
                self.assertEqual(
                    (workspace / "written.txt").read_text(encoding="utf-8"),
                    "updated by HTTP\n",
                )
        finally:
            if session_id:
                try:
                    delete = Request(
                        f"http://127.0.0.1:{port}/mcp",
                        headers=self._headers(session_id),
                        method="DELETE",
                    )
                    with urlopen(delete, timeout=5) as response:
                        if response.status != 204 or response.read() != b"":
                            cleanup_errors.append(
                                "MCP session DELETE did not return an empty 204 response"
                            )
                except Exception as err:
                    cleanup_errors.append(f"MCP session DELETE failed: {err}")
            if httpd is not None:
                httpd.shutdown()
                httpd.server_close()
            if thread is not None:
                thread.join(timeout=5)
                if thread.is_alive():
                    cleanup_errors.append("MCP HTTP test server leaked")
            if cleanup_errors:
                self.fail("; ".join(cleanup_errors))

    def _post(self, port, payload, session_id=None):
        request = Request(
            f"http://127.0.0.1:{port}/mcp",
            data=json.dumps(payload).encode("utf-8"),
            headers=self._headers(session_id),
            method="POST",
        )
        with urlopen(request, timeout=5) as response:
            body = response.read()
            return (
                response.status,
                response.headers,
                json.loads(body.decode("utf-8")) if body else None,
            )

    def _headers(self, session_id=None):
        headers = {
            "Accept": "application/json, text/event-stream",
            "Authorization": f"Bearer {self.HTTP_AUTH_TOKEN}",
            "Content-Type": "application/json",
        }
        if session_id:
            headers.update(
                {
                    "MCP-Protocol-Version": "2025-06-18",
                    "MCP-Session-Id": session_id,
                }
            )
        return headers


def _minimal_arguments(schema):
    from qcopilots_common.mcp_http import ToolError, _validate_tool_arguments

    candidates = _object_candidates(schema)
    for candidate in candidates:
        try:
            _validate_tool_arguments(candidate, schema)
        except ToolError:
            continue
        return candidate
    raise AssertionError(f"Could not construct valid arguments for schema: {schema}")


def _conflicting_tool_arguments(name, valid_arguments):
    if name not in NORMALIZED_SCHEMA_TOOLS:
        return None
    if name == "edit_file":
        return {**valid_arguments, "old_text": "duplicate edit selector"}
    if name in {"read_skill", "list_skill_resources", "read_skill_resource"}:
        return {**valid_arguments, "name": "duplicate skill selector"}
    if name == "remove_map_layers":
        return {"layer_id": "inventory-layer", "confirmation_token": "x" * 16}
    if name == "set_vector_selection":
        return {"layer_id": "inventory-layer", "mode": "clear", "feature_ids": [1]}
    if name == "delete_vector_features":
        return {
            "layer_id": "inventory-layer",
            "feature_ids": [1],
            "confirmation_token": "x" * 16,
        }
    if name == "configure_vector_labels":
        return {
            "layer_id": "inventory-layer",
            "enabled": False,
            "field_or_expression": "id",
        }
    return {"cursor": "inventory-cursor", "max_results": 1}


def _tool_call_arguments(plugin_name, tool, workspace):
    if plugin_name == "qcopilots_mcp_server_web_search":
        return {
            "web_search": {"query": "QGIS"},
            "web_open": {"url": "https://example.invalid"},
            "web_find": {"page_id": "inventory-page", "pattern": "QGIS", "start_line": 1},
            "web_search_status": {},
        }[tool.name]
    if plugin_name == "qcopilots_mcp_server_builtin_tools":
        return _builtin_call_arguments(workspace)[tool.name]
    if plugin_name == "qcopilots_mcp_server_skills":
        return _skill_call_arguments()[tool.name]
    return _minimal_arguments(tool.input_schema)


def _builtin_call_arguments(workspace):
    source = workspace / "source.txt"
    if not source.exists():
        source.write_text("alpha\nneedle\n", encoding="utf-8")
    source_sha256 = hashlib.sha256(source.read_bytes()).hexdigest()
    written_content = "created by HTTP\n"
    written_sha256 = hashlib.sha256(written_content.encode("utf-8")).hexdigest()
    return {
        "read_file": {"path": str(source)},
        "get_file_metadata": {"path": str(source)},
        "copy_file": {
            "source": str(source),
            "target": str(workspace / "copied.txt"),
            "expected_source_sha256": source_sha256,
        },
        "file_glob_search": {
            "path": str(workspace),
            "include": "*.txt",
        },
        "grep_search": {
            "path": str(workspace),
            "pattern": "needle",
        },
        "exec_shell_command": {
            "command": [sys.executable, "-c", "print('qcopilots-http')"],
            "cwd": str(workspace),
        },
        "write_file": {
            "path": str(workspace / "written.txt"),
            "content": written_content,
        },
        "edit_file": {
            "path": str(workspace / "written.txt"),
            "search": "created",
            "replace": "updated",
            "expected_sha256": written_sha256,
        },
    }


def _skill_call_arguments():
    selector = {"slug": "qgis-skills-creator"}
    return {
        "list_skills": {},
        "read_skill": dict(selector),
        "list_skill_resources": dict(selector),
        "read_skill_resource": {
            **selector,
            "resource": "references/qgis-tools.catalog.schema.json",
        },
        "qgis-skills-creator": {
            "task": "Validate a deterministic QGIS skill creation plan."
        },
    }


def _object_candidates(schema):
    base = _required_object_values(schema)
    minimum_properties = int(schema.get("minProperties", 0))
    additional = schema.get("additionalProperties", True)
    while len(base) < minimum_properties:
        name = f"value_{len(base) + 1}"
        if name not in base:
            base[name] = (
                _minimal_value(additional)
                if isinstance(additional, dict)
                else None
            )
    alternatives = schema.get("oneOf") or schema.get("anyOf") or []
    if not alternatives:
        return [base]

    candidates = []
    for alternative in alternatives:
        if not isinstance(alternative, dict):
            continue
        candidate = dict(base)
        properties = dict(schema.get("properties", {}))
        properties.update(alternative.get("properties", {}))
        for name in alternative.get("required", []):
            property_schema = properties.get(name, {})
            if property_schema is not False:
                candidate[name] = _minimal_value(property_schema)
        candidates.append(candidate)
    return candidates or [base]


def _required_object_values(schema):
    properties = schema.get("properties", {})
    return {
        name: _minimal_value(properties.get(name, {}))
        for name in schema.get("required", [])
        if properties.get(name, {}) is not False
    }


def _minimal_value(schema):
    if schema is True or not isinstance(schema, dict):
        return None
    if "const" in schema:
        return copy.deepcopy(schema["const"])
    if "enum" in schema:
        return copy.deepcopy(schema["enum"][0])
    if "default" in schema:
        return copy.deepcopy(schema["default"])

    alternatives = schema.get("oneOf") or schema.get("anyOf") or []
    for alternative in alternatives:
        if not isinstance(alternative, dict):
            continue
        candidate_schema = copy.deepcopy(schema)
        candidate_schema.pop("oneOf", None)
        candidate_schema.pop("anyOf", None)
        shared_properties = candidate_schema.get("properties")
        shared_required = candidate_schema.get("required")
        alternative_properties = alternative.get("properties")
        alternative_required = alternative.get("required")
        candidate_schema.update(copy.deepcopy(alternative))
        if isinstance(shared_properties, dict) and isinstance(
            alternative_properties, dict
        ):
            candidate_schema["properties"] = {
                **shared_properties,
                **alternative_properties,
            }
        if isinstance(shared_required, list) and isinstance(
            alternative_required, list
        ):
            candidate_schema["required"] = list(
                dict.fromkeys([*shared_required, *alternative_required])
            )
        try:
            candidate = _minimal_value(candidate_schema)
        except AssertionError:
            continue

        from qcopilots_common.mcp_http import ToolError, _validate_tool_arguments

        wrapper_schema = {
            "type": "object",
            "properties": {"value": schema},
            "required": ["value"],
            "additionalProperties": False,
        }
        try:
            _validate_tool_arguments({"value": candidate}, wrapper_schema)
        except ToolError:
            continue
        return candidate
    if alternatives:
        raise AssertionError(f"Could not construct valid value for schema: {schema}")

    schema_type = schema.get("type")
    if isinstance(schema_type, list):
        schema_type = next(
            (item for item in schema_type if item != "null"),
            schema_type[0],
        )
    if schema_type == "object":
        return _minimal_arguments(schema)
    if schema_type == "array":
        count = int(schema.get("minItems", 0))
        return [_minimal_value(schema.get("items", {})) for _ in range(count)]
    if schema_type == "boolean":
        return False
    if schema_type in {"integer", "number"}:
        if "exclusiveMinimum" in schema:
            return schema["exclusiveMinimum"] + 1
        return schema.get("minimum", 0)
    if schema_type == "null":
        return None

    minimum = max(1, int(schema.get("minLength", 1)))
    pattern = str(schema.get("pattern") or "")
    if "qQ" in pattern and "gG" in pattern and "zZ" in pattern:
        return "test.qgz"
    return "x" * minimum


if __name__ == "__main__":
    unittest.main()
