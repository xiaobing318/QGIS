from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemEditDeleteEntryTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "filesystem_edit_delete_entry")

    def test_success_delete_entry(self):
        """验证 delete entry 的成功场景。"""
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "delete.txt", "bye")

        result = tools.filesystem_edit_delete_entry(
            path=str(target),
            confirm_destructive=True,
            confirm_write=True,
        )
        self.assertTrue(result["ok"])
        self.assertFalse(target.exists())

    def test_failure_without_confirm_write(self):
        """验证 without confirm write 的失败场景。"""
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "delete.txt", "bye")

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_delete_entry(
                path=str(target),
                confirm_destructive=True,
            )
        self.assertIn("confirm_write must be true", str(ctx.exception))

    def test_failure_without_confirmation(self):
        """验证 without confirmation 的失败场景。"""
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "delete.txt", "bye")

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_delete_entry(
                path=str(target),
                confirm_destructive=False,
                confirm_write=True,
            )
        self.assertIn("confirm_destructive must be true", str(ctx.exception))
