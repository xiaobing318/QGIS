from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsCommonGetQgisInfoTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the expected capability is registered."""
        assert_tool_registered(self, "common_get_qgis_info")

    def test_success_returns_runtime_snapshot(self):
        """Verify that the runtime snapshot is returned."""
        tools = self.build_tools()
        result = tools.common_get_qgis_info()

        self.assertIn("qgis", result)
        self.assertIn("python", result)
        self.assertIn("project", result)
        self.assertIn("processing_mcp", result)

    def test_success_contains_expected_sections(self):
        """Verify that the response contains the expected sections."""
        tools = self.build_tools()
        result = tools.common_get_qgis_info()
        self.assertIn("version", result["qgis"])
        self.assertIn("platform", result)
        self.assertIn("active_plugins", result)
        self.assertIn("layer_count", result["project"])
        self.assertIn("available", result["processing_mcp"])
        self.assertIn("filesystem", result["processing_mcp"])
        self.assertIn("write_policy", result["processing_mcp"]["filesystem"])
        self.assertTrue(
            result["processing_mcp"]["filesystem"]["write_policy"][
                "require_confirm_write"
            ]
        )
