from __future__ import annotations

from dataclasses import replace

from processingmcpserver.config import ProcessingMCPFilesystemConfig
from processingmcpserver.mcp_tools import ProcessingMCPTools

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyRunner, assert_tool_registered


class ToolsFilesystemEditWriteTextTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "filesystem_edit_write_text")

    def test_success_write_and_overwrite(self):
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = root / "target.txt"

        first = tools.filesystem_edit_write_text(path=str(target), content="v1")
        self.assertTrue(first["ok"])

        second = tools.filesystem_edit_write_text(
            path=str(target),
            content="v2",
            overwrite=True,
            confirm_destructive=True,
        )
        self.assertTrue(second["ok"])
        self.assertEqual(target.read_text(encoding="utf-8"), "v2")

    def test_failure_overwrite_without_confirm(self):
        tools = self.build_tools()
        root = self.make_temp_dir()
        target = self.create_text_file(root / "target.txt", "v1")

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_write_text(
                path=str(target),
                content="v2",
                overwrite=True,
                confirm_destructive=False,
            )
        self.assertIn("confirm_destructive must be true", str(ctx.exception))

    def test_failure_write_inside_readonly_root(self):
        root = self.make_temp_dir()
        config = replace(
            self._build_config("streamable-http"),
            filesystem=ProcessingMCPFilesystemConfig(
                allowed_roots=[str(root)],
                readonly_roots=[str(root)],
                disable_filesystem_tools=False,
            ),
        )
        tools = ProcessingMCPTools(iface=None, runner=DummyRunner(), config=config)

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_edit_write_text(
                path=str(root / "readonly.txt"),
                content="v1",
            )
        self.assertIn("inside readonly_roots", str(ctx.exception))
