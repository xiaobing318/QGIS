from __future__ import annotations

from processingmcpserver.mcp_tools import ProcessingMCPTools

from ._shared_case_base import ProcessingMCPTestBase


class ToolsNormalizersTest(ProcessingMCPTestBase):
    def test_feature_limit_default_and_dynamic_override(self):
        """
        作用：执行测试用例 `feature limit default and dynamic override`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `feature limit default and dynamic override`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()

        requested, applied, capped = tools._normalize_feature_limit(None)
        self.assertEqual(requested, ProcessingMCPTools.DEFAULT_FEATURE_LIMIT)
        self.assertEqual(applied, ProcessingMCPTools.DEFAULT_FEATURE_LIMIT)
        self.assertFalse(capped)

        requested, applied, capped = tools._normalize_feature_limit(42)
        self.assertEqual(requested, 42)
        self.assertEqual(applied, 42)
        self.assertFalse(capped)

    def test_feature_limit_handles_zero_negative_and_cap(self):
        """
        作用：执行测试用例 `feature limit handles zero negative and cap`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `feature limit handles zero negative and cap`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()

        requested, applied, capped = tools._normalize_feature_limit(0)
        self.assertEqual((requested, applied, capped), (0, 0, False))

        requested, applied, capped = tools._normalize_feature_limit(-5)
        self.assertEqual((requested, applied, capped), (-5, 0, False))

        requested, applied, capped = tools._normalize_feature_limit(
            ProcessingMCPTools.MAX_FEATURE_LIMIT + 1
        )
        self.assertEqual(requested, ProcessingMCPTools.MAX_FEATURE_LIMIT + 1)
        self.assertEqual(applied, ProcessingMCPTools.MAX_FEATURE_LIMIT)
        self.assertTrue(capped)

    def test_algorithm_dataset_and_filesystem_limits_cap(self):
        """
        作用：执行测试用例 `algorithm dataset and filesystem limits cap`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `algorithm dataset and filesystem limits cap`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()

        requested, applied, capped = tools._normalize_algorithm_list_limit(
            ProcessingMCPTools.MAX_ALGORITHM_LIST_LIMIT + 1
        )
        self.assertEqual(requested, ProcessingMCPTools.MAX_ALGORITHM_LIST_LIMIT + 1)
        self.assertEqual(applied, ProcessingMCPTools.MAX_ALGORITHM_LIST_LIMIT)
        self.assertTrue(capped)

        requested, applied, capped = tools._normalize_dataset_limit(
            ProcessingMCPTools.MAX_DATASET_LIMIT + 1
        )
        self.assertEqual(requested, ProcessingMCPTools.MAX_DATASET_LIMIT + 1)
        self.assertEqual(applied, ProcessingMCPTools.MAX_DATASET_LIMIT)
        self.assertTrue(capped)

        requested, applied, capped = tools._normalize_filesystem_limit(
            ProcessingMCPTools.MAX_FILESYSTEM_LIST_LIMIT + 1
        )
        self.assertEqual(
            requested, ProcessingMCPTools.MAX_FILESYSTEM_LIST_LIMIT + 1
        )
        self.assertEqual(applied, ProcessingMCPTools.MAX_FILESYSTEM_LIST_LIMIT)
        self.assertTrue(capped)

    def test_filter_and_glob_normalizers(self):
        """
        作用：执行测试用例 `filter and glob normalizers`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `filter and glob normalizers`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()

        self.assertEqual(tools._normalize_dataset_kind("vector"), "vector")
        self.assertEqual(tools._normalize_dataset_kind("both"), "both")
        self.assertEqual(tools._normalize_geometry_type_filter("line"), "line")
        self.assertEqual(tools._normalize_layer_types_filter("raster"), "raster")
        self.assertEqual(tools._normalize_name_glob(""), "*")
        self.assertEqual(tools._normalize_name_glob(None), "*")

        with self.assertRaises(Exception):
            tools._normalize_dataset_kind("invalid-kind")
        with self.assertRaises(Exception):
            tools._normalize_geometry_type_filter("curve")
        with self.assertRaises(Exception):
            tools._normalize_layer_types_filter("mesh")
