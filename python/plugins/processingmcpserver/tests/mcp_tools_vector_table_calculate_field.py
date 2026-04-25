from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableCalculateFieldTest(ProcessingMCPTestBase):
    def test_registered(self):
        """
        作用：执行测试用例 `registered`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `registered`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        assert_tool_registered(self, "mcp_tools_vector_table_calculate_field")

    def test_default_creates_copy_layer(self):
        """
        作用：执行测试用例 `default creates copy layer`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `default creates copy layer`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("calc_field_vector_copy")

        result = tools.mcp_tools_vector_table_calculate_field(
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
        """
        作用：执行测试用例 `success calculate field`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success calculate field`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("calc_field_vector")

        result = tools.mcp_tools_vector_table_calculate_field(
            layer_ref=layer.id(),
            field_name="calc_value",
            expression="\"value\" * 2",
            field_type="double",
            in_place=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 3)

    def test_failure_invalid_expression(self):
        """
        作用：执行测试用例 `failure invalid expression`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure invalid expression`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("calc_field_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_vector_table_calculate_field(
                layer_ref=layer.id(),
                field_name="calc_value",
                expression="\"value\" +",
                in_place=True,
            )
        self.assertIn("Invalid expression", str(ctx.exception))

