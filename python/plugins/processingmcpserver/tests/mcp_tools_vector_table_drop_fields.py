from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableDropFieldsTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_vector_table_drop_fields")

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
        layer = self.add_sample_vector_layer("drop_field_vector_copy")
        tools.mcp_tools_vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
            in_place=True,
        )

        result = tools.mcp_tools_vector_table_drop_fields(
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
        """
        作用：执行测试用例 `success drop field`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success drop field`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("drop_field_vector")
        add_result = tools.mcp_tools_vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
        )
        target_layer_id = add_result["summary"]["output_layer_id"]

        result = tools.mcp_tools_vector_table_drop_fields(
            layer_ref=target_layer_id,
            fields=["temp_field"],
            in_place=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 1)

    def test_failure_empty_fields(self):
        """
        作用：执行测试用例 `failure empty fields`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure empty fields`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("drop_field_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_vector_table_drop_fields(layer_ref=layer.id(), fields=[])
        self.assertIn("fields is required", str(ctx.exception))

