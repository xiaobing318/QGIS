from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerResolveReferencesTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "layer_resolve_references")

    def test_success_reports_ambiguous_names(self):
        tools = self.build_tools()
        layer_a = self.add_sample_vector_layer("duplicate-layer-name")
        layer_b = self.add_sample_vector_layer("duplicate-layer-name")

        result = tools.layer_resolve_references(
            refs=["duplicate-layer-name", layer_a.id()],
            strict=False,
        )

        self.assertEqual(result["resolved"][layer_a.id()], layer_a.id())
        self.assertEqual(
            sorted(result["ambiguous"]["duplicate-layer-name"]),
            sorted([layer_a.id(), layer_b.id()]),
        )
        self.assertNotIn("duplicate-layer-name", result["resolved"])

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

    def test_failure_strict_with_ambiguous_name(self):
        tools = self.build_tools()
        self.add_sample_vector_layer("duplicate-layer-strict")
        self.add_sample_vector_layer("duplicate-layer-strict")

        with self.assertRaises(Exception) as ctx:
            tools.layer_resolve_references(
                refs=["duplicate-layer-strict"],
                strict=True,
            )
        self.assertIn("ambiguous", str(ctx.exception))
