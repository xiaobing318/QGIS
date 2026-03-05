from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerListTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "layer_list")

    def test_success_list_vector_and_raster(self):
        tools = self.build_tools()
        vector_layer = self.add_sample_vector_layer("layer_list_vector")
        raster_layer = self.add_sample_raster_layer("layer_list_raster")

        vector_result = tools.layer_list(layer_types="vector")
        raster_result = tools.layer_list(layer_types="raster")
        vector_ids = {item["id"] for item in vector_result}
        raster_ids = {item["id"] for item in raster_result}
        self.assertIn(vector_layer.id(), vector_ids)
        self.assertIn(raster_layer.id(), raster_ids)

    def test_failure_invalid_layer_type_filter(self):
        tools = self.build_tools()
        with self.assertRaises(Exception):
            tools.layer_list(layer_types="invalid")
