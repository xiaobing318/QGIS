from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorStatsBasicTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the target capability is registered."""
        assert_tool_registered(self, "vector_stats_basic")

    def test_success_vector_stats_basic(self):
        """Verify the successful path for vector stats basic."""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("vector_stats_basic_layer")

        result = tools.vector_stats_basic(layer_ref=layer.id(), field_name="value")
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "vector_stats_basic")
        self.assertIn("mean", result["summary"])
        self.assertIn("stdev", result["summary"])
        self.assertAlmostEqual(result["summary"]["stdev"], 1.0, places=6)

    def test_failure_missing_field(self):
        """Verify the failure path for missing field."""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("vector_stats_basic_layer2")
        with self.assertRaises(Exception) as ctx:
            tools.vector_stats_basic(layer_ref=layer.id(), field_name="not_exists")
        error_message = str(ctx.exception)
        self.assertTrue(
            "not_exists" in error_message or "Invalid field" in error_message,
            msg=error_message,
        )
