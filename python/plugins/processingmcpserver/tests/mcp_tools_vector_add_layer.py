from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorAddLayerTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "vector_add_layer")

    def test_success_add_geojson_layer(self):
        """验证 add GeoJSON layer 的成功场景。"""
        tools = self.build_tools()
        geojson = self.sample_vector_path()

        result = tools.vector_add_layer(path=str(geojson), provider="ogr")
        self.assertEqual(result["type"], "vector_0")
        self.assertEqual(result["feature_count"], 1)

    def test_failure_invalid_path(self):
        """验证 invalid path 的失败场景。"""
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.vector_add_layer(path="C:/not-exist/sample.geojson", provider="ogr")
        self.assertIn("Layer is not valid", str(ctx.exception))
