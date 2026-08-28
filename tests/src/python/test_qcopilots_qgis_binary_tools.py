"""Tests for the QCopilots QGIS Binary MCP tool surface.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-08-02"
__copyright__ = "Copyright 2026, The QGIS Project"

import ast
import json
import sys
import unittest
from pathlib import Path
from unittest import mock


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))

from qcopilots_common import qgis_binary_tools


SERVICE_ID = "qcopilots.mcp_server_qgis_binary"
EXPECTED_TOOL_NAMES = [
    "list_qgis_binaries",
    "get_qgis_binary_details",
    "start_qgis_binary",
    "get_qgis_binary_job",
    "list_qgis_binary_jobs",
    "cancel_qgis_binary_job",
]
EXPECTED_TOOL_DESCRIPTION_TERMS = {
    "list_qgis_binaries": (
        "without starting a process",
        "cursor-paginated",
        "binary_id",
        "package-relative path",
        "does not execute a configured probe",
    ),
    "get_qgis_binary_details": (
        "without running it",
        "effective catalog policy",
        "resource limits",
        "does not execute the probe",
    ),
    "start_qgis_binary": (
        "asynchronous QgsTask job",
        "without a command shell",
        "blocks R3 binaries",
        "polling or cancellation",
        "absolute input and output paths",
        "existing absolute working directory",
        "create output parent directories",
        "rejects non-empty stdin",
    ),
    "get_qgis_binary_job": (
        "without waiting for or changing it",
        "lifecycle state",
        "stdout and stderr tails",
    ),
    "list_qgis_binary_jobs": (
        "without changing or waiting for jobs",
        "lifecycle state",
        "get_qgis_binary_job",
    ),
    "cancel_qgis_binary_job": (
        "spawned process tree",
        "complete asynchronously",
        "confirm the terminal state",
    ),
}


class _RecordingBridge:
    calls = []

    def __init__(self, base_url):
        self.base_url = base_url

    def call(self, operation, arguments):
        self.calls.append((operation, arguments))
        return {"operation": operation, "arguments": arguments}


class TestQCopilotsQGISBinaryTools(unittest.TestCase):
    def setUp(self):
        _RecordingBridge.calls = []

    def _tools(self):
        with mock.patch.object(qgis_binary_tools, "BridgeClient", _RecordingBridge):
            return qgis_binary_tools.build_qgis_binary_tools()

    def test_exposes_only_six_generic_tools(self):
        tools = self._tools()

        self.assertEqual([tool.name for tool in tools], EXPECTED_TOOL_NAMES)
        self.assertEqual(len({tool.name for tool in tools}), 6)
        for tool in tools:
            with self.subTest(tool=tool.name):
                self.assertEqual(tool.input_schema["type"], "object")
                self.assertIs(tool.input_schema["additionalProperties"], False)

    def test_tool_descriptions_are_specific_unique_and_serialized(self):
        tools = {tool.name: tool for tool in self._tools()}
        descriptions = []

        self.assertEqual(set(tools), set(EXPECTED_TOOL_DESCRIPTION_TERMS))
        for tool_name, expected_terms in EXPECTED_TOOL_DESCRIPTION_TERMS.items():
            with self.subTest(tool=tool_name):
                tool = tools[tool_name]
                description = tool.description
                self.assertIsInstance(description, str)
                self.assertEqual(description, description.strip())
                self.assertGreaterEqual(len(description), 120)
                for term in expected_terms:
                    self.assertIn(term.casefold(), description.casefold())

                descriptor = tool.descriptor()
                self.assertEqual(descriptor["name"], tool_name)
                self.assertEqual(descriptor["description"], description)
                self.assertEqual(descriptor["inputSchema"], tool.input_schema)
                descriptions.append(description)

        self.assertEqual(len(set(descriptions)), len(descriptions))
        self.assertNotIn(
            "probe metadata",
            tools["list_qgis_binaries"].description.casefold(),
        )

    def test_every_public_input_property_has_a_description(self):
        for tool in self._tools():
            for property_name, property_schema in tool.input_schema[
                "properties"
            ].items():
                with self.subTest(tool=tool.name, property=property_name):
                    description = property_schema.get("description")
                    self.assertIsInstance(description, str)
                    self.assertGreaterEqual(len(description.strip()), 16)

    def test_tool_handlers_use_fixed_bridge_operations_and_start_defaults(self):
        tools = {tool.name: tool for tool in self._tools()}

        tools["list_qgis_binaries"].handler({"cursor": "0", "limit": 10})
        tools["get_qgis_binary_details"].handler({"binary_id": "gdalinfo"})
        tools["start_qgis_binary"].handler({"binary_id": "gdalinfo"})
        tools["get_qgis_binary_job"].handler({"job_id": "job-1"})
        tools["list_qgis_binary_jobs"].handler({"limit": 5})
        tools["cancel_qgis_binary_job"].handler({"job_id": "job-1"})

        self.assertEqual(
            _RecordingBridge.calls,
            [
                ("qgis_binary_list_binaries", {"cursor": "0", "limit": 10}),
                ("qgis_binary_get_binary_details", {"binary_id": "gdalinfo"}),
                (
                    "qgis_binary_start",
                    {
                        "binary_id": "gdalinfo",
                        "arguments": [],
                        "confirmed_risk": False,
                    },
                ),
                ("qgis_binary_get_job", {"job_id": "job-1"}),
                ("qgis_binary_list_jobs", {"limit": 5}),
                ("qgis_binary_cancel_job", {"job_id": "job-1"}),
            ],
        )

    def test_public_input_schemas_match_binary_job_contract(self):
        tools = {tool.name: tool for tool in self._tools()}
        list_properties = tools["list_qgis_binaries"].input_schema["properties"]
        start_schema = tools["start_qgis_binary"].input_schema
        start_properties = start_schema["properties"]
        list_jobs_properties = tools["list_qgis_binary_jobs"].input_schema["properties"]

        self.assertEqual(
            set(list_properties),
            {"cursor", "limit", "query", "group", "enabled", "risk"},
        )
        self.assertEqual(list_properties["limit"]["minimum"], 1)
        self.assertEqual(list_properties["limit"]["maximum"], 200)
        self.assertEqual(list_properties["risk"]["enum"], ["R1", "R2", "R3"])

        self.assertEqual(start_schema["required"], ["binary_id"])
        self.assertEqual(start_properties["arguments"]["default"], [])
        self.assertIs(start_properties["confirmed_risk"]["default"], False)
        self.assertGreater(start_properties["stdin"]["maxLength"], 0)
        self.assertIn(
            "anywhere the QGIS process account can access",
            start_properties["arguments"]["description"],
        )
        self.assertIn(
            "each array item is one argv entry",
            start_properties["arguments"]["description"].casefold(),
        )
        self.assertIn(
            "not from the MCP plugin directory",
            start_properties["arguments"]["description"],
        )
        self.assertIn(
            "service does not create them",
            start_properties["arguments"]["description"],
        )
        self.assertIn(
            "relative paths resolve from that package root",
            start_properties["working_directory"]["description"],
        )
        self.assertIn(
            "prefer an existing absolute directory",
            start_properties["working_directory"]["description"].casefold(),
        )
        self.assertIn(
            "non-empty stdin is rejected before job creation",
            start_properties["stdin"]["description"],
        )
        self.assertEqual(start_properties["client_request_id"]["maxLength"], 128)
        self.assertNotIn("executable", start_properties)
        self.assertNotIn("executable_path", start_properties)
        self.assertNotIn("environment", start_properties)
        self.assertNotIn("environment_variables", start_properties)

        self.assertEqual(
            list_jobs_properties["states"]["items"]["enum"],
            list(qgis_binary_tools.PUBLIC_JOB_STATES),
        )
        self.assertEqual(list_jobs_properties["limit"]["maximum"], 200)

    def test_manifest_is_discoverable_and_in_default_startup_lists(self):
        plugin_dir = PLUGINS_ROOT / "qcopilots_mcp_server_qgis_binary"
        manager_dir = PLUGINS_ROOT / "qcopilots_mcp_servers_manager"
        manifest = json.loads(
            (plugin_dir / "qcopilots_service.json").read_text(encoding="utf-8")
        )
        manager_config = json.loads(
            (manager_dir / "qcopilots_manager_config.json").read_text(encoding="utf-8")
        )
        manager_schema = json.loads(
            (manager_dir / "qcopilots_manager_config.schema.json").read_text(
                encoding="utf-8"
            )
        )

        self.assertEqual(manifest["service_id"], SERVICE_ID)
        self.assertEqual(manifest["default_port"], 48216)
        self.assertEqual(
            manifest["capabilities"],
            ["tools", "qgis-bridge", "qgis-binary"],
        )
        self.assertIn(
            SERVICE_ID,
            manager_config["default_startup"]["service_ids"],
        )
        self.assertIn(
            SERVICE_ID,
            manager_schema["properties"]["default_startup"]["properties"]
            ["service_ids"]["default"],
        )
        self.assertIn(
            SERVICE_ID,
            manager_schema["properties"]["default_startup"]["properties"]
            ["service_ids"]["items"]["examples"],
        )

        for source_name in ("config_store.py", "plugin.py"):
            source_path = manager_dir / source_name
            defaults = _top_level_literal(source_path, "DEFAULT_STARTUP_SERVICE_IDS")
            self.assertIn(SERVICE_ID, defaults, source_name)


def _top_level_literal(path, name):
    module = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    for node in module.body:
        if not isinstance(node, ast.Assign):
            continue
        if any(isinstance(target, ast.Name) and target.id == name for target in node.targets):
            return ast.literal_eval(node.value)
    raise AssertionError(f"Could not find {name} in {path}")


if __name__ == "__main__":
    unittest.main()
