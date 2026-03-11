from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemQueryListEntriesTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "filesystem_query_list_entries")

    def test_success_list_entries(self):
        """验证 list entries 的成功场景。"""
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

    def test_success_recursive_list_filters_relative_paths_and_truncates(self):
        """验证 recursive list filters relative paths and truncates 的成功场景。"""
        tools = self.build_tools()
        root = self.make_temp_dir()
        nested = root / "nested"
        self.create_text_file(root / "a.txt", "alpha")
        self.create_text_file(nested / "b.log", "beta")
        self.create_text_file(nested / "c.log", "gamma")

        filtered = tools.filesystem_query_list_entries(
            directory=str(root),
            recursive=True,
            include_files=True,
            include_directories=False,
            name_glob="*.log",
            limit=10,
        )
        truncated = tools.filesystem_query_list_entries(
            directory=str(root),
            recursive=True,
            include_files=True,
            include_directories=True,
            name_glob="*",
            limit=1,
        )

        self.assertEqual(filtered["summary"]["returned_count"], 2)
        self.assertTrue(
            all(not entry["is_directory"] for entry in filtered["outputs"]["entries"])
        )
        self.assertEqual(
            {entry["relative_path"] for entry in filtered["outputs"]["entries"]},
            {"nested\\b.log", "nested\\c.log"},
        )
        self.assertEqual(truncated["summary"]["returned_count"], 1)
        self.assertTrue(truncated["summary"]["truncated"])
        self.assertGreaterEqual(truncated["summary"]["matched_total"], 3)

    def test_failure_invalid_include_flags(self):
        """验证 invalid include flags 的失败场景。"""
        tools = self.build_tools()
        root = self.make_temp_dir()

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_query_list_entries(
                directory=str(root),
                include_files=False,
                include_directories=False,
            )
        self.assertIn("cannot both be false", str(ctx.exception))
