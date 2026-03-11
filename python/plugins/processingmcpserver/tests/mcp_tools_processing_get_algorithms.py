from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProcessingGetAlgorithmsTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "processing_get_algorithms")

    def test_success_get_algorithms_limited(self):
        """验证 get algorithms limited 的成功场景。"""
        tools = self.build_tools()

        result = tools.processing_get_algorithms(limit=5)
        self.assertIn("count", result)
        self.assertIn("returned", result)
        self.assertLessEqual(int(result["returned"]), 5)

    def test_success_filter_by_provider_and_return_detail_payload(self):
        """验证 filter by provider and return detail payload 的成功场景。"""
        tools = self.build_tools()

        filtered = tools.processing_get_algorithms(provider_id="native", limit=5)
        detailed = tools.processing_get_algorithms(algorithm_id="native:buffer")

        self.assertGreater(filtered["returned"], 0)
        self.assertTrue(
            all(item["provider_id"] == "native" for item in filtered["algorithms"])
        )
        self.assertEqual(detailed["algorithm"]["id"], "native:buffer")
        self.assertIn("parameters", detailed["algorithm"])
        self.assertIn("outputs", detailed["algorithm"])

    def test_success_limit_cap_is_reported(self):
        """验证 limit cap is reported 的成功场景。"""
        tools = self.build_tools()

        result = tools.processing_get_algorithms(
            limit=tools.MAX_ALGORITHM_LIST_LIMIT + 1
        )

        self.assertEqual(result["applied_limit"], tools.MAX_ALGORITHM_LIST_LIMIT)
        self.assertTrue(result["limit_capped"])

    def test_failure_unknown_algorithm_id(self):
        """验证 unknown algorithm ID 的失败场景。"""
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.processing_get_algorithms(algorithm_id="not-exist:algorithm")
        self.assertIn("Algorithm not found", str(ctx.exception))
