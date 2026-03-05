from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorGetLayerFeaturesTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "vector_get_layer_features")

    def test_success_limit_and_payload(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("feature_vector")

        result = tools.vector_get_layer_features(layer_ref=layer.id(), limit=2)
        self.assertEqual(result["layer_id"], layer.id())
        self.assertEqual(result["applied_limit"], 2)
        self.assertEqual(len(result["features"]), 2)

    def test_failure_layer_not_found(self):
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.vector_get_layer_features(layer_ref="missing-layer", limit=2)
        self.assertIn("Layer not found", str(ctx.exception))
