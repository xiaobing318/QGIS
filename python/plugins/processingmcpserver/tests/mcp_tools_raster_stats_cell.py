from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsRasterStatsCellTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "raster_stats_cell")

    def test_success_raster_stats_cell(self):
        """验证 raster stats cell 的成功场景。"""
        tools = self.build_tools()
        layer1 = self.add_sample_raster_layer("cell_stats_raster1")
        layer2 = self.add_sample_raster_layer("cell_stats_raster2")

        result = tools.raster_stats_cell(
            raster_layer_refs=[layer1.id(), layer2.id()],
            ignore_nodata=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "raster_stats_cell")
        self.assertEqual(result["summary"]["input_count"], 2)

    def test_failure_empty_refs(self):
        """验证 empty refs 的失败场景。"""
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.raster_stats_cell(raster_layer_refs=[])
        self.assertIn("raster_layer_refs is required", str(ctx.exception))
