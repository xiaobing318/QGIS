from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerGetPanelTreeTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "layer_get_panel_tree")

    def test_success_return_tree_and_layers(self):
        """
        作用：执行测试用例 `success return tree and layers`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success return tree and layers`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        self.add_sample_vector_layer("panel_tree_vector")

        result = tools.layer_get_panel_tree(include_hidden=True)
        self.assertIn("tree", result)
        self.assertIn("groups", result)
        self.assertIn("layers", result)
        self.assertIn("children", result["tree"])
        self.assertTrue(any(item["name"] == "panel_tree_vector" for item in result["layers"]))

    def test_safety_hidden_filter(self):
        """
        作用：执行测试用例 `safety hidden filter`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `safety hidden filter`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        self.add_sample_vector_layer("panel_tree_vector2")

        result = tools.layer_get_panel_tree(include_hidden=False)
        self.assertIn("tree", result)
        self.assertIn("groups", result)
        self.assertIn("layers", result)
