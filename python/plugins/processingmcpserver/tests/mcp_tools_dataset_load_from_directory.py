from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsDatasetLoadFromDirectoryTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "dataset_load_from_directory")

    def test_success_dataset_load_vector(self):
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        vector_file = self.copy_test_data_file(
            "sample_vector.geojson", temp_root, "dataset_load.geojson"
        )

        result = tools.dataset_load_from_directory(
            directory=str(vector_file.parent),
            recursive=False,
            dataset_kind="vector",
            geometry_type="any",
            name_glob="*.geojson",
            limit=100,
            skip_invalid=True,
        )
        self.assertEqual(result["requested_count"], 1)
        self.assertEqual(result["loaded_count"], 1)
        self.assertEqual(result["failed_count"], 0)

    def test_failure_missing_directory(self):
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.dataset_load_from_directory(directory="C:/not-exist")
        self.assertIn("Directory not found", str(ctx.exception))
