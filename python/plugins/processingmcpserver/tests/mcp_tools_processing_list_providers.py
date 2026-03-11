from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProcessingListProvidersTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "processing_list_providers")

    def test_success_list_providers(self):
        """验证 list providers 的成功场景。"""
        tools = self.build_tools()
        result = tools.processing_list_providers()

        self.assertIn("count", result)
        self.assertIn("providers", result)
        self.assertIsInstance(result["providers"], list)

    def test_safety_payload_shape(self):
        """验证 safety payload shape 场景。"""
        tools = self.build_tools()
        result = tools.processing_list_providers()
        if result["providers"]:
            first = result["providers"][0]
            self.assertIn("id", first)
            self.assertIn("name", first)
            self.assertIn("active", first)
