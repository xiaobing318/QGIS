from __future__ import annotations

from qgis.core import QgsFeature, QgsGeometry, QgsProject, QgsVectorLayer

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorPrepareWorkLayerTest(ProcessingMCPTestBase):
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
        assert_tool_registered(self, "vector_prepare_work_layer")

    def test_prepare_work_layer_standardizes_geometry_crs_and_schema(self):
        """
        作用：执行测试用例 `prepare work layer standardizes geometry crs and schema`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `prepare work layer standardizes geometry crs and schema`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = QgsVectorLayer(
            "MultiPoint?crs=EPSG:4326&field=very_long_field_name:string(32)",
            "prepare_dirty",
            "memory",
        )
        self.assertTrue(layer.isValid())
        provider = layer.dataProvider()

        multipart = QgsFeature(layer.fields())
        multipart.setAttributes(["alpha"])
        multipart.setGeometry(
            QgsGeometry.fromWkt("MULTIPOINT((108.9000 34.2000),(108.9100 34.2100))")
        )
        duplicate = QgsFeature(layer.fields())
        duplicate.setAttributes(["alpha"])
        duplicate.setGeometry(
            QgsGeometry.fromWkt("MULTIPOINT((108.9000 34.2000),(108.9100 34.2100))")
        )
        empty = QgsFeature(layer.fields())
        empty.setAttributes(["empty"])
        add_result = provider.addFeatures([multipart, duplicate, empty])
        self.assertTrue(add_result[0] if isinstance(add_result, tuple) else add_result)
        layer.updateExtents()
        QgsProject.instance().addMapLayer(layer)

        result = tools.vector_prepare_work_layer(
            layer_ref=layer.id(),
            task_name="prepare-task",
            target_crs="EPSG:3857",
            normalize_field_names=True,
            multipart_policy="singleparts",
        )

        prepared_layer = self.project_layer(result["summary"]["output_layer_id"])
        self.assertIsNotNone(prepared_layer)
        self.assertEqual(result["summary"]["removed_empty_geometry_count"], 1)
        self.assertEqual(result["summary"]["removed_duplicate_geometry_count"], 1)
        self.assertEqual(result["outputs"]["final_report"]["multipart_feature_count"], 0)
        self.assertEqual(result["outputs"]["final_report"]["crs"], "EPSG:3857")
        self.assertTrue(result["outputs"]["field_name_mapping"])
        self.assertEqual(prepared_layer.featureCount(), 2)

    def test_failure_invalid_multipart_policy(self):
        """
        作用：执行测试用例 `failure invalid multipart policy`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure invalid multipart policy`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("prepare_invalid_policy")

        with self.assertRaises(Exception) as ctx:
            tools.vector_prepare_work_layer(layer_ref=layer.id(), multipart_policy="explode")
        self.assertIn("multipart_policy", str(ctx.exception))
