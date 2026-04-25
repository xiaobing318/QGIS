from __future__ import annotations

from processingmcpserver.mcp_tools import register_tools

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, assert_tool_registered


class ToolsDatasetLoadFromDirectoryTest(ProcessingMCPTestBase):
    def test_removed_dataset_load_from_directory_not_registered(self):
        mcp = DummyMcp()
        register_tools(mcp, self.build_tools(), enable_execute_code=False)
        self.assertNotIn("dataset_load_from_directory", set(mcp.tool_names))

    def test_dataset_load_layers_registered(self):
        assert_tool_registered(self, "mcp_tools_dataset_load_layers")

    def test_dataset_load_layers_from_directory(self):
        tools = self.build_tools()
        temp_root = self.make_temp_dir()
        self.copy_test_data_file("sample_vector.geojson", temp_root, "dataset_load.geojson")

        result = tools.mcp_tools_dataset_load_layers(
            directory=str(temp_root),
            recursive=False,
            dataset_kind="vector",
            geometry_type="any",
            name_glob="*.geojson",
            limit=100,
            skip_invalid=True,
        )

        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["requested_count"], 1)
        self.assertEqual(result["summary"]["loaded_count"], 1)
