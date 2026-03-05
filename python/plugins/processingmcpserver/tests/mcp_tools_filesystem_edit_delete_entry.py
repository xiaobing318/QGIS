from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemEditDeleteEntryTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "filesystem_edit_delete_entry")

    def test_success_delete_entry(self):
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "delete.txt", "bye")

        result = tools.filesystem_edit_delete_entry(
            path=str(target),
            confirm_destructive=True,
        )
        self.assertTrue(result["ok"])
        self.assertFalse(target.exists())

    def test_failure_without_confirmation(self):
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "delete.txt", "bye")

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_delete_entry(
                path=str(target),
                confirm_destructive=False,
            )
        self.assertIn("confirm_destructive must be true", str(ctx.exception))
