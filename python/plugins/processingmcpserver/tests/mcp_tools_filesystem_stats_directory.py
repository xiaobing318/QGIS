from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemStatsDirectoryTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "filesystem_stats_directory")

    def test_success_stats_directory(self):
        tools = self.build_tools()
        root = self.make_temp_dir()
        self.create_text_file(root / "a.txt", "aaa")
        self.create_text_file(root / "b.log", "bbb")

        result = tools.filesystem_stats_directory(directory=str(root), recursive=True)
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["file_count"], 2)
        self.assertIn(".txt", result["outputs"]["extensions"])

    def test_failure_missing_directory(self):
        tools = self.build_tools()
        missing_dir = self.make_temp_dir() / "missing"
        with self.assertRaises(Exception) as ctx:
            tools.filesystem_stats_directory(
                directory=str(missing_dir), recursive=True
            )
        self.assertIn("Directory not found", str(ctx.exception))
