from __future__ import annotations

from qgis.core import QgsProject

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerRemoveTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "layer_remove")

    def test_success_remove_existing_layer(self):
        """
        作用：执行测试用例 `success remove existing layer`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success remove existing layer`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("remove_vector")
        layer_id = layer.id()

        result = tools.layer_remove(layer_id=layer_id)
        self.assertEqual(result["removed"], layer_id)
        self.assertNotIn(layer_id, QgsProject.instance().mapLayers())

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
            tools.layer_remove(layer_id="missing-layer")
        self.assertIn("Layer not found", str(ctx.exception))
