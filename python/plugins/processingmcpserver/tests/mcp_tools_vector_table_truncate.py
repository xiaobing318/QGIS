from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableTruncateTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_vector_table_truncate")

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
        layer = self.add_sample_vector_layer("truncate_records_vector_copy")

        result = tools.mcp_tools_vector_table_truncate(
            layer_ref=layer.id(),
            confirm_destructive=True,
        )

        output_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(output_layer)
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertNotEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertEqual(layer.featureCount(), 3)
        self.assertEqual(output_layer.featureCount(), 0)

    def test_success_truncate_records(self):
        """
        作用：执行测试用例 `success truncate records`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success truncate records`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("truncate_records_vector")

        result = tools.mcp_tools_vector_table_truncate(
            layer_ref=layer.id(),
            in_place=True,
            confirm_destructive=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 3)
        self.assertEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertEqual(result["outputs"]["feature_count"], 0)

    def test_failure_without_confirmation(self):
        """
        作用：执行测试用例 `failure without confirmation`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure without confirmation`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("truncate_records_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_vector_table_truncate(
                layer_ref=layer.id(),
                in_place=True,
                confirm_destructive=False,
            )
        self.assertIn("confirm_destructive must be true", str(ctx.exception))

