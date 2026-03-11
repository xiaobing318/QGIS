from __future__ import annotations

from qgis.core import QgsProject

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerRemoveTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "layer_remove")

    def test_success_remove_existing_layer(self):
        """验证 remove existing layer 的成功场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("remove_vector")
        layer_id = layer.id()

        result = tools.layer_remove(layer_id=layer_id)
        self.assertEqual(result["removed"], layer_id)
        self.assertNotIn(layer_id, QgsProject.instance().mapLayers())

    def test_failure_missing_layer(self):
        """验证 missing layer 的失败场景。"""
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.layer_remove(layer_id="missing-layer")
        self.assertIn("Layer not found", str(ctx.exception))
