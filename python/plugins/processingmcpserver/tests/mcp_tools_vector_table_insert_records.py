from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableInsertRecordsTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "vector_table_insert_records")

    def test_default_creates_copy_layer_and_reports_unknown_fields(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("insert_records_vector_copy")

        result = tools.vector_table_insert_records(
            layer_ref=layer.id(),
            records=[
                {
                    "name": "delta",
                    "value": 4.5,
                    "category": "new",
                    "ignored_extra": "skip-me",
                    "geometry_wkt": "POINT(108.9300 34.2300)",
                }
            ],
        )

        output_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(output_layer)
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertNotEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertEqual(layer.featureCount(), 3)
        self.assertEqual(output_layer.featureCount(), 4)
        self.assertEqual(result["outputs"]["ignored_fields"], ["ignored_extra"])
        self.assertIn("Ignored unknown fields", result["warnings"][0])

    def test_success_insert_records(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("insert_records_vector")

        result = tools.vector_table_insert_records(
            layer_ref=layer.id(),
            records=[
                {
                    "name": "delta",
                    "value": 4.5,
                    "category": "new",
                    "geometry_wkt": "POINT(108.9300 34.2300)",
                }
            ],
            in_place=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 1)

    def test_failure_empty_records(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("insert_records_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_insert_records(layer_ref=layer.id(), records=[])
        self.assertIn("records is required", str(ctx.exception))
