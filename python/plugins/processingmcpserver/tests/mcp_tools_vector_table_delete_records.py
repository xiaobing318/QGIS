from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableDeleteRecordsTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "vector_table_delete_records")

    def test_success_delete_records(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("delete_records_vector")

        result = tools.vector_table_delete_records(
            layer_ref=layer.id(),
            where="\"name\" = 'alpha'",
            in_place=True,
            confirm_destructive=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 1)

    def test_failure_without_confirmation(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("delete_records_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_delete_records(
                layer_ref=layer.id(),
                where="",
                in_place=True,
                confirm_destructive=False,
            )
        self.assertIn("confirm_destructive must be true", str(ctx.exception))
