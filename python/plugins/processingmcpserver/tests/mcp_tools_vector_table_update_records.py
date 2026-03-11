from __future__ import annotations

from unittest.mock import MagicMock, patch

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableUpdateRecordsTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "vector_table_update_records")

    def test_default_creates_copy_layer(self):
        """验证默认条件下的 creates copy layer 场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("update_records_vector_copy")

        result = tools.vector_table_update_records(
            layer_ref=layer.id(),
            where="\"name\" = 'alpha'",
            set_literals={"category": "g9"},
            set_expressions={"value": "\"value\" + 10"},
        )

        output_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(output_layer)
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertNotEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertEqual(self.vector_attribute_values(layer, "category"), ["g1", "g1", "g2"])
        self.assertEqual(self.vector_attribute_values(layer, "value"), [1.0, 2.0, 3.0])
        self.assertEqual(
            self.vector_attribute_values(output_layer, "category"),
            ["g9", "g1", "g2"],
        )
        self.assertEqual(
            self.vector_attribute_values(output_layer, "value"),
            [11.0, 2.0, 3.0],
        )

    def test_success_update_records(self):
        """验证 update records 的成功场景。"""
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
        """验证 without updates 的失败场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("update_records_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_update_records(layer_ref=layer.id(), in_place=True)
        self.assertIn("set_literals or set_expressions", str(ctx.exception))

    def test_failure_missing_field(self):
        """验证 missing field 的失败场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("update_records_vector_missing_field")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_update_records(
                layer_ref=layer.id(),
                set_literals={"missing_field": "value"},
                in_place=True,
            )
        self.assertIn("Field not found: missing_field", str(ctx.exception))

    def test_failure_duplicate_layer_name_is_ambiguous(self):
        """验证 duplicate layer name is ambiguous 的失败场景。"""
        tools = self.build_tools()
        self.add_sample_vector_layer("duplicate-update-layer")
        self.add_sample_vector_layer("duplicate-update-layer")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_update_records(
                layer_ref="duplicate-update-layer",
                set_literals={"category": "g9"},
                in_place=True,
            )
        self.assertIn("Ambiguous layer reference", str(ctx.exception))

    @patch("processingmcpserver.mcp_tools.QgsExpression")
    def test_failure_where_eval_error(self, mock_expression_class):
        """验证 where eval error 的失败场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("update_records_vector_eval_error")
        where_expr = MagicMock()
        where_expr.hasParserError.return_value = False
        where_expr.evaluate.return_value = True
        where_expr.hasEvalError.return_value = True
        where_expr.evalErrorString.return_value = "where eval boom"
        mock_expression_class.return_value = where_expr

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_update_records(
                layer_ref=layer.id(),
                where="value > 0",
                set_literals={"category": "g9"},
                in_place=True,
            )
        self.assertIn("where eval boom", str(ctx.exception))
