"""Contract and loopback HTTP tests for the complete QCopilots MCP tool inventory.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-08-25"
__copyright__ = "Copyright 2026, The QGIS Project"

import copy
import hashlib
import json
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest import mock
from urllib.request import Request, urlopen


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))


EXPECTED_TOOLS = {
    "qcopilots_mcp_server_builtin_tools": [
        "read_file",
        "get_file_metadata",
        "copy_file",
        "file_glob_search",
        "grep_search",
        "exec_shell_command",
        "write_file",
        "edit_file",
        "get_datetime",
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
        "describe_layer_sources",
        "list_map_layers",
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
    "qcopilots_mcp_server_builtin_tools": 9,
    "qcopilots_mcp_server_skills": 5,
    "qcopilots_mcp_server_interactive_tools": 33,
    "qcopilots_mcp_server_processing_vector": 6,
    "qcopilots_mcp_server_processing_raster": 6,
    "qcopilots_mcp_server_processing_general": 6,
    "qcopilots_mcp_server_qgis_binary": 6,
}

BRIDGE_SERVICES = {
    "qcopilots_mcp_server_interactive_tools",
    "qcopilots_mcp_server_processing_vector",
    "qcopilots_mcp_server_processing_raster",
    "qcopilots_mcp_server_processing_general",
    "qcopilots_mcp_server_qgis_binary",
}


class _RecordingBridge:
    calls = []

    def __init__(self, base_url):
        self.base_url = base_url

    def call(self, operation, arguments):
        self.calls.append((operation, copy.deepcopy(arguments)))
        return {"operation": operation, "arguments": arguments}


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
                    processing_tools.build_processing_tools("general")
                ),
                "qcopilots_mcp_server_qgis_binary": (
                    qgis_binary_tools.build_qgis_binary_tools()
                ),
            }

    def test_manifests_define_the_exact_71_tool_surface(self):
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

            self.assertEqual(len(all_names), 71)
            self.assertEqual(len(all_names), len(set(all_names)))

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

    def test_all_71_tool_contracts_complete_http_lifecycle_list_and_call(self):
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
                    )

        self.assertEqual(len(_RecordingBridge.calls), 57)

    def _exercise_http_surface(self, plugin_name, tools, workspace):
        from qcopilots_common.mcp_http import McpHttpServer

        controller = McpHttpServer(
            name=plugin_name,
            version="0.1.0",
            tools=tools,
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
                EXPECTED_TOOLS[plugin_name],
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
                self.assertFalse(called["result"].get("isError", False))
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


def _tool_call_arguments(plugin_name, tool, workspace):
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
        "get_datetime": {"timezone": "UTC"},
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
