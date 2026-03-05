from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerGetDetailsTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "layer_get_details")

    def test_success_get_details_by_layer_id(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("details_vector")

        result = tools.layer_get_details(layer_ref=layer.id())
        self.assertEqual(result["name"], "details_vector")
        self.assertIn("fields", result)
        self.assertIn("feature_count", result)

    def test_failure_missing_layer(self):
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.layer_get_details(layer_ref="missing-layer")
        self.assertIn("Layer not found", str(ctx.exception))
