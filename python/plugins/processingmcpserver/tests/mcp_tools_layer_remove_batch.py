from __future__ import annotations

from qgis.core import QgsProject

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerRemoveBatchTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "layer_remove_batch")

    def test_success_remove_and_report_missing(self):
        tools = self.build_tools()
        layer1 = self.add_sample_vector_layer("remove_batch_vector_1")
        layer2 = self.add_sample_vector_layer("remove_batch_vector_2")
        layer1_id = layer1.id()
        layer2_id = layer2.id()

        result = tools.layer_remove_batch([layer1_id, "missing", layer2_id])
        self.assertIn(layer1_id, result["removed"])
        self.assertIn(layer2_id, result["removed"])
        self.assertIn("missing", result["missing"])
        self.assertNotIn(layer1_id, QgsProject.instance().mapLayers())
        self.assertNotIn(layer2_id, QgsProject.instance().mapLayers())

    def test_safety_ignore_empty_ids(self):
        tools = self.build_tools()
        result = tools.layer_remove_batch(["", "   "])
        self.assertEqual(result["removed"], [])
        self.assertEqual(result["missing"], [])
