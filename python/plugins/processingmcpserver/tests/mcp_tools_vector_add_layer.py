from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorAddLayerTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "vector_add_layer")

    def test_success_add_geojson_layer(self):
        """
        作用：执行测试用例 `success add geojson layer`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success add geojson layer`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        geojson = self.sample_vector_path()

        result = tools.vector_add_layer(path=str(geojson), provider="ogr")
        self.assertEqual(result["type"], "vector_0")
        self.assertEqual(result["feature_count"], 1)

    def test_failure_invalid_path(self):
        """
        作用：执行测试用例 `failure invalid path`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure invalid path`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.vector_add_layer(path="C:/not-exist/sample.geojson", provider="ogr")
        self.assertIn("Layer is not valid", str(ctx.exception))
