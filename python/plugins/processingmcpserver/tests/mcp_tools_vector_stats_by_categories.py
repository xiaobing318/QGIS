from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorStatsByCategoriesTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "vector_stats_by_categories")

    def test_success_vector_stats_by_categories(self):
        """
        作用：执行测试用例 `success vector stats by categories`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success vector stats by categories`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("vector_stats_cat_layer")

        result = tools.vector_stats_by_categories(
            layer_ref=layer.id(),
            category_fields=["category"],
            values_field="value",
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "vector_stats_by_categories")

    def test_failure_empty_categories(self):
        """
        作用：执行测试用例 `failure empty categories`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure empty categories`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("vector_stats_cat_layer2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_stats_by_categories(layer_ref=layer.id(), category_fields=[])
        self.assertIn("category_fields is required", str(ctx.exception))
