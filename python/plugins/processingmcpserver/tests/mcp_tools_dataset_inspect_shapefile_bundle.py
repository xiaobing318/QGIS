from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsDatasetInspectShapefileBundleTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the expected capability is registered."""
        assert_tool_registered(self, "dataset_inspect_shapefile_bundle")

    def test_success_inspect_complete_bundle(self):
        """Verify the successful path for inspecting a complete bundle."""
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        layer = self.add_sample_vector_layer("shapefile_bundle_complete")
        shp_path = self.export_layer_to_shapefile(layer, temp_root, "complete_bundle")

        result = tools.dataset_inspect_shapefile_bundle(path=str(shp_path))

        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["returned"], 1)
        bundle = result["outputs"]["bundles"][0]
        self.assertTrue(bundle["complete_bundle"])
        self.assertEqual(bundle["geometry_type"], "point")
        self.assertEqual(bundle["missing_required"], [])

    def test_detects_missing_required_sidecar(self):
        """Verify that a missing required sidecar is reported."""
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        layer = self.add_sample_vector_layer("shapefile_bundle_missing_sidecar")
        shp_path = self.export_layer_to_shapefile(layer, temp_root, "broken_bundle")
        shx_path = shp_path.with_suffix(".shx")
        self.assertTrue(shx_path.exists())
        shx_path.unlink()

        result = tools.dataset_inspect_shapefile_bundle(path=str(shp_path))

        bundle = result["outputs"]["bundles"][0]
        self.assertFalse(bundle["complete_bundle"])
        self.assertIn("shx", bundle["missing_required"])
