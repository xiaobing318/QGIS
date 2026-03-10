from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProcessingListProvidersTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "processing_list_providers")

    def test_success_list_providers(self):
        tools = self.build_tools()
        result = tools.processing_list_providers()

        self.assertIn("count", result)
        self.assertIn("providers", result)
        self.assertIsInstance(result["providers"], list)

    def test_safety_payload_shape(self):
        tools = self.build_tools()
        result = tools.processing_list_providers()
        if result["providers"]:
            first = result["providers"][0]
            self.assertIn("id", first)
            self.assertIn("name", first)
            self.assertIn("active", first)
