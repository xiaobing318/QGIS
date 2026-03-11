from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorStatsBasicTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "vector_stats_basic")

    def test_success_vector_stats_basic(self):
        """验证 vector stats basic 的成功场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("vector_stats_basic_layer")

        result = tools.vector_stats_basic(layer_ref=layer.id(), field_name="value")
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "vector_stats_basic")
        self.assertIn("mean", result["summary"])
        self.assertIn("stdev", result["summary"])
        self.assertAlmostEqual(result["summary"]["stdev"], 1.0, places=6)

    def test_failure_missing_field(self):
        """验证 missing field 的失败场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("vector_stats_basic_layer2")
        with self.assertRaises(Exception) as ctx:
            tools.vector_stats_basic(layer_ref=layer.id(), field_name="not_exists")
        error_message = str(ctx.exception)
        self.assertTrue(
            "not_exists" in error_message or "Invalid field" in error_message,
            msg=error_message,
        )
