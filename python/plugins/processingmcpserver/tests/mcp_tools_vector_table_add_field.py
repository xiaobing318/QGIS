from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableAddFieldTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_vector_table_add_field")

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
        layer = self.add_sample_vector_layer("add_field_vector_copy")

        result = tools.mcp_tools_vector_table_add_field(
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
        """
        作用：执行测试用例 `success add field`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success add field`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("add_field_vector")

        result = tools.mcp_tools_vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
        )
        self.assertTrue(result["ok"])
        self.assertIn("temp_field", result["outputs"]["fields"])

    def test_failure_duplicate_field(self):
        """
        作用：执行测试用例 `failure duplicate field`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure duplicate field`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("add_field_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_vector_table_add_field(
                layer_ref=layer.id(),
                field_name="name",
                field_type="string",
                in_place=True,
            )
        self.assertIn("Field already exists", str(ctx.exception))

