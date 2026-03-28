from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsCommonGetQgisInfoTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "common_get_qgis_info")

    def test_success_returns_runtime_snapshot(self):
        """
        作用：执行测试用例 `success returns runtime snapshot`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success returns runtime snapshot`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        result = tools.common_get_qgis_info()

        self.assertIn("qgis", result)
        self.assertIn("python", result)
        self.assertIn("project", result)
        self.assertIn("processing_mcp", result)

    def test_success_contains_expected_sections(self):
        """
        作用：执行测试用例 `success contains expected sections`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success contains expected sections`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        result = tools.common_get_qgis_info()
        self.assertIn("version", result["qgis"])
        self.assertIn("platform", result)
        self.assertIn("active_plugins", result)
        self.assertIn("layer_count", result["project"])
        self.assertIn("available", result["processing_mcp"])
        self.assertIn("filesystem", result["processing_mcp"])
        self.assertIn("write_policy", result["processing_mcp"]["filesystem"])
        self.assertTrue(
            result["processing_mcp"]["filesystem"]["write_policy"][
                "require_confirm_write"
            ]
        )
