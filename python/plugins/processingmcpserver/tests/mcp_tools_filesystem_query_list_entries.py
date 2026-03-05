from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemQueryListEntriesTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "filesystem_query_list_entries")

    def test_success_list_entries(self):
        tools = self.build_tools()
        root = self.make_temp_dir()
        self.create_text_file(root / "a.txt", "alpha")
        self.create_text_file(root / "b.log", "beta")

        result = tools.filesystem_query_list_entries(
            directory=str(root),
            recursive=False,
            include_files=True,
            include_directories=True,
            name_glob="*",
            limit=100,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["returned_count"], 2)

    def test_failure_invalid_include_flags(self):
        tools = self.build_tools()
        root = self.make_temp_dir()

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_query_list_entries(
                directory=str(root),
                include_files=False,
                include_directories=False,
            )
        self.assertIn("cannot both be false", str(ctx.exception))
