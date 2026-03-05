from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemQueryReadTextTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "filesystem_query_read_text")

    def test_success_read_with_truncate(self):
        tools = self.build_tools()
        root = self.make_temp_dir()
        file_path = self.create_text_file(root / "a.txt", "abcdefghij")

        result = tools.filesystem_query_read_text(path=str(file_path), max_chars=5)
        self.assertTrue(result["ok"])
        self.assertEqual(result["outputs"]["text"], "abcde")
        self.assertTrue(result["summary"]["truncated"])

    def test_failure_missing_file(self):
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.filesystem_query_read_text(path="C:/not-exist.txt")
        self.assertIn("File not found", str(ctx.exception))
