from __future__ import annotations

import json

from processingmcpserver.mcp_prompts import REGISTERED_PROMPT_NAMES, register_prompts
from processingmcpserver.mcp_resources import (
    REGISTERED_RESOURCE_URIS,
    register_resources,
)
from processingmcpserver.mcp_tools import REGISTERED_TOOL_NAMES, register_tools

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class RegistrationsIntegrityTest(ProcessingMCPTestBase):
    def test_register_tools_matches_expected_set(self):
        removed_render_tool = "render" + "_map"
        legacy_removed = {
            "ping",
            "load_project",
            "create_new_project",
            "get_project_info",
            "zoom_to_layer",
            "save_project",
            "execute_processing",
            "execute_code",
            removed_render_tool,
        }

        mcp = DummyMcp()
        register_tools(mcp, DummyTools(), enable_execute_code=False)
        self.assertEqual(set(mcp.tool_names), set(REGISTERED_TOOL_NAMES))
        self.assertTrue(legacy_removed.isdisjoint(set(mcp.tool_names)))

    def test_register_prompts_and_resources_discoverable(self):
        mcp = DummyMcp()
        tools = DummyTools()
        register_prompts(mcp, tools)
        register_resources(mcp, tools)

        self.assertEqual(set(mcp.prompt_names), set(REGISTERED_PROMPT_NAMES))
        self.assertEqual(set(mcp.resource_uris), set(REGISTERED_RESOURCE_URIS))

    def test_prompt_templates_exclude_removed_tool_names(self):
        removed_render_tool = "render" + "_map"
        legacy_removed = [
            "ping",
            "load_project",
            "create_new_project",
            "get_project_info",
            "save_project",
            "zoom_to_layer",
            "execute_processing(",
            "execute_code",
            removed_render_tool,
        ]
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())
        for prompt_name, prompt_fn in mcp.prompt_funcs.items():
            output = prompt_fn(task="test")
            for removed in legacy_removed:
                self.assertNotIn(
                    removed,
                    output,
                    msg=f"{prompt_name} still contains removed tool name: {removed}",
                )

    def test_resource_envelope_for_kept_resources(self):
        mcp = DummyMcp()
        tools = DummyTools()
        register_resources(mcp, tools)

        project_info_payload = json.loads(mcp.resource_funcs["qgis://project/info"]())
        self.assertTrue(project_info_payload["ok"])
        self.assertIn("generated_at", project_info_payload)
        self.assertIn("project_id", project_info_payload)
        self.assertEqual(project_info_payload["schema_version"], "1.0.0")
        self.assertIn("data", project_info_payload)

        layers_summary_payload = json.loads(
            mcp.resource_funcs["qgis://project/layers/summary"]()
        )
        self.assertTrue(layers_summary_payload["ok"])
        self.assertIn("generated_at", layers_summary_payload)
        self.assertIn("project_id", layers_summary_payload)
        self.assertEqual(layers_summary_payload["schema_version"], "1.0.0")
        self.assertIn("data", layers_summary_payload)

