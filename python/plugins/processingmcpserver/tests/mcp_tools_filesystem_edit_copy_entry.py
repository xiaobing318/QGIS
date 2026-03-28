from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsFilesystemEditCopyEntryTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the target capability is registered."""
        assert_tool_registered(self, "filesystem_edit_copy_entry")

    def test_success_copy_entry(self):
        """Verify the successful path for copy entry."""
        tools = self.build_tools()
        root = self.make_temp_dir()
        source = self.create_text_file(root / "source.txt", "src")
        target = root / "copied.txt"

        result = tools.filesystem_edit_copy_entry(
            source_path=str(source),
            target_path=str(target),
            confirm_write=True,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(target.read_text(encoding="utf-8"), "src")

    def test_failure_without_confirm_write(self):
        """Verify the failure path for without confirm write."""
        tools = self.build_tools()
        root = self.make_temp_dir()
        source = self.create_text_file(root / "source.txt", "src")
        target = root / "copied.txt"

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_copy_entry(
                source_path=str(source),
                target_path=str(target),
            )
        self.assertIn("confirm_write must be true", str(ctx.exception))

    def test_failure_overwrite_without_confirm(self):
        """Verify the failure path for overwrite without confirm."""
        tools = self.build_tools()
        root = self.make_temp_dir()
        source = self.create_text_file(root / "source.txt", "src")
        target = self.create_text_file(root / "copied.txt", "old")

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_copy_entry(
                source_path=str(source),
                target_path=str(target),
                overwrite=True,
                confirm_destructive=False,
                confirm_write=True,
            )
        self.assertIn("confirm_destructive must be true", str(ctx.exception))
