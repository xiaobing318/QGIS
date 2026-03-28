from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsRasterAddLayersTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the target capability is registered."""
        assert_tool_registered(self, "raster_add_layers")

    def test_success_with_skip_invalid(self):
        """Verify the successful path for with skip invalid."""
        tools = self.build_tools()
        raster_path = self.sample_raster_path()

        result = tools.raster_add_layers(
            paths=[str(raster_path), "C:/not-exist/sample.tif"],
            provider="gdal",
            skip_invalid=True,
        )
        self.assertEqual(result["requested_count"], 2)
        self.assertEqual(result["loaded_count"], 1)
        self.assertEqual(result["failed_count"], 1)

    def test_failure_without_skip_invalid(self):
        """Verify the failure path for without skip invalid."""
        tools = self.build_tools()
        raster_path = self.sample_raster_path()

        with self.assertRaises(Exception) as ctx:
            tools.raster_add_layers(
                paths=[str(raster_path), "C:/not-exist/sample.tif"],
                provider="gdal",
                skip_invalid=False,
            )
        self.assertIn("Failed to load raster dataset", str(ctx.exception))
