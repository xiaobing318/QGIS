from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemQueryEntryInfoTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "filesystem_query_entry_info")

    def test_success_entry_info(self):
        """验证 entry info 的成功场景。"""
        tools = self.build_tools()
        root = self.make_temp_dir()
        file_path = self.create_text_file(root / "a.txt", "hello")

        result = tools.filesystem_query_entry_info(path=str(file_path))
        self.assertTrue(result["ok"])
        self.assertEqual(result["outputs"]["entry"]["name"], "a.txt")

    def test_failure_missing_path(self):
        """验证 missing path 的失败场景。"""
        tools = self.build_tools()
        missing_path = self.make_temp_dir() / "missing.txt"
        with self.assertRaises(Exception) as ctx:
            tools.filesystem_query_entry_info(path=str(missing_path))
        self.assertIn("Path not found", str(ctx.exception))
