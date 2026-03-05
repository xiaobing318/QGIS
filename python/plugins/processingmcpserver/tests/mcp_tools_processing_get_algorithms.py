from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProcessingGetAlgorithmsTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "processing_get_algorithms")

    def test_success_get_algorithms_limited(self):
        tools = self.build_tools()

        result = tools.processing_get_algorithms(limit=5)
        self.assertIn("count", result)
        self.assertIn("returned", result)
        self.assertLessEqual(int(result["returned"]), 5)

    def test_failure_unknown_algorithm_id(self):
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.processing_get_algorithms(algorithm_id="not-exist:algorithm")
        self.assertIn("Algorithm not found", str(ctx.exception))
