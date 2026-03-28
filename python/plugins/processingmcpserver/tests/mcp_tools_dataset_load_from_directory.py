from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsDatasetLoadFromDirectoryTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "dataset_load_from_directory")

    def test_success_dataset_load_vector(self):
        """
        作用：执行测试用例 `success dataset load vector`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success dataset load vector`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        vector_file = self.copy_test_data_file(
            "sample_vector.geojson", temp_root, "dataset_load.geojson"
        )

        result = tools.dataset_load_from_directory(
            directory=str(vector_file.parent),
            recursive=False,
            dataset_kind="vector",
            geometry_type="any",
            name_glob="*.geojson",
            limit=100,
            skip_invalid=True,
        )
        self.assertEqual(result["requested_count"], 1)
        self.assertEqual(result["loaded_count"], 1)
        self.assertEqual(result["failed_count"], 0)

    def test_success_dataset_load_raster(self):
        """
        作用：执行测试用例 `success dataset load raster`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success dataset load raster`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        raster_file = self.copy_test_data_file("dem.tif", temp_root, "dataset_load.tif")

        result = tools.dataset_load_from_directory(
            directory=str(raster_file.parent),
            recursive=False,
            dataset_kind="raster",
            geometry_type="any",
            name_glob="*.tif",
            limit=100,
            skip_invalid=True,
        )

        self.assertEqual(result["requested_count"], 1)
        self.assertEqual(result["loaded_count"], 1)
        self.assertEqual(result["failed_count"], 0)

    def test_success_skip_invalid_collects_failures(self):
        """
        作用：执行测试用例 `success skip invalid collects failures`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success skip invalid collects failures`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        self.copy_test_data_file("sample_vector.geojson", temp_root, "valid.geojson")
        self.create_text_file(temp_root / "broken.geojson", "{not-json}")

        result = tools.dataset_load_from_directory(
            directory=str(temp_root),
            recursive=False,
            dataset_kind="vector",
            geometry_type="any",
            name_glob="*.geojson",
            limit=100,
            skip_invalid=True,
        )

        self.assertEqual(result["requested_count"], 2)
        self.assertEqual(result["loaded_count"], 1)
        self.assertEqual(result["failed_count"], 1)
        self.assertIn("broken.geojson", result["failed"][0]["path"])

    def test_failure_skip_invalid_false_raises_on_invalid_dataset(self):
        """
        作用：执行测试用例 `failure skip invalid false raises on invalid dataset`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure skip invalid false raises on invalid dataset`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        self.copy_test_data_file("sample_vector.geojson", temp_root, "valid.geojson")
        self.create_text_file(temp_root / "broken.geojson", "{not-json}")

        with self.assertRaises(Exception) as ctx:
            tools.dataset_load_from_directory(
                directory=str(temp_root),
                recursive=False,
                dataset_kind="vector",
                geometry_type="any",
                name_glob="*.geojson",
                limit=100,
                skip_invalid=False,
            )
        self.assertIn("Failed to load dataset from directory", str(ctx.exception))

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
            tools.dataset_load_from_directory(directory="C:/not-exist")
        self.assertIn("Directory not found", str(ctx.exception))
