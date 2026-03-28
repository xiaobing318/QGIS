from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerGetDetailsTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the target capability is registered."""
        assert_tool_registered(self, "layer_get_details")

    def test_success_get_details_by_layer_id(self):
        """Verify the successful path for get details by layer ID."""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("details_vector")

        result = tools.layer_get_details(layer_ref=layer.id())
        self.assertEqual(result["name"], "details_vector")
        self.assertIn("fields", result)
        self.assertIn("feature_count", result)

    def test_success_get_details_by_name_and_raster_payload(self):
        """Verify the successful path for get details by name and raster payload."""
        tools = self.build_tools()
        raster_layer = self.add_sample_raster_layer("details_raster")

        result = tools.layer_get_details(layer_ref="details_raster")

        self.assertEqual(result["id"], raster_layer.id())
        self.assertEqual(result["type"], "raster")
        self.assertIn("width", result)
        self.assertIn("height", result)
        self.assertIn("band_count", result)

    def test_failure_missing_layer(self):
        """Verify the failure path for missing layer."""
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.layer_get_details(layer_ref="missing-layer")
        self.assertIn("Layer not found", str(ctx.exception))

    def test_failure_duplicate_layer_name_is_ambiguous(self):
        """Verify the failure path for duplicate layer name is ambiguous."""
        tools = self.build_tools()
        self.add_sample_vector_layer("duplicate-details-layer")
        self.add_sample_vector_layer("duplicate-details-layer")

        with self.assertRaises(Exception) as ctx:
            tools.layer_get_details(layer_ref="duplicate-details-layer")
        self.assertIn("Ambiguous layer reference", str(ctx.exception))
