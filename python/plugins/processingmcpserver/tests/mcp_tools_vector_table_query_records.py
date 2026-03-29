from __future__ import annotations

import json

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableQueryRecordsTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "vector_table_query_records")

    def test_success_query_records(self):
        """
        作用：执行测试用例 `success query records`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success query records`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("query_records_vector")

        result = tools.vector_table_query_records(
            layer_ref=layer.id(),
            where="\"name\" = 'alpha'",
            fields=["name", "value"],
            limit=1,
            order_by="name",
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["returned"], 1)
        self.assertEqual(result["outputs"]["records"][0]["attributes"]["name"], "alpha")

    def test_success_query_records_with_offset_sort_and_geometry(self):
        """
        作用：执行测试用例 `success query records with offset sort and geometry`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success query records with offset sort and geometry`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("query_records_vector_offset")

        result = tools.vector_table_query_records(
            layer_ref=layer.id(),
            limit=1,
            offset=1,
            order_by="name",
            include_geometry=True,
        )

        self.assertEqual(result["summary"]["offset"], 1)
        self.assertEqual(result["summary"]["returned"], 1)
        self.assertEqual(result["outputs"]["records"][0]["attributes"]["name"], "beta")
        self.assertEqual(
            result["outputs"]["records"][0]["geometry_wkt"], "Point (108.91 34.21)"
        )

    def test_success_can_replace_vector_get_layer_features(self):
        """
        作用：执行测试用例 `success can replace vector get layer features`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success can replace vector get layer features`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("query_records_vector_get_features_compat")

        result = tools.vector_table_query_records(
            layer_ref=layer.id(),
            limit=2,
            include_geometry=True,
        )

        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["requested_limit"], 2)
        self.assertEqual(result["summary"]["applied_limit"], 2)
        self.assertEqual(result["summary"]["returned"], 2)
        self.assertFalse(result["summary"]["limit_capped"])
        self.assertEqual(len(result["outputs"]["records"]), 2)
        self.assertEqual(
            result["outputs"]["records"][0]["geometry_wkt"], "Point (108.9 34.2)"
        )
        self.assertIn("name", result["outputs"]["records"][0]["attributes"])

    def test_failure_invalid_where(self):
        """
        作用：执行测试用例 `failure invalid where`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure invalid where`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("query_records_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.vector_table_query_records(layer_ref=layer.id(), where="\"name\" = ")
        self.assertIn("Invalid where expression", str(ctx.exception))

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
        layer = self.add_serialization_vector_layer("query_records_vector_serialization")

        result = tools.vector_table_query_records(
            layer_ref=layer.id(),
            fields=["event_date", "event_time", "event_ts", "enabled", "notes"],
            limit=1,
        )

        attributes = result["outputs"]["records"][0]["attributes"]
        self.assertEqual(attributes["event_date"], "2026-03-10")
        self.assertTrue(attributes["event_time"].startswith("12:34:56"))
        self.assertTrue(attributes["event_ts"].startswith("2026-03-10T12:34:56"))
        self.assertIs(attributes["enabled"], True)
        self.assertIsNone(attributes["notes"])
        json.dumps(result, ensure_ascii=False)
