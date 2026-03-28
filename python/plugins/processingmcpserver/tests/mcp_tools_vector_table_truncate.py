from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableTruncateTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the expected capability is registered."""
        assert_tool_registered(self, "vector_table_truncate")

    def test_default_creates_copy_layer(self):
        """Verify that the default behavior creates a copy layer."""
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
        """Verify the successful path for truncating records."""
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
        """Verify that confirmation is required."""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("truncate_records_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_truncate(
                layer_ref=layer.id(),
                in_place=True,
                confirm_destructive=False,
            )
        self.assertIn("confirm_destructive must be true", str(ctx.exception))
