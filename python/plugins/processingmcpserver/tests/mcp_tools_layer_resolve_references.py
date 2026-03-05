from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerResolveReferencesTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "layer_resolve_references")

    def test_success_non_strict_reports_missing(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("resolve_ref_vector")

        result = tools.layer_resolve_references(
            refs=[layer.id(), "missing-layer"],
            strict=False,
        )
        self.assertEqual(result["resolved"][layer.id()], layer.id())
        self.assertIn("missing-layer", result["missing"])

    def test_failure_strict_with_missing(self):
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.layer_resolve_references(refs=["missing-layer"], strict=True)
        self.assertIn("Layer reference resolve failed", str(ctx.exception))
