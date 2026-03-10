from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableCalculateFieldTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "vector_table_calculate_field")

    def test_default_creates_copy_layer(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("calc_field_vector_copy")

        result = tools.vector_table_calculate_field(
            layer_ref=layer.id(),
            field_name="calc_value",
            expression="\"value\" * 2",
            field_type="double",
        )

        output_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(output_layer)
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertNotEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertNotIn("calc_value", self.vector_field_names(layer))
        self.assertIn("calc_value", self.vector_field_names(output_layer))
        self.assertEqual(self.vector_attribute_values(layer, "value"), [1.0, 2.0, 3.0])
        self.assertEqual(
            self.vector_attribute_values(output_layer, "calc_value"),
            [2.0, 4.0, 6.0],
        )

    def test_success_calculate_field(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("calc_field_vector")

        result = tools.vector_table_calculate_field(
            layer_ref=layer.id(),
            field_name="calc_value",
            expression="\"value\" * 2",
            field_type="double",
            in_place=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 3)

    def test_failure_invalid_expression(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("calc_field_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_calculate_field(
                layer_ref=layer.id(),
                field_name="calc_value",
                expression="\"value\" +",
                in_place=True,
            )
        self.assertIn("Invalid expression", str(ctx.exception))
