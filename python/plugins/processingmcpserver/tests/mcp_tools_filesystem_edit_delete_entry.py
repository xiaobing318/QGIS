from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemEditDeleteEntryTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the target capability is registered."""
        assert_tool_registered(self, "filesystem_edit_delete_entry")

    def test_success_delete_entry(self):
        """Verify the successful path for delete entry."""
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
        """Verify the failure path for without confirm write."""
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
        """Verify the failure path for without confirmation."""
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
