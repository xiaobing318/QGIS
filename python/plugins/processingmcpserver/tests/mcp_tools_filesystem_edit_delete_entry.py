from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemEditDeleteEntryTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "mcp_tools_filesystem_edit_delete_entry")

    def test_success_delete_entry(self):
        """
        作用：执行测试用例 `success delete entry`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success delete entry`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "delete.txt", "bye")

        result = tools.mcp_tools_filesystem_edit_delete_entry(
            path=str(target),
            confirm_destructive=True,
            confirm_write=True,
        )
        self.assertTrue(result["ok"])
        self.assertFalse(target.exists())

    def test_failure_without_confirm_write(self):
        """
        作用：执行测试用例 `failure without confirm write`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure without confirm write`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "delete.txt", "bye")

        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_filesystem_edit_delete_entry(
                path=str(target),
                confirm_destructive=True,
            )
        self.assertIn("confirm_write must be true", str(ctx.exception))

    def test_failure_without_confirmation(self):
        """
        作用：执行测试用例 `failure without confirmation`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure without confirmation`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "delete.txt", "bye")

        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_filesystem_edit_delete_entry(
                path=str(target),
                confirm_destructive=False,
                confirm_write=True,
            )
        self.assertIn("confirm_destructive must be true", str(ctx.exception))

