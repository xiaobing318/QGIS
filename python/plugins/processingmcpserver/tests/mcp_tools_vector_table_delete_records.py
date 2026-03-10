from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableDeleteRecordsTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "vector_table_delete_records")

    def test_default_creates_copy_layer(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("delete_records_vector_copy")

        result = tools.vector_table_delete_records(
            layer_ref=layer.id(),
            where="\"name\" = 'alpha'",
            confirm_destructive=True,
        )

        output_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(output_layer)
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertNotEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertEqual(layer.featureCount(), 3)
        self.assertEqual(output_layer.featureCount(), 2)

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

    def test_success_delete_records_without_where_deletes_all(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("delete_records_vector_all")

        result = tools.vector_table_delete_records(
            layer_ref=layer.id(),
            where="",
            in_place=True,
            confirm_destructive=True,
        )

        self.assertEqual(result["summary"]["affected_count"], 3)
        self.assertEqual(result["outputs"]["feature_count"], 0)
        self.assertEqual(result["summary"]["output_layer_id"], layer.id())

    def test_failure_invalid_where(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("delete_records_vector_invalid_where")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_delete_records(
                layer_ref=layer.id(),
                where="\"name\" = ",
                in_place=True,
                confirm_destructive=True,
            )
        self.assertIn("Invalid where expression", str(ctx.exception))

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

    def test_failure_duplicate_layer_name_is_ambiguous(self):
        tools = self.build_tools()
        self.add_sample_vector_layer("duplicate-delete-layer")
        self.add_sample_vector_layer("duplicate-delete-layer")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_delete_records(
                layer_ref="duplicate-delete-layer",
                where="",
                in_place=True,
                confirm_destructive=True,
            )
        self.assertIn("Ambiguous layer reference", str(ctx.exception))
