from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemEditWriteTextTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "filesystem_edit_write_text")

    def test_success_write_and_overwrite(self):
        """
        作用：执行测试用例 `success write and overwrite`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success write and overwrite`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = root / "target.txt"

        first = tools.filesystem_edit_write_text(
            path=str(target), content="v1", confirm_write=True
        )
        self.assertTrue(first["ok"])

        second = tools.filesystem_edit_write_text(
            path=str(target),
            content="v2",
            overwrite=True,
            confirm_destructive=True,
            confirm_write=True,
        )
        self.assertTrue(second["ok"])
        self.assertEqual(target.read_text(encoding="utf-8"), "v2")

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
        target = root / "target.txt"

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_write_text(path=str(target), content="v1")
        self.assertIn("confirm_write must be true", str(ctx.exception))

    def test_failure_overwrite_without_confirm(self):
        """
        作用：执行测试用例 `failure overwrite without confirm`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure overwrite without confirm`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "target.txt", "v1")

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_write_text(
                path=str(target),
                content="v2",
                overwrite=True,
                confirm_destructive=False,
                confirm_write=True,
            )
        self.assertIn("confirm_destructive must be true", str(ctx.exception))
