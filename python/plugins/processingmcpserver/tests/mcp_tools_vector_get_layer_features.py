from __future__ import annotations

import json

from processingmcpserver.mcp_tools import ProcessingMCPTools

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorGetLayerFeaturesTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "vector_get_layer_features")

    def test_success_limit_and_payload(self):
        """
        作用：执行测试用例 `success limit and payload`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success limit and payload`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("feature_vector")

        result = tools.vector_get_layer_features(layer_ref=layer.id(), limit=2)
        self.assertEqual(result["layer_id"], layer.id())
        self.assertEqual(result["applied_limit"], 2)
        self.assertEqual(len(result["features"]), 2)
        self.assertEqual(result["features"][0]["geometry_wkt"], "Point (108.9 34.2)")

    def test_success_zero_or_negative_limit_returns_empty_slice(self):
        """
        作用：执行测试用例 `success zero or negative limit returns empty slice`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success zero or negative limit returns empty slice`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("feature_vector_zero")

        zero_result = tools.vector_get_layer_features(layer_ref=layer.id(), limit=0)
        negative_result = tools.vector_get_layer_features(layer_ref=layer.id(), limit=-5)

        self.assertEqual(zero_result["applied_limit"], 0)
        self.assertEqual(zero_result["features"], [])
        self.assertEqual(negative_result["applied_limit"], 0)
        self.assertEqual(negative_result["features"], [])

    def test_success_limit_cap_is_reported(self):
        """
        作用：执行测试用例 `success limit cap is reported`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success limit cap is reported`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("feature_vector_capped")

        result = tools.vector_get_layer_features(
            layer_ref=layer.id(),
            limit=ProcessingMCPTools.MAX_FEATURE_LIMIT + 1,
        )

        self.assertEqual(result["applied_limit"], ProcessingMCPTools.MAX_FEATURE_LIMIT)
        self.assertTrue(result["limit_capped"])
        self.assertEqual(len(result["features"]), 3)

    def test_failure_layer_not_found(self):
        """
        作用：执行测试用例 `failure layer not found`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure layer not found`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.vector_get_layer_features(layer_ref="missing-layer", limit=2)
        self.assertIn("Layer not found", str(ctx.exception))

    def test_success_serializes_qt_field_values(self):
        """
        作用：执行测试用例 `success serializes qt field values`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success serializes qt field values`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_serialization_vector_layer("feature_vector_serialization")

        result = tools.vector_get_layer_features(layer_ref=layer.id(), limit=1)

        attributes = result["features"][0]["attributes"]
        self.assertEqual(attributes["event_date"], "2026-03-10")
        self.assertTrue(attributes["event_time"].startswith("12:34:56"))
        self.assertTrue(attributes["event_ts"].startswith("2026-03-10T12:34:56"))
        self.assertIs(attributes["enabled"], True)
        self.assertIsNone(attributes["notes"])
        json.dumps(result, ensure_ascii=False)
