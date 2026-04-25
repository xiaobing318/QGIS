from __future__ import annotations

from qgis.core import QgsProject

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerRemoveBatchTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_layer_remove_batch")

    def test_success_remove_and_report_missing(self):
        """
        作用：执行测试用例 `success remove and report missing`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success remove and report missing`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer1 = self.add_sample_vector_layer("remove_batch_vector_1")
        layer2 = self.add_sample_vector_layer("remove_batch_vector_2")
        layer1_id = layer1.id()
        layer2_id = layer2.id()

        result = tools.mcp_tools_layer_remove_batch([layer1_id, "missing", layer2_id])
        self.assertIn(layer1_id, result["removed"])
        self.assertIn(layer2_id, result["removed"])
        self.assertIn("missing", result["missing"])
        self.assertNotIn(layer1_id, QgsProject.instance().mapLayers())
        self.assertNotIn(layer2_id, QgsProject.instance().mapLayers())

    def test_success_single_layer_id(self):
        """
        作用：执行测试用例 `success single layer id`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success single layer id`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("remove_batch_single")
        layer_id = layer.id()

        result = tools.mcp_tools_layer_remove_batch([layer_id])
        self.assertEqual(result["removed"], [layer_id])
        self.assertEqual(result["missing"], [])
        self.assertNotIn(layer_id, QgsProject.instance().mapLayers())

    def test_safety_ignore_empty_ids(self):
        """
        作用：执行测试用例 `safety ignore empty ids`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `safety ignore empty ids`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        result = tools.mcp_tools_layer_remove_batch(["", "   "])
        self.assertEqual(result["removed"], [])
        self.assertEqual(result["missing"], [])

