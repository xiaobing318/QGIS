from __future__ import annotations

from dataclasses import replace
from pathlib import Path

from processingmcpserver.config import ProcessingMCPFilesystemConfig
from processingmcpserver.mcp_tools import ProcessingMCPTools

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyRunner, assert_tool_registered


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
        missing_path = self.make_temp_dir() / "missing.txt"
        with self.assertRaises(Exception) as ctx:
            tools.filesystem_query_read_text(path=str(missing_path))
        self.assertIn("File not found", str(ctx.exception))

    def test_failure_path_outside_allowed_roots(self):
        tools = self.build_tools()
        file_path = Path(__file__).resolve()

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_query_read_text(path=str(file_path), max_chars=3)
        self.assertIn("outside allowed_roots", str(ctx.exception))

    def test_failure_when_filesystem_tools_disabled(self):
        base_config = self._build_config("streamable-http")
        config = replace(
            base_config,
            filesystem=ProcessingMCPFilesystemConfig(
                allowed_roots=base_config.filesystem.allowed_roots,
                readonly_roots=[],
                disable_filesystem_tools=True,
            ),
        )
        tools = ProcessingMCPTools(iface=None, runner=DummyRunner(), config=config)
        root = self.make_temp_dir()
        file_path = self.create_text_file(root / "a.txt", "abcdefghij")

        with self.assertRaises(Exception) as ctx:
            tools.filesystem_query_read_text(path=str(file_path), max_chars=5)
        self.assertIn("disabled by configuration", str(ctx.exception))
