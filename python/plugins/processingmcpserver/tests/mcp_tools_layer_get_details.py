from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerGetDetailsTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_layer_get_details")

    def test_success_get_details_by_layer_id(self):
        """
        作用：执行测试用例 `success get details by layer id`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success get details by layer id`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("details_vector")

        result = tools.mcp_tools_layer_get_details(layer_ref=layer.id())
        self.assertEqual(result["name"], "details_vector")
        self.assertIn("fields", result)
        self.assertIn("feature_count", result)

    def test_success_get_details_by_name_and_raster_payload(self):
        """
        作用：执行测试用例 `success get details by name and raster payload`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success get details by name and raster payload`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        raster_layer = self.add_sample_raster_layer("details_raster")

        result = tools.mcp_tools_layer_get_details(layer_ref="details_raster")

        self.assertEqual(result["id"], raster_layer.id())
        self.assertEqual(result["type"], "raster")
        self.assertIn("width", result)
        self.assertIn("height", result)
        self.assertIn("band_count", result)

    def test_failure_missing_layer(self):
        """
        作用：执行测试用例 `failure missing layer`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure missing layer`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_layer_get_details(layer_ref="missing-layer")
        self.assertIn("Layer not found", str(ctx.exception))

    def test_failure_duplicate_layer_name_is_ambiguous(self):
        """
        作用：执行测试用例 `failure duplicate layer name is ambiguous`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure duplicate layer name is ambiguous`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        self.add_sample_vector_layer("duplicate-details-layer")
        self.add_sample_vector_layer("duplicate-details-layer")

        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_layer_get_details(layer_ref="duplicate-details-layer")
        self.assertIn("Ambiguous layer reference", str(ctx.exception))

