from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProcessingGetParameterTemplateTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "processing_get_parameter_template")

    def test_success_get_parameter_template(self):
        tools = self.build_tools()

        result = tools.processing_get_parameter_template("native:buffer")
        self.assertIn("algorithm", result)
        self.assertIn("required_parameters", result)
        self.assertIn("optional_parameters", result)
        self.assertIn("outputs", result)

    def test_failure_unknown_algorithm_id(self):
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.processing_get_parameter_template("not-exist:algorithm")
        self.assertIn("Algorithm not found", str(ctx.exception))
