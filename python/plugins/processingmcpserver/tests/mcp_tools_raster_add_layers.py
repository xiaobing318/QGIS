from __future__ import annotations

from processingmcpserver.mcp_tools import register_tools

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, assert_tool_registered


class ToolsRasterAddLayersTest(ProcessingMCPTestBase):
    def test_removed_raster_add_layers_not_registered(self):
        mcp = DummyMcp()
        register_tools(mcp, self.build_tools(), enable_execute_code=False)
        self.assertNotIn("raster_add_layers", set(mcp.tool_names))

    def test_dataset_load_layers_registered(self):
        assert_tool_registered(self, "mcp_tools_dataset_load_layers")

    def test_dataset_load_layers_can_load_raster_path(self):
        tools = self.build_tools()
        raster_path = self.sample_raster_path()

        result = tools.mcp_tools_dataset_load_layers(paths=[str(raster_path)], skip_invalid=True)
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["requested_count"], 1)
        self.assertEqual(result["summary"]["loaded_count"], 1)
