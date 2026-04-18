from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemStatsDirectoryTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_filesystem_stats_directory")

    def test_success_stats_directory(self):
        """
        作用：执行测试用例 `success stats directory`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success stats directory`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        root = self.make_temp_dir()
        self.create_text_file(root / "a.txt", "aaa")
        self.create_text_file(root / "b.log", "bbb")

        result = tools.mcp_tools_filesystem_stats_directory(directory=str(root), recursive=True)
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["file_count"], 2)
        self.assertIn(".txt", result["outputs"]["extensions"])

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
        missing_dir = self.make_temp_dir() / "missing"
        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_filesystem_stats_directory(
                directory=str(missing_dir), recursive=True
            )
        self.assertIn("Directory not found", str(ctx.exception))

