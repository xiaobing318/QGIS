from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsRasterStatsCellTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "raster_stats_cell")

    def test_success_raster_stats_cell(self):
        """
        作用：执行测试用例 `success raster stats cell`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success raster stats cell`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer1 = self.add_sample_raster_layer("cell_stats_raster1")
        layer2 = self.add_sample_raster_layer("cell_stats_raster2")

        result = tools.raster_stats_cell(
            raster_layer_refs=[layer1.id(), layer2.id()],
            ignore_nodata=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "raster_stats_cell")
        self.assertEqual(result["summary"]["input_count"], 2)

    def test_failure_empty_refs(self):
        """
        作用：执行测试用例 `failure empty refs`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure empty refs`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.raster_stats_cell(raster_layer_refs=[])
        self.assertIn("raster_layer_refs is required", str(ctx.exception))
