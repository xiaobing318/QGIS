from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsRasterAddLayersTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "raster_add_layers")

    def test_success_with_skip_invalid(self):
        """
        作用：执行测试用例 `success with skip invalid`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success with skip invalid`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        raster_path = self.sample_raster_path()

        result = tools.raster_add_layers(
            paths=[str(raster_path), "C:/not-exist/sample.tif"],
            provider="gdal",
            skip_invalid=True,
        )
        self.assertEqual(result["requested_count"], 2)
        self.assertEqual(result["loaded_count"], 1)
        self.assertEqual(result["failed_count"], 1)

    def test_failure_without_skip_invalid(self):
        """
        作用：执行测试用例 `failure without skip invalid`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure without skip invalid`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        raster_path = self.sample_raster_path()

        with self.assertRaises(Exception) as ctx:
            tools.raster_add_layers(
                paths=[str(raster_path), "C:/not-exist/sample.tif"],
                provider="gdal",
                skip_invalid=False,
            )
        self.assertIn("Failed to load raster dataset", str(ctx.exception))
