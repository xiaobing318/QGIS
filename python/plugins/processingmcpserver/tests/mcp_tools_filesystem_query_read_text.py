from __future__ import annotations

from pathlib import Path

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemQueryReadTextTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the expected capability is registered."""
        assert_tool_registered(self, "filesystem_query_read_text")

    def test_success_read_with_truncate(self):
        """Verify the successful path for reading with truncation."""
        tools = self.build_tools()
        root = self.make_temp_dir()
        file_path = self.create_text_file(root / "a.txt", "abcdefghij")

        result = tools.filesystem_query_read_text(path=str(file_path), max_chars=5)
        self.assertTrue(result["ok"])
        self.assertEqual(result["outputs"]["text"], "abcde")
        self.assertTrue(result["summary"]["truncated"])

    def test_failure_missing_file(self):
        """Verify the failure path for a missing file."""
        tools = self.build_tools()
        missing_path = self.make_temp_dir() / "missing.txt"
        with self.assertRaises(Exception) as ctx:
            tools.filesystem_query_read_text(path=str(missing_path))
        self.assertIn("File not found", str(ctx.exception))

    def test_success_reads_path_outside_previous_whitelist_scope(self):
        """Verify that a path outside the previous whitelist scope can be read."""
        tools = self.build_tools()
        file_path = Path(__file__).resolve()

        result = tools.filesystem_query_read_text(path=str(file_path), max_chars=3)
        self.assertTrue(result["ok"])
        self.assertEqual(result["outputs"]["text"], "fro")
