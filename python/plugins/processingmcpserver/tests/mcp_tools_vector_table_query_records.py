from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableQueryRecordsTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "vector_table_query_records")

    def test_success_query_records(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("query_records_vector")

        result = tools.vector_table_query_records(
            layer_ref=layer.id(),
            where="\"name\" = 'alpha'",
            fields=["name", "value"],
            limit=1,
            order_by="name",
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["returned"], 1)
        self.assertEqual(result["outputs"]["records"][0]["attributes"]["name"], "alpha")

    def test_failure_invalid_where(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("query_records_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_query_records(layer_ref=layer.id(), where="\"name\" = ")
        self.assertIn("Invalid where expression", str(ctx.exception))
