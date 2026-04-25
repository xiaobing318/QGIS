from __future__ import annotations

from pathlib import Path

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemQueryReadTextTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_filesystem_query_read_text")

    def test_success_read_with_truncate(self):
        """
        作用：执行测试用例 `success read with truncate`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success read with truncate`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        root = self.make_temp_dir()
        file_path = self.create_text_file(root / "a.txt", "abcdefghij")

        result = tools.mcp_tools_filesystem_query_read_text(path=str(file_path), max_chars=5)
        self.assertTrue(result["ok"])
        self.assertEqual(result["outputs"]["text"], "abcde")
        self.assertTrue(result["summary"]["truncated"])

    def test_failure_missing_file(self):
        """
        作用：执行测试用例 `failure missing file`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure missing file`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        missing_path = self.make_temp_dir() / "missing.txt"
        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_filesystem_query_read_text(path=str(missing_path))
        self.assertIn("File not found", str(ctx.exception))

    def test_success_reads_path_outside_previous_whitelist_scope(self):
        """
        作用：执行测试用例 `success reads path outside previous whitelist scope`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success reads path outside previous whitelist scope`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        file_path = Path(__file__).resolve()

        result = tools.mcp_tools_filesystem_query_read_text(path=str(file_path), max_chars=3)
        self.assertTrue(result["ok"])
        self.assertEqual(result["outputs"]["text"], "fro")

