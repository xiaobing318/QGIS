from __future__ import annotations

import tempfile
from pathlib import Path
from unittest.mock import patch

import processingmcpserver
import processingmcpserver.capabilities_markdown as capabilities_markdown
from processingmcpserver.mcp_prompts import (
    REGISTERED_PROMPT_NAMES,
    _REGISTERED_PROMPT_DOCSTRINGS,
)
from processingmcpserver.mcp_resources import (
    REGISTERED_RESOURCE_URIS,
    _REGISTERED_RESOURCE_DOCSTRINGS,
)
from processingmcpserver.mcp_tools import (
    REGISTERED_TOOL_NAMES,
    _REGISTERED_TOOL_DOCSTRINGS,
)

from ._shared_case_base import ProcessingMCPTestBase


class CapabilitiesMarkdownRuntimeTest(ProcessingMCPTestBase):
    def test_default_output_path_targets_exporter_module_directory(self):
        """Verify the default behavior for output path targets exporter module directory."""
        expected = (
            Path(capabilities_markdown.__file__).resolve().parent
            / capabilities_markdown.DEFAULT_CAPABILITIES_MARKDOWN_FILENAME
        ).resolve(strict=False)

        self.assertEqual(capabilities_markdown._resolve_output_path(""), expected)

    def test_write_markdown_uses_default_target_when_output_omitted(self):
        """Verify write markdown uses default target when output omitted."""
        with tempfile.TemporaryDirectory() as temp_dir:
            expected = (
                Path(temp_dir) / capabilities_markdown.DEFAULT_CAPABILITIES_MARKDOWN_FILENAME
            ).resolve(strict=False)

            with patch(
                "processingmcpserver.capabilities_markdown._resolve_output_path",
                return_value=expected,
            ) as resolve_output_path:
                written_path = processingmcpserver.write_mcp_capabilities_markdown()

            resolve_output_path.assert_called_once_with("")
            self.assertEqual(written_path, expected)
            self.assertTrue(expected.exists())

    def test_write_markdown_supports_explicit_output_path(self):
        """Verify write markdown supports explicit output path."""
        with tempfile.TemporaryDirectory() as temp_dir:
            output_path = Path(temp_dir) / "nested" / "caps.md"

            written_path = processingmcpserver.write_mcp_capabilities_markdown(output_path)

            self.assertEqual(written_path, output_path.resolve(strict=False))
            self.assertTrue(written_path.exists())

    def test_written_markdown_covers_registered_capabilities(self):
        """Verify written markdown covers registered capabilities."""
        with tempfile.TemporaryDirectory() as temp_dir:
            output_path = Path(temp_dir) / "capabilities.md"
            written_path = processingmcpserver.write_mcp_capabilities_markdown(output_path)
            content = written_path.read_text(encoding="utf-8")

        self.assertIn("# Processing MCP Capabilities", content)
        self.assertIn(f"- Tool count: `{len(REGISTERED_TOOL_NAMES)}`", content)
        self.assertIn(f"- Prompt count: `{len(REGISTERED_PROMPT_NAMES)}`", content)
        self.assertIn(f"- Resource count: `{len(REGISTERED_RESOURCE_URIS)}`", content)

        for tool_name in REGISTERED_TOOL_NAMES:
            self.assertIn(f"### `{tool_name}`", content)
            self.assertIn(_REGISTERED_TOOL_DOCSTRINGS[tool_name], content)

        for prompt_name in REGISTERED_PROMPT_NAMES:
            self.assertIn(f"### `{prompt_name}`", content)
            self.assertIn(_REGISTERED_PROMPT_DOCSTRINGS[prompt_name], content)

        for resource_uri in REGISTERED_RESOURCE_URIS:
            self.assertIn(f"### `{resource_uri}`", content)
            self.assertIn(_REGISTERED_RESOURCE_DOCSTRINGS[resource_uri], content)
