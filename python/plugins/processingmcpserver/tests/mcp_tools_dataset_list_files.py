from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsDatasetListFilesTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_dataset_list_files")

    def test_success_dataset_scan(self):
        """
        作用：执行测试用例 `success dataset scan`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success dataset scan`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        vector_file = self.copy_test_data_file(
            "sample_vector.geojson", temp_root, "dataset.geojson"
        )
        (temp_root / "note.txt").write_text("hello", encoding="utf-8")

        result = tools.mcp_tools_dataset_list_files(
            directory=str(vector_file.parent),
            recursive=False,
            dataset_kind="both",
            geometry_type="any",
            name_glob="*",
            limit=100,
        )
        self.assertEqual(result["returned"], 1)
        self.assertEqual(result["datasets"][0]["dataset_kind"], "vector")

    def test_success_dataset_scan_recursive_filter_and_truncate(self):
        """
        作用：执行测试用例 `success dataset scan recursive filter and truncate`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success dataset scan recursive filter and truncate`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        nested = temp_root / "nested"
        self.copy_test_data_file("sample_vector.geojson", temp_root, "top.geojson")
        self.copy_test_data_file("sample_vector.geojson", nested, "nested.geojson")
        self.copy_test_data_file("dem.tif", nested, "nested_dem.tif")

        recursive_result = tools.mcp_tools_dataset_list_files(
            directory=str(temp_root),
            recursive=True,
            dataset_kind="vector",
            geometry_type="point",
            name_glob="*.geojson",
            limit=10,
        )
        truncated_result = tools.mcp_tools_dataset_list_files(
            directory=str(temp_root),
            recursive=True,
            dataset_kind="both",
            geometry_type="any",
            name_glob="*",
            limit=1,
        )

        self.assertEqual(recursive_result["returned"], 2)
        self.assertEqual(recursive_result["matched_total"], 2)
        self.assertTrue(
            all(item["dataset_kind"] == "vector" for item in recursive_result["datasets"])
        )
        self.assertTrue(
            all(item["geometry_type"] == "point" for item in recursive_result["datasets"])
        )
        self.assertEqual(truncated_result["returned"], 1)
        self.assertGreaterEqual(truncated_result["matched_total"], 3)
        self.assertTrue(truncated_result["truncated"])

    def test_failure_missing_directory(self):
        """
        作用：执行测试用例 `failure missing directory`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure missing directory`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_dataset_list_files(directory="C:/not-exist", recursive=False)
        self.assertIn("Directory not found", str(ctx.exception))

