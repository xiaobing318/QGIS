from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemEditWriteTextTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "filesystem_edit_write_text")

    def test_success_write_and_overwrite(self):
        """验证 write and overwrite 的成功场景。"""
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
        """验证 without confirm write 的失败场景。"""
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = root / "target.txt"

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_write_text(path=str(target), content="v1")
        self.assertIn("confirm_write must be true", str(ctx.exception))

    def test_failure_overwrite_without_confirm(self):
        """验证 overwrite without confirm 的失败场景。"""
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
