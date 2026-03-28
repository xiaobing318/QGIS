from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProcessingListProvidersTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the target capability is registered."""
        assert_tool_registered(self, "processing_list_providers")

    def test_success_list_providers(self):
        """Verify the successful path for list providers."""
        tools = self.build_tools()
        result = tools.processing_list_providers()

        self.assertIn("count", result)
        self.assertIn("count_scope", result)
        self.assertIn("total_algorithm_count", result)
        self.assertIn("active_provider_count", result)
        self.assertIn("active_algorithm_count", result)
        self.assertIn("providers", result)
        self.assertEqual(result["count_scope"], "registered_providers")
        self.assertIsInstance(result["count"], int)
        self.assertIsInstance(result["total_algorithm_count"], int)
        self.assertIsInstance(result["active_provider_count"], int)
        self.assertIsInstance(result["active_algorithm_count"], int)
        self.assertIsInstance(result["providers"], list)
        self.assertEqual(result["count"], len(result["providers"]))

        provider_algorithm_total = sum(
            int(provider.get("algorithm_count", 0)) for provider in result["providers"]
        )
        active_provider_count = sum(
            1 for provider in result["providers"] if bool(provider.get("active"))
        )
        active_algorithm_total = sum(
            int(provider.get("algorithm_count", 0))
            for provider in result["providers"]
            if bool(provider.get("active"))
        )
        self.assertEqual(provider_algorithm_total, result["total_algorithm_count"])
        self.assertEqual(active_provider_count, result["active_provider_count"])
        self.assertEqual(active_algorithm_total, result["active_algorithm_count"])

    def test_safety_payload_shape(self):
        """Verify safety payload shape."""
        tools = self.build_tools()
        result = tools.processing_list_providers()
        if result["providers"]:
            first = result["providers"][0]
            self.assertIn("id", first)
            self.assertIn("name", first)
            self.assertIn("active", first)
            self.assertIn("algorithm_count", first)
            self.assertIsInstance(first["algorithm_count"], int)

    def test_consistent_with_processing_algorithms_count(self):
        """Verify provider statistics stay consistent with algorithm counts."""
        tools = self.build_tools()
        providers_result = tools.processing_list_providers()
        algorithms_result = tools.processing_get_algorithms(limit=0)

        self.assertIn("count", algorithms_result)
        self.assertEqual(
            int(algorithms_result["count"]),
            int(providers_result["total_algorithm_count"]),
        )
