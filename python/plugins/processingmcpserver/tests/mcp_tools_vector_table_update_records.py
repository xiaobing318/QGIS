from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableUpdateRecordsTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "vector_table_update_records")

    def test_success_update_records(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("update_records_vector")

        result = tools.vector_table_update_records(
            layer_ref=layer.id(),
            where="\"name\" = 'alpha'",
            set_literals={"category": "g9"},
            set_expressions={"value": "\"value\" + 10"},
            in_place=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 1)

    def test_failure_without_updates(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("update_records_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_update_records(layer_ref=layer.id(), in_place=True)
        self.assertIn("set_literals or set_expressions", str(ctx.exception))
