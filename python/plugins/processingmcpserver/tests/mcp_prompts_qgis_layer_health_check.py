from __future__ import annotations

from processingmcpserver.mcp_prompts import register_prompts

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class PromptQgisLayerHealthCheckTest(ProcessingMCPTestBase):
    def test_prompt_registered(self):
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())
        self.assertIn("qgis_layer_health_check", mcp.prompt_names)

    def test_prompt_mentions_expected_tools(self):
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())

        prompt_fn = mcp.prompt_funcs["qgis_layer_health_check"]
        output = prompt_fn(task="检查道路图层")
        self.assertIn("layer_get_panel_tree", output)
        self.assertIn("layer_get_details", output)
        self.assertIn("vector_get_layer_features", output)

    def test_prompt_excludes_removed_tool_names(self):
        removed_names = ["execute_processing(", "execute_code", "render_map"]
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())
        output = mcp.prompt_funcs["qgis_layer_health_check"](task="test")
        for name in removed_names:
            self.assertNotIn(name, output)

