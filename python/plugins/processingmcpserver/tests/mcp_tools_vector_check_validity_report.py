from __future__ import annotations

from qgis.core import QgsFeature, QgsGeometry, QgsProject, QgsVectorLayer

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsVectorCheckValidityReportTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "vector_check_validity_report")

    def test_success_clean_layer(self):
        """验证 clean layer 的成功场景。"""
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("validity_clean")

        result = tools.vector_check_validity_report(
            layer_ref=layer.id(),
            required_fields=["name", "value"],
            expected_crs="EPSG:4326",
        )

        report = result["outputs"]["report"]
        self.assertTrue(report["safe_for_export"])
        self.assertEqual(report["invalid_geometry_count"], 0)
        self.assertEqual(report["missing_required_fields"], [])
        self.assertEqual(report["crs"], "EPSG:4326")

    def test_detects_duplicates_and_long_field_names(self):
        """验证 duplicates and long field names 的成功场景。"""
        tools = self.build_tools()
        layer = QgsVectorLayer(
            "Point?crs=EPSG:4326&field=very_long_field_name:string(32)",
            "validity_dirty",
            "memory",
        )
        self.assertTrue(layer.isValid())
        provider = layer.dataProvider()

        features: list[QgsFeature] = []
        for value in ("alpha", "alpha"):
            feature = QgsFeature(layer.fields())
            feature.setAttributes([value])
            feature.setGeometry(QgsGeometry.fromWkt("POINT(108.9000 34.2000)"))
            features.append(feature)
        add_result = provider.addFeatures(features)
        self.assertTrue(add_result[0] if isinstance(add_result, tuple) else add_result)
        layer.updateExtents()
        QgsProject.instance().addMapLayer(layer)

        result = tools.vector_check_validity_report(layer_ref=layer.id())

        report = result["outputs"]["report"]
        self.assertEqual(report["duplicate_geometry_count"], 1)
        self.assertEqual(report["duplicate_record_count"], 1)
        self.assertIn("very_long_field_name", report["field_name_length_gt_10"])

