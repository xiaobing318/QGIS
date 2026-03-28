from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerGetPanelTreeTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the expected capability is registered."""
        assert_tool_registered(self, "layer_get_panel_tree")

    def test_success_return_tree_and_layers(self):
        """Verify that the panel tree and layers are returned."""
        tools = self.build_tools()
        self.add_sample_vector_layer("panel_tree_vector")

        result = tools.layer_get_panel_tree(include_hidden=True)
        self.assertIn("tree", result)
        self.assertIn("groups", result)
        self.assertIn("layers", result)
        self.assertIn("children", result["tree"])
        self.assertTrue(any(item["name"] == "panel_tree_vector" for item in result["layers"]))

    def test_safety_hidden_filter(self):
        """Verify safety hidden filter."""
        tools = self.build_tools()
        self.add_sample_vector_layer("panel_tree_vector2")

        result = tools.layer_get_panel_tree(include_hidden=False)
        self.assertIn("tree", result)
        self.assertIn("groups", result)
        self.assertIn("layers", result)
