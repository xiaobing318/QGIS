from __future__ import annotations

from pathlib import Path

from qgis.core import QgsVectorFileWriter

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorExportShapefileTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "mcp_tools_vector_export_dataset")

    def test_default_temporary_output(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("export_temp_default")

        result = tools.mcp_tools_vector_export_dataset(layer_ref=layer.id())
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "mcp_tools_vector_export_dataset")

    def test_shp_export_requires_confirm_write(self):
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("export_shp_guard")
        output_dir = self.make_temp_dir()
        target = output_dir / "guarded.shp"

        fail = tools.mcp_tools_vector_export_dataset(
            layer_ref=layer.id(),
            output_path=str(target),
            file_format="shp",
            overwrite=True,
            confirm_write=False,
            confirm_destructive=False,
        )
        self.assertFalse(fail["ok"])

        ok = tools.mcp_tools_vector_export_dataset(
            layer_ref=layer.id(),
            output_path=str(target),
            file_format="shp",
            overwrite=True,
            confirm_write=True,
            confirm_destructive=True,
        )
        self.assertTrue(ok["ok"])
        self.assertTrue(Path(target).exists())
        self.assertTrue(QgsVectorFileWriter.deleteShapeFile(str(target)))
