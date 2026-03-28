from __future__ import annotations

from qgis.core import (
    QgsFeature,
    QgsGeometry,
    QgsProject,
    QgsVectorFileWriter,
    QgsVectorLayer,
)

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorExportShapefileTest(ProcessingMCPTestBase):
    def test_registered(self):
        """
        作用：执行测试用例 `registered`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `registered`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        assert_tool_registered(self, "vector_export_shapefile")

    def test_success_export_with_auto_truncated_field_names(self):
        """
        作用：执行测试用例 `success export with auto truncated field names`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success export with auto truncated field names`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = QgsVectorLayer(
            "Point?crs=EPSG:4326&field=very_long_field_name:string(32)",
            "export_long_field",
            "memory",
        )
        self.assertTrue(layer.isValid())
        provider = layer.dataProvider()
        feature = QgsFeature(layer.fields())
        feature.setAttributes(["alpha"])
        feature.setGeometry(QgsGeometry.fromWkt("POINT(108.9000 34.2000)"))
        add_result = provider.addFeatures([feature])
        self.assertTrue(add_result[0] if isinstance(add_result, tuple) else add_result)
        layer.updateExtents()
        QgsProject.instance().addMapLayer(layer)
        output_dir = self.make_temp_dir()

        result = tools.vector_export_shapefile(
            layer_ref=layer.id(),
            output_directory=str(output_dir),
            file_name="stable_output",
            task_name="export-task",
            overwrite=False,
            auto_truncate_field_names=True,
            confirm_write=True,
        )

        self.assertTrue((output_dir / "stable_output.shp").exists())
        self.assertTrue(result["outputs"]["field_name_mapping"])
        self.assertEqual(
            result["outputs"]["final_report"]["field_name_length_gt_10"],
            [],
        )
        self.assertTrue(
            QgsVectorFileWriter.deleteShapeFile(str(output_dir / "stable_output.shp"))
        )

    def test_failure_overwrite_requires_confirm_destructive(self):
        """
        作用：执行测试用例 `failure overwrite requires confirm destructive`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure overwrite requires confirm destructive`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("export_overwrite_guard")
        output_dir = self.make_temp_dir()

        tools.vector_export_shapefile(
            layer_ref=layer.id(),
            output_directory=str(output_dir),
            file_name="guarded",
            confirm_write=True,
        )

        with self.assertRaises(Exception) as ctx:
            tools.vector_export_shapefile(
                layer_ref=layer.id(),
                output_directory=str(output_dir),
                file_name="guarded",
                overwrite=True,
                confirm_write=True,
                confirm_destructive=False,
            )
        self.assertIn("confirm_destructive", str(ctx.exception))
        self.assertTrue(QgsVectorFileWriter.deleteShapeFile(str(output_dir / "guarded.shp")))
