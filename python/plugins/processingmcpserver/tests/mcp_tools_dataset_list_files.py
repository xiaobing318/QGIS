from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsDatasetListFilesTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "dataset_list_files")

    def test_success_dataset_scan(self):
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        vector_file = self.copy_test_data_file(
            "sample_vector.geojson", temp_root, "dataset.geojson"
        )
        (temp_root / "note.txt").write_text("hello", encoding="utf-8")

        result = tools.dataset_list_files(
            directory=str(vector_file.parent),
            recursive=False,
            dataset_kind="both",
            geometry_type="any",
            name_glob="*",
            limit=100,
        )
        self.assertEqual(result["returned"], 1)
        self.assertEqual(result["datasets"][0]["dataset_kind"], "vector")

    def test_failure_missing_directory(self):
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.dataset_list_files(directory="C:/not-exist", recursive=False)
        self.assertIn("Directory not found", str(ctx.exception))
