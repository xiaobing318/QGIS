from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableRenameFieldTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the expected capability is registered."""
        assert_tool_registered(self, "vector_table_rename_field")

    def test_default_creates_copy_layer(self):
        """Verify that the default behavior creates a copy layer."""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("rename_field_vector_copy")
        tools.vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
            in_place=True,
        )

        result = tools.vector_table_rename_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            new_field_name="temp_field2",
        )

        output_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(output_layer)
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertNotEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertIn("temp_field", self.vector_field_names(layer))
        self.assertNotIn("temp_field2", self.vector_field_names(layer))
        self.assertNotIn("temp_field", self.vector_field_names(output_layer))
        self.assertIn("temp_field2", self.vector_field_names(output_layer))

    def test_success_rename_field(self):
        """Verify the successful path for renaming a field."""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("rename_field_vector")
        add_result = tools.vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
        )
        target_layer_id = add_result["summary"]["output_layer_id"]

        result = tools.vector_table_rename_field(
            layer_ref=target_layer_id,
            field_name="temp_field",
            new_field_name="temp_field2",
            in_place=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 1)

    def test_failure_missing_field(self):
        """Verify the failure path for a missing field."""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("rename_field_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_rename_field(
                layer_ref=layer.id(),
                field_name="not_exists",
                new_field_name="new_name",
                in_place=True,
            )
        self.assertIn("Field not found", str(ctx.exception))
