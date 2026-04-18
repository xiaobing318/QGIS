from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorStatsBasicTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_vector_stats_basic")

    def test_success_vector_stats_basic(self):
        """
        作用：执行测试用例 `success vector stats basic`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success vector stats basic`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("vector_stats_basic_layer")

        result = tools.mcp_tools_vector_stats_basic(layer_ref=layer.id(), field_name="value")
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "mcp_tools_vector_stats_basic")
        self.assertIn("mean", result["summary"])
        self.assertIn("stdev", result["summary"])
        self.assertAlmostEqual(result["summary"]["stdev"], 1.0, places=6)

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
        layer = self.add_sample_vector_layer("vector_stats_basic_layer2")
        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_vector_stats_basic(layer_ref=layer.id(), field_name="not_exists")
        error_message = str(ctx.exception)
        self.assertTrue(
            "not_exists" in error_message or "Invalid field" in error_message,
            msg=error_message,
        )

