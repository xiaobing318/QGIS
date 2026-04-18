from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProcessingGetParameterTemplateTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_processing_get_parameter_template")

    def test_success_get_parameter_template(self):
        """
        作用：执行测试用例 `success get parameter template`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success get parameter template`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()

        result = tools.mcp_tools_processing_get_parameter_template("native:buffer")
        self.assertIn("algorithm", result)
        self.assertIn("required_parameters", result)
        self.assertIn("optional_parameters", result)
        self.assertIn("outputs", result)

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
            tools.mcp_tools_processing_get_parameter_template("not-exist:algorithm")
        self.assertIn("Algorithm not found", str(ctx.exception))

