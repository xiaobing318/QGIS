from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorStatsByCategoriesTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "vector_stats_by_categories")

    def test_success_vector_stats_by_categories(self):
        """验证 vector stats by categories 的成功场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("vector_stats_cat_layer")

        result = tools.vector_stats_by_categories(
            layer_ref=layer.id(),
            category_fields=["category"],
            values_field="value",
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "vector_stats_by_categories")

    def test_failure_empty_categories(self):
        """验证 empty categories 的失败场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("vector_stats_cat_layer2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_stats_by_categories(layer_ref=layer.id(), category_fields=[])
        self.assertIn("category_fields is required", str(ctx.exception))
