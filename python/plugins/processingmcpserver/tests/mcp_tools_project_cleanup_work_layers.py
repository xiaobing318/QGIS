from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProjectCleanupWorkLayersTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the expected capability is registered."""
        assert_tool_registered(self, "project_cleanup_work_layers")

    def test_cleanup_removes_work_layers_and_temp_files(self):
        """Verify that cleanup removes work layers and temporary files."""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("cleanup_source")
        prepared = tools.vector_prepare_work_layer(
            layer_ref=layer.id(),
            task_name="cleanup-task",
        )
        work_layer_id = prepared["summary"]["output_layer_id"]
        output_dir = self.make_temp_dir()
        shapefile_path = self.export_layer_to_shapefile(layer, output_dir, "cleanup_temp")

        result = tools.project_cleanup_work_layers(
            task_name="cleanup-task",
            temp_paths=[str(shapefile_path)],
            delete_temp_files=True,
            confirm_write=True,
            confirm_destructive=True,
        )

        self.assertIn(work_layer_id, result["outputs"]["removed_layer_ids"])
        self.assertIsNone(self.project_layer(work_layer_id))
        self.assertFalse(shapefile_path.exists())
