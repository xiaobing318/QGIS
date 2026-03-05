from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableRenameFieldTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "vector_table_rename_field")

    def test_success_rename_field(self):
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
