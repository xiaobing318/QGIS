from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableRenameFieldTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_vector_table_rename_field")

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
        layer = self.add_sample_vector_layer("rename_field_vector_copy")
        tools.mcp_tools_vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
            in_place=True,
        )

        result = tools.mcp_tools_vector_table_rename_field(
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
        """
        作用：执行测试用例 `success rename field`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success rename field`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("rename_field_vector")
        add_result = tools.mcp_tools_vector_table_add_field(
            layer_ref=layer.id(),
            field_name="temp_field",
            field_type="string",
        )
        target_layer_id = add_result["summary"]["output_layer_id"]

        result = tools.mcp_tools_vector_table_rename_field(
            layer_ref=target_layer_id,
            field_name="temp_field",
            new_field_name="temp_field2",
            in_place=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 1)

    def test_failure_missing_field(self):
        """
        作用：执行测试用例 `failure missing field`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure missing field`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("rename_field_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_vector_table_rename_field(
                layer_ref=layer.id(),
                field_name="not_exists",
                new_field_name="new_name",
                in_place=True,
            )
        self.assertIn("Field not found", str(ctx.exception))

