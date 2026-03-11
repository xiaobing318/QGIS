from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemEditAppendTextTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "filesystem_edit_append_text")

    def test_success_append(self):
        """验证 append 的成功场景。"""
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "target.txt", "v1")

        result = tools.filesystem_edit_append_text(path=str(target), content="-v2")
        self.assertTrue(result["ok"])
        self.assertEqual(target.read_text(encoding="utf-8"), "v1-v2")

    def test_failure_parent_missing_without_create(self):
        """验证 parent missing without create 的失败场景。"""
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = root / "missing" / "target.txt"
        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_append_text(
                path=str(target),
                content="x",
                create_parents=False,
            )
        self.assertIn("Parent directory not found", str(ctx.exception))
