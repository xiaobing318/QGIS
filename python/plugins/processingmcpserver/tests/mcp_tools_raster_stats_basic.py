from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsRasterStatsBasicTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the expected capability is registered."""
        assert_tool_registered(self, "raster_stats_basic")

    def test_success_raster_stats_basic(self):
        """Verify the successful path for basic raster statistics."""
        tools = self.build_tools()
        layer = self.add_sample_raster_layer("raster_stats_basic_layer")

        result = tools.raster_stats_basic(layer_ref=layer.id(), band=1)
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "raster_stats_basic")
        self.assertIn("count", result["summary"])

    def test_failure_non_raster_layer(self):
        """Verify the failure path for a non-raster layer."""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("raster_stats_basic_layer2")

        with self.assertRaises(Exception) as ctx:
            tools.raster_stats_basic(layer_ref=layer.id(), band=1)
        self.assertIn("Layer is not a raster layer", str(ctx.exception))
