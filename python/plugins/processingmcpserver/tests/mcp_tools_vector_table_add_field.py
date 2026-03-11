from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableAddFieldTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "vector_table_add_field")

    def test_default_creates_copy_layer(self):
        """验证默认条件下的 creates copy layer 场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("add_field_vector_copy")

        result = tools.vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
        )

        output_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(output_layer)
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertNotEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertNotIn("temp_field", self.vector_field_names(layer))
        self.assertIn("temp_field", self.vector_field_names(output_layer))

    def test_success_add_field(self):
        """验证 add field 的成功场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("add_field_vector")

        result = tools.vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
        )
        self.assertTrue(result["ok"])
        self.assertIn("temp_field", result["outputs"]["fields"])

    def test_failure_duplicate_field(self):
        """验证 duplicate field 的失败场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("add_field_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_add_field(
                layer_ref=layer.id(),
                field_name="name",
                field_type="string",
                in_place=True,
            )
        self.assertIn("Field already exists", str(ctx.exception))
