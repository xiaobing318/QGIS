from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorAddLayersTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "vector_add_layers")

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
        geojson = self.sample_vector_path()

        result = tools.vector_add_layers(
            paths=[str(geojson), "C:/not-exist/sample.geojson"],
            provider="ogr",
            skip_invalid=True,
        )
        self.assertEqual(result["requested_count"], 2)
        self.assertEqual(result["loaded_count"], 1)
        self.assertEqual(result["failed_count"], 1)

    def test_success_single_path_list(self):
        """
        作用：执行测试用例 `success single path list`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success single path list`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        geojson = self.sample_vector_path()

        result = tools.vector_add_layers(paths=[str(geojson)], provider="ogr")
        self.assertEqual(result["requested_count"], 1)
        self.assertEqual(result["loaded_count"], 1)
        self.assertEqual(result["failed_count"], 0)

    def test_failure_paths_must_be_list_of_strings(self):
        """
        作用：执行测试用例 `failure paths must be list of strings`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure paths must be list of strings`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()

        with self.assertRaises(Exception) as text_ctx:
            tools.vector_add_layers(paths="C:/data/a.geojson", provider="ogr")  # type: ignore[arg-type]
        self.assertIn("paths must be a list[str]", str(text_ctx.exception))

        with self.assertRaises(Exception) as mixed_ctx:
            tools.vector_add_layers(paths=["C:/data/a.geojson", 1], provider="ogr")  # type: ignore[list-item]
        self.assertIn("paths must be a list[str]", str(mixed_ctx.exception))

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
        geojson = self.sample_vector_path()

        with self.assertRaises(Exception) as ctx:
            tools.vector_add_layers(
                paths=[str(geojson), "C:/not-exist/sample.geojson"],
                provider="ogr",
                skip_invalid=False,
            )
        self.assertIn("Failed to load vector dataset", str(ctx.exception))
