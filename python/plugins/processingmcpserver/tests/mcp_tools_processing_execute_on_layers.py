from __future__ import annotations

from unittest.mock import patch

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProcessingExecuteOnLayersTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "processing_execute_on_layers")

    @patch("processingmcpserver.mcp_tools.mcp_tools_processing_execute_on_layers.processing.run")
    def test_success_default_safety_policy(self, mock_run):
        """
        作用：执行测试用例 `success default safety policy`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success default safety policy`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_run`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        mock_run.return_value = {"OUTPUT": "TEMPORARY_OUTPUT"}
        layer = self.add_sample_vector_layer("execute_on_layers_vector")

        result = tools.processing_execute_on_layers(
            algorithm="fake:buffer",
            layer_bindings={"INPUT": layer.id()},
            parameters={
                "OUTPUT": "C:/tmp/result.gpkg",
                "IN_PLACE": True,
            },
            load_results=False,
            batch_mode=False,
        )

        called_parameters = mock_run.call_args.args[1]
        self.assertEqual(called_parameters.get("INPUT"), layer.id())
        self.assertEqual(called_parameters.get("OUTPUT"), "TEMPORARY_OUTPUT")
        self.assertFalse(bool(called_parameters.get("IN_PLACE")))
        self.assertFalse(result["safety_policy"]["allow_disk_write"])
        self.assertFalse(result["safety_policy"]["allow_in_place_edit"])

    @patch("processingmcpserver.mcp_tools.mcp_tools_processing_execute_on_layers.processing.run")
    def test_batch_mode_mismatch_reported(self, mock_run):
        """
        作用：执行测试用例 `batch mode mismatch reported`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `batch mode mismatch reported`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_run`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        mock_run.return_value = {"OUTPUT": "TEMPORARY_OUTPUT"}
        layer = self.add_sample_vector_layer("execute_on_layers_vector2")

        result = tools.processing_execute_on_layers(
            algorithm="fake:buffer",
            layer_bindings={"INPUT": [layer.id(), layer.id()], "CLIP": [layer.id()]},
            parameters={},
            load_results=False,
            batch_mode=True,
        )

        self.assertEqual(result["run_count"], 2)
        self.assertEqual(result["success_count"], 1)
        self.assertEqual(result["failure_count"], 1)

    @patch("processingmcpserver.mcp_tools.mcp_tools_processing_execute_on_layers.processing.run")
    def test_explicit_allow_disk_write_and_in_place(self, mock_run):
        """
        作用：执行测试用例 `explicit allow disk write and in place`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `explicit allow disk write and in place`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_run`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        mock_run.return_value = {"OUTPUT": "C:/tmp/result.gpkg"}
        layer = self.add_sample_vector_layer("execute_on_layers_vector3")

        result = tools.processing_execute_on_layers(
            algorithm="fake:buffer",
            layer_bindings={"INPUT": layer.id()},
            parameters={"OUTPUT": "C:/tmp/result.gpkg", "IN_PLACE": True},
            load_results=False,
            batch_mode=False,
            allow_disk_write=True,
            allow_in_place_edit=True,
        )

        called_parameters = mock_run.call_args.args[1]
        self.assertEqual(called_parameters.get("OUTPUT"), "C:/tmp/result.gpkg")
        self.assertTrue(bool(called_parameters.get("IN_PLACE")))
        self.assertTrue(result["safety_policy"]["allow_disk_write"])
        self.assertTrue(result["safety_policy"]["allow_in_place_edit"])

    def test_failure_requires_bindings(self):
        """
        作用：执行测试用例 `failure requires bindings`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure requires bindings`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.processing_execute_on_layers(
                algorithm="native:buffer",
                layer_bindings={},
                parameters={},
                load_results=False,
                batch_mode=False,
            )
        self.assertIn("layer_bindings is required", str(ctx.exception))

    def test_failure_invalid_bound_layer_ref(self):
        """
        作用：执行测试用例 `failure invalid bound layer ref`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure invalid bound layer ref`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()

        with self.assertRaises(Exception) as ctx:
            tools.processing_execute_on_layers(
                algorithm="fake:buffer",
                layer_bindings={"INPUT": "missing-layer"},
                parameters={},
                load_results=False,
                batch_mode=False,
            )
        self.assertIn("Layer not found", str(ctx.exception))

    @patch("processingmcpserver.mcp_tools.mcp_tools_processing_execute_on_layers.processing.run")
    def test_failure_propagates_processing_runtime_error(self, mock_run):
        """
        作用：执行测试用例 `failure propagates processing runtime error`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure propagates processing runtime error`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_run`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("execute_on_layers_vector4")
        mock_run.side_effect = RuntimeError("processing boom")

        with self.assertRaises(RuntimeError) as ctx:
            tools.processing_execute_on_layers(
                algorithm="fake:buffer",
                layer_bindings={"INPUT": layer.id()},
                parameters={},
                load_results=False,
                batch_mode=False,
            )
        self.assertIn("processing boom", str(ctx.exception))
