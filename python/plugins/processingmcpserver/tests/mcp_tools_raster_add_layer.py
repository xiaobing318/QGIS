from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsRasterAddLayerTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "raster_add_layer")

    def test_success_add_raster_layer(self):
        tools = self.build_tools()
        raster_path = self.sample_raster_path()

        result = tools.raster_add_layer(path=str(raster_path), provider="gdal")
        self.assertEqual(result["type"], "raster")
        self.assertGreater(int(result["width"]), 0)
        self.assertGreater(int(result["height"]), 0)

    def test_failure_invalid_path(self):
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.raster_add_layer(path="C:/not-exist/sample.tif", provider="gdal")
        self.assertIn("Layer is not valid", str(ctx.exception))
