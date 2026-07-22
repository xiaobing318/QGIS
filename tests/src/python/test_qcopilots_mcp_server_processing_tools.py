"""QGIS unit tests for QCopilots Processing MCP tools.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-12"
__copyright__ = "Copyright 2026, The QGIS Project"

import unittest


class FakeProcessingAlgorithm:
    def __init__(self, algorithm_id, display_name, group_id, tags):
        self._id = algorithm_id
        self._display_name = display_name
        self._group_id = group_id
        self._tags = tags

    def id(self):
        return self._id

    def displayName(self):
        return self._display_name

    def groupId(self):
        return self._group_id

    def tags(self):
        return self._tags

    def shortHelpString(self):
        return self._display_name + " help"

    def parameterDefinitions(self):
        return []

    def outputDefinitions(self):
        return []


class FakeProcessingBridge:
    def __init__(self):
        self.calls = []

    def run_algorithm(self, algorithm_id, parameters, context=None, feedback=None):
        self.calls.append((algorithm_id, parameters, context, feedback))
        return {
            "algorithm_id": algorithm_id,
            "parameters": parameters,
            "outputs": {"OUTPUT": "memory:"},
        }


class TestQCopilotsMcpServerProcessingTools(unittest.TestCase):
    def test_processing_algorithms_are_filtered_by_category(self):
        from qcopilots_common.processing_tools import (
            classify_processing_algorithm,
            filter_processing_algorithms,
        )

        vector = FakeProcessingAlgorithm(
            "native:buffer",
            "Buffer",
            "vectorgeometry",
            ["vector", "geometry"],
        )
        raster = FakeProcessingAlgorithm(
            "gdal:warpreproject",
            "Warp",
            "rasterprojections",
            ["raster", "projection"],
        )
        other = FakeProcessingAlgorithm(
            "native:setprojectvariable",
            "Set project variable",
            "modelertools",
            ["project"],
        )

        self.assertEqual(classify_processing_algorithm(vector), "vector")
        self.assertEqual(classify_processing_algorithm(raster), "raster")
        self.assertIsNone(classify_processing_algorithm(other))
        self.assertEqual(
            [
                algorithm.id()
                for algorithm in filter_processing_algorithms(
                    [raster, other, vector],
                    "vector",
                )
            ],
            ["native:buffer"],
        )
        self.assertEqual(
            [
                algorithm.id()
                for algorithm in filter_processing_algorithms(
                    [raster, other, vector],
                    "raster",
                )
            ],
            ["gdal:warpreproject"],
        )

    def test_processing_tool_registry_bridges_calls_to_processing_runner(self):
        from qcopilots_common.processing_tools import ProcessingToolRegistry

        vector = FakeProcessingAlgorithm(
            "native:buffer",
            "Buffer",
            "vectorgeometry",
            ["vector", "geometry"],
        )
        raster = FakeProcessingAlgorithm(
            "gdal:warpreproject",
            "Warp",
            "rasterprojections",
            ["raster", "projection"],
        )
        bridge = FakeProcessingBridge()
        registry = ProcessingToolRegistry(
            category="vector",
            algorithms=[raster, vector],
            bridge=bridge,
        )

        tools = registry.list_tools()
        self.assertEqual([tool.name for tool in tools], ["processing_native_buffer"])
        self.assertEqual(tools[0].description, "Buffer help")
        self.assertEqual(tools[0].algorithm_id, "native:buffer")

        result = registry.call_tool(
            "processing_native_buffer",
            {"INPUT": "memory:", "DISTANCE": 10, "OUTPUT": "memory:"},
        )
        self.assertEqual(
            result,
            {
                "algorithm_id": "native:buffer",
                "parameters": {
                    "INPUT": "memory:",
                    "DISTANCE": 10,
                    "OUTPUT": "memory:",
                },
                "outputs": {"OUTPUT": "memory:"},
            },
        )
        self.assertEqual(
            [(algorithm_id, parameters) for algorithm_id, parameters, _, _ in bridge.calls],
            [
                (
                    "native:buffer",
                    {"INPUT": "memory:", "DISTANCE": 10, "OUTPUT": "memory:"},
                )
            ],
        )

        with self.assertRaises(KeyError):
            registry.call_tool("processing_gdal_warpreproject", {})

    def test_build_processing_tools_uses_service_bridge_payloads(self):
        import qcopilots_common.processing_tools as processing_tools

        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return {"tool": tool, "arguments": arguments}

        original_bridge_client = processing_tools.BridgeClient
        processing_tools.BridgeClient = FakeBridgeClient
        try:
            tools = processing_tools.build_processing_tools("vector")
            self.assertEqual(
                [tool.name for tool in tools],
                [
                    "list_vector_processing_algorithms",
                    "get_vector_processing_algorithm_details",
                    "run_vector_processing_algorithm",
                ],
            )
            raster_tools = processing_tools.build_processing_tools("raster")
            self.assertEqual(
                [tool.name for tool in raster_tools],
                [
                    "list_raster_processing_algorithms",
                    "get_raster_processing_algorithm_details",
                    "run_raster_processing_algorithm",
                ],
            )
            self.assertTrue(all("requires_auth" not in tool.descriptor() for tool in tools + raster_tools))
            tools[0].handler({})
            tools[1].handler({"algorithm_id": "native:buffer"})
            tools[2].handler({"algorithm_id": "native:buffer", "parameters": {"OUTPUT": "memory:"}})
            raster_tools[0].handler({})
            raster_tools[1].handler({"algorithm_id": "gdal:warpreproject"})
            raster_tools[2].handler(
                {"algorithm_id": "gdal:warpreproject", "parameters": {"OUTPUT": "memory:"}}
            )
        finally:
            processing_tools.BridgeClient = original_bridge_client

        self.assertEqual(
            calls,
            [
                ("processing_list_algorithms", {"category": "vector"}),
                (
                    "processing_algorithm_details",
                    {"algorithm_id": "native:buffer", "category": "vector"},
                ),
                (
                    "processing_run_algorithm",
                    {
                        "algorithm_id": "native:buffer",
                        "parameters": {"OUTPUT": "memory:"},
                        "category": "vector",
                    },
                ),
                ("processing_list_algorithms", {"category": "raster"}),
                (
                    "processing_algorithm_details",
                    {"algorithm_id": "gdal:warpreproject", "category": "raster"},
                ),
                (
                    "processing_run_algorithm",
                    {
                        "algorithm_id": "gdal:warpreproject",
                        "parameters": {"OUTPUT": "memory:"},
                        "category": "raster",
                    },
                ),
            ],
        )

    def test_build_interactive_tools_uses_service_bridge_payloads(self):
        import qcopilots_common.interactive_layer_tools as interactive_layer_tools
        import qcopilots_common.processing_tools as processing_tools

        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return {"tool": tool, "arguments": arguments}

        original_bridge_client = processing_tools.BridgeClient
        original_interactive_layer_bridge_client = interactive_layer_tools.BridgeClient
        processing_tools.BridgeClient = FakeBridgeClient
        interactive_layer_tools.BridgeClient = FakeBridgeClient
        try:
            tools = processing_tools.build_interactive_tools()
            self.assertEqual(
                [tool.name for tool in tools],
                [
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
                    "create_vector_layer",
                    "add_vector_features",
                    "update_vector_features",
                ],
            )
            self.assertTrue(all("requires_auth" not in tool.descriptor() for tool in tools))
            tools_by_name = {tool.name: tool for tool in tools}
            tools_by_name["list_layers"].handler({})
            tools_by_name["set_layer_visibility"].handler({"layer_id": "abc", "visible": True})
            tools_by_name["export_map_image"].handler({"path": "map.png"})
            tools_by_name["describe_layer_sources"].handler({})
            tools_by_name["create_vector_layer"].handler({"name": "scratch", "path": "memory:"})
        finally:
            processing_tools.BridgeClient = original_bridge_client
            interactive_layer_tools.BridgeClient = original_interactive_layer_bridge_client

        self.assertEqual(
            calls,
            [
                ("list_layers", {}),
                ("set_layer_visibility", {"layer_id": "abc", "visible": True}),
                ("export_map_image", {"path": "map.png"}),
                ("interactive_layer_describe_sources", {}),
                ("create_vector_layer", {"name": "scratch", "path": "memory:"}),
            ],
        )

    def test_service_main_wiring_uses_expected_tools_and_ports(self):
        import os

        from qcopilots_common.constants import DEFAULT_SERVICE_PORTS

        modules = [
            (
                "qcopilots_mcp_server_builtin_tools.server",
                "qcopilots.mcp_server_builtin_tools",
                "QCopilots MCP Server Builtin Tools",
                "read_file",
            ),
            (
                "qcopilots_mcp_server_skills.server",
                "qcopilots.mcp_server_skills",
                "QCopilots MCP Server Skills",
                "list_skills",
            ),
            (
                "qcopilots_mcp_server_interactive_tools.server",
                "qcopilots.mcp_server_interactive_tools",
                "QCopilots MCP Server Interactive Tools",
                [
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
                    "create_vector_layer",
                    "add_vector_features",
                    "update_vector_features",
                ],
            ),
            (
                "qcopilots_mcp_server_processing_vector.server",
                "qcopilots.mcp_server_processing_vector",
                "QCopilots MCP Server Processing Vector",
                [
                    "list_vector_processing_algorithms",
                    "get_vector_processing_algorithm_details",
                    "run_vector_processing_algorithm",
                ],
            ),
            (
                "qcopilots_mcp_server_processing_raster.server",
                "qcopilots.mcp_server_processing_raster",
                "QCopilots MCP Server Processing Raster",
                [
                    "list_raster_processing_algorithms",
                    "get_raster_processing_algorithm_details",
                    "run_raster_processing_algorithm",
                ],
            ),
        ]
        old_codex_home = os.environ.get("CODEX_HOME")
        captures = []

        def fake_run_mcp_server(
            name,
            version,
            tools,
            default_port,
            description,
            logger,
            resources=None,
            prompts=None,
        ):
            resolved_tools = tools() if callable(tools) else tools
            resolved_resources = resources() if callable(resources) else resources
            resolved_prompts = prompts() if callable(prompts) else prompts
            captures.append(
                {
                    "name": name,
                    "version": version,
                    "tool_names": [tool.name for tool in resolved_tools],
                    "default_port": default_port,
                    "description": description,
                    "logger": logger.name,
                    "resource_count": len(resolved_resources or []),
                    "prompt_count": len(resolved_prompts or []),
                }
            )

        try:
            os.environ["CODEX_HOME"] = ""
            for module_name, service_id, expected_name, expected_tools in modules:
                module = __import__(module_name, fromlist=["main"])
                original_runner = module.run_mcp_server
                module.run_mcp_server = fake_run_mcp_server
                try:
                    module.main()
                finally:
                    module.run_mcp_server = original_runner

                capture = captures[-1]
                self.assertEqual(capture["name"], expected_name)
                self.assertEqual(capture["version"], "0.1.0")
                if isinstance(expected_tools, list):
                    self.assertEqual(capture["tool_names"], expected_tools)
                else:
                    self.assertIn(expected_tools, capture["tool_names"])
                self.assertNotIn("add_vector_layer", capture["tool_names"])
                self.assertNotIn("add_raster_layer", capture["tool_names"])
                self.assertNotIn("remove_layer", capture["tool_names"])
                self.assertEqual(
                    capture["default_port"],
                    DEFAULT_SERVICE_PORTS[service_id],
                )
                self.assertEqual(capture["logger"], service_id)
        finally:
            if old_codex_home is None:
                os.environ.pop("CODEX_HOME", None)
            else:
                os.environ["CODEX_HOME"] = old_codex_home


if __name__ == "__main__":
    unittest.main()
