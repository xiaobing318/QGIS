from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProcessingGetAlgorithmsTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_processing_get_algorithms")

    def test_success_get_algorithms_limited(self):
        """
        作用：执行测试用例 `success get algorithms limited`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success get algorithms limited`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()

        result = tools.mcp_tools_processing_get_algorithms(limit=5)
        self.assertIn("count", result)
        self.assertIn("returned", result)
        self.assertLessEqual(int(result["returned"]), 5)

    def test_success_filter_by_provider_and_return_detail_payload(self):
        """
        作用：执行测试用例 `success filter by provider and return detail payload`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success filter by provider and return detail payload`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()

        filtered = tools.mcp_tools_processing_get_algorithms(provider_id="native", limit=5)
        detailed = tools.mcp_tools_processing_get_algorithms(algorithm_id="native:buffer")

        self.assertGreater(filtered["returned"], 0)
        self.assertTrue(
            all(item["provider_id"] == "native" for item in filtered["algorithms"])
        )
        self.assertEqual(detailed["algorithm"]["id"], "native:buffer")
        self.assertIn("parameters", detailed["algorithm"])
        self.assertIn("outputs", detailed["algorithm"])

    def test_success_limit_cap_is_reported(self):
        """
        作用：执行测试用例 `success limit cap is reported`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success limit cap is reported`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()

        result = tools.mcp_tools_processing_get_algorithms(
            limit=tools.MAX_ALGORITHM_LIST_LIMIT + 1
        )

        self.assertEqual(result["applied_limit"], tools.MAX_ALGORITHM_LIST_LIMIT)
        self.assertTrue(result["limit_capped"])

    def test_failure_unknown_algorithm_id(self):
        """
        作用：执行测试用例 `failure unknown algorithm id`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure unknown algorithm id`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_processing_get_algorithms(algorithm_id="not-exist:algorithm")
        self.assertIn("Algorithm not found", str(ctx.exception))

