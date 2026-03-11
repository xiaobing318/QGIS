from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableTruncateTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "vector_table_truncate")

    def test_default_creates_copy_layer(self):
        """验证默认条件下的 creates copy layer 场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("truncate_records_vector_copy")

        result = tools.vector_table_truncate(
            layer_ref=layer.id(),
            confirm_destructive=True,
        )

        output_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(output_layer)
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertNotEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertEqual(layer.featureCount(), 3)
        self.assertEqual(output_layer.featureCount(), 0)

    def test_success_truncate_records(self):
        """验证 truncate records 的成功场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("truncate_records_vector")

        result = tools.vector_table_truncate(
            layer_ref=layer.id(),
            in_place=True,
            confirm_destructive=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 3)
        self.assertEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertEqual(result["outputs"]["feature_count"], 0)

    def test_failure_without_confirmation(self):
        """验证 without confirmation 的失败场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("truncate_records_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_truncate(
                layer_ref=layer.id(),
                in_place=True,
                confirm_destructive=False,
            )
        self.assertIn("confirm_destructive must be true", str(ctx.exception))
