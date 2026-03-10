from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableDropFieldsTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "vector_table_drop_fields")

    def test_default_creates_copy_layer(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("drop_field_vector_copy")
        tools.vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
            in_place=True,
        )

        result = tools.vector_table_drop_fields(
            layer_ref=layer.id(),
            fields=["temp_field"],
        )

        output_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(output_layer)
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertNotEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertIn("temp_field", self.vector_field_names(layer))
        self.assertNotIn("temp_field", self.vector_field_names(output_layer))

    def test_success_drop_field(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("drop_field_vector")
        add_result = tools.vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
        )
        target_layer_id = add_result["summary"]["output_layer_id"]

        result = tools.vector_table_drop_fields(
            layer_ref=target_layer_id,
            fields=["temp_field"],
            in_place=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 1)

    def test_failure_empty_fields(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("drop_field_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_drop_fields(layer_ref=layer.id(), fields=[])
        self.assertIn("fields is required", str(ctx.exception))
