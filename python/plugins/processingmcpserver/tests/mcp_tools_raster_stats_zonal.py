from __future__ import annotations

from unittest.mock import patch

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class _FakeProcessingRegistry:
    def __init__(self, available_algorithms: list[str]):
        """
        作用：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `available_algorithms`（`list[str]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self._available_algorithms = set(available_algorithms)

    def providers(self):
        """
        作用：实现 `providers` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `providers` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return []

    def algorithmById(self, algorithm_id: str):
        """
        作用：实现 `algorithmById` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `algorithmById` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `algorithm_id`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        if algorithm_id in self._available_algorithms:
            return object()
        return None


class ToolsRasterStatsZonalTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "raster_stats_zonal")

    def test_success_raster_stats_zonal(self):
        """
        作用：执行测试用例 `success raster stats zonal`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success raster stats zonal`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon")
        raster_layer = self.add_sample_raster_layer("zonal_raster")
        original_fields = self.vector_field_names(vector_layer)

        result = tools.raster_stats_zonal(
            vector_layer_ref=vector_layer.id(),
            raster_layer_ref=raster_layer.id(),
            raster_band=1,
            in_place=False,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "raster_stats_zonal")
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertIn("output_layer_id", result["summary"])
        self.assertNotEqual(result["summary"]["output_layer_id"], vector_layer.id())
        self.assertEqual(result["summary"]["algorithm"], "native:zonalstatisticsfb")
        self.assertNotIn("algorithm_selected", result["summary"])
        self.assertNotIn("fallback_used", result["summary"])
        self.assertEqual(self.vector_field_names(vector_layer), original_fields)

    def test_failure_invalid_raster_ref(self):
        """
        作用：执行测试用例 `failure invalid raster ref`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure invalid raster ref`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon2")

        with self.assertRaises(Exception) as ctx:
            tools.raster_stats_zonal(
                vector_layer_ref=vector_layer.id(),
                raster_layer_ref="missing-raster",
            )
        self.assertIn("Layer not found", str(ctx.exception))

    @patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal.processing.run")
    @patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal.QgsApplication.processingRegistry")
    def test_success_uses_latest_algorithm_only(
        self, mock_processing_registry, mock_run
    ):
        """
        作用：执行测试用例 `success uses latest algorithm only`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success uses latest algorithm only`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_processing_registry`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `mock_run`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon_fb_only")
        raster_layer = self.add_sample_raster_layer("zonal_raster_fb_only")
        mock_processing_registry.return_value = _FakeProcessingRegistry(
            ["native:zonalstatisticsfb"]
        )
        mock_run.return_value = {"OUTPUT": "fb-output-layer"}

        with patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal._PROCESSING_INITIALIZED", True):
            result = tools.raster_stats_zonal(
                vector_layer_ref=vector_layer.id(),
                raster_layer_ref=raster_layer.id(),
                raster_band=1,
                in_place=False,
            )

        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertEqual(result["summary"]["algorithm"], "native:zonalstatisticsfb")
        self.assertEqual(result["summary"]["output_layer_id"], "fb-output-layer")
        self.assertNotIn("algorithm_selected", result["summary"])
        self.assertNotIn("fallback_used", result["summary"])
        self.assertEqual(mock_run.call_args.args[0], "native:zonalstatisticsfb")
        called_parameters = mock_run.call_args.args[1]
        self.assertIn("INPUT", called_parameters)
        self.assertIn("OUTPUT", called_parameters)
        self.assertNotIn("INPUT_VECTOR", called_parameters)

    @patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal.processing.run")
    @patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal.QgsApplication.processingRegistry")
    def test_success_in_place_normalizes_parameters_and_output_layer_id(
        self, mock_processing_registry, mock_run
    ):
        """
        作用：执行测试用例 `success in place normalizes parameters and output layer id`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success in place normalizes parameters and output layer id`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_processing_registry`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `mock_run`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon_in_place")
        raster_layer = self.add_sample_raster_layer("zonal_raster_in_place")
        mock_processing_registry.return_value = _FakeProcessingRegistry(
            ["native:zonalstatisticsfb"]
        )
        mock_run.return_value = {}

        with patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal._PROCESSING_INITIALIZED", True):
            result = tools.raster_stats_zonal(
                vector_layer_ref=vector_layer.id(),
                raster_layer_ref=raster_layer.id(),
                raster_band=0,
                column_prefix="",
                in_place=True,
            )

        called_parameters = mock_run.call_args.args[1]
        self.assertEqual(called_parameters["RASTER_BAND"], 1)
        self.assertEqual(called_parameters["COLUMN_PREFIX"], "z_")
        self.assertEqual(called_parameters["OUTPUT"], vector_layer)
        self.assertEqual(result["summary"]["mode"], "in_place")
        self.assertEqual(result["summary"]["output_layer_id"], vector_layer.id())

    @patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal.QgsApplication.processingRegistry")
    def test_failure_when_zonalstatisticsfb_unavailable(self, mock_processing_registry):
        """
        作用：执行测试用例 `failure when zonalstatisticsfb unavailable`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure when zonalstatisticsfb unavailable`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_processing_registry`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon_no_algorithm")
        raster_layer = self.add_sample_raster_layer("zonal_raster_no_algorithm")
        mock_processing_registry.return_value = _FakeProcessingRegistry([])

        with (
            patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal._PROCESSING_INITIALIZED", True),
            self.assertRaises(Exception) as ctx,
        ):
            tools.raster_stats_zonal(
                vector_layer_ref=vector_layer.id(),
                raster_layer_ref=raster_layer.id(),
            )
        self.assertIn("Algorithm not available: native:zonalstatisticsfb", str(ctx.exception))
