from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorTableInsertRecordsTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_vector_table_insert_records")

    def test_default_creates_copy_layer_and_reports_unknown_fields(self):
        """
        作用：执行测试用例 `default creates copy layer and reports unknown fields`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `default creates copy layer and reports unknown fields`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("insert_records_vector_copy")

        result = tools.mcp_tools_vector_table_insert_records(
            layer_ref=layer.id(),
            records=[
                {
                    "name": "delta",
                    "value": 4.5,
                    "category": "new",
                    "ignored_extra": "skip-me",
                    "geometry_wkt": "POINT(108.9300 34.2300)",
                }
            ],
        )

        output_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(output_layer)
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertNotEqual(result["summary"]["output_layer_id"], layer.id())
        self.assertEqual(layer.featureCount(), 3)
        self.assertEqual(output_layer.featureCount(), 4)
        self.assertEqual(result["outputs"]["ignored_fields"], ["ignored_extra"])
        self.assertIn("Ignored unknown fields", result["warnings"][0])

    def test_success_insert_records(self):
        """
        作用：执行测试用例 `success insert records`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success insert records`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("insert_records_vector")

        result = tools.mcp_tools_vector_table_insert_records(
            layer_ref=layer.id(),
            records=[
                {
                    "name": "delta",
                    "value": 4.5,
                    "category": "new",
                    "geometry_wkt": "POINT(108.9300 34.2300)",
                }
            ],
            in_place=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["affected_count"], 1)

    def test_failure_empty_records(self):
        """
        作用：执行测试用例 `failure empty records`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure empty records`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("insert_records_vector2")

        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_vector_table_insert_records(layer_ref=layer.id(), records=[])
        self.assertIn("records is required", str(ctx.exception))

