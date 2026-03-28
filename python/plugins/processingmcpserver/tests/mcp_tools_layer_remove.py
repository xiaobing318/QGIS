from __future__ import annotations

from qgis.core import QgsProject

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerRemoveTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the expected capability is registered."""
        assert_tool_registered(self, "layer_remove")

    def test_success_remove_existing_layer(self):
        """Verify the successful path for removing an existing layer."""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("remove_vector")
        layer_id = layer.id()

        result = tools.layer_remove(layer_id=layer_id)
        self.assertEqual(result["removed"], layer_id)
        self.assertNotIn(layer_id, QgsProject.instance().mapLayers())

    def test_failure_missing_layer(self):
        """Verify the failure path for a missing layer."""
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.layer_remove(layer_id="missing-layer")
        self.assertIn("Layer not found", str(ctx.exception))
