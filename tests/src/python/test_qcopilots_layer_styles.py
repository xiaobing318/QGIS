"""Tests for QCopilots layer style loading and direct vector renderers.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "Ethan.Yang"
__date__ = "2026-11-01"
__copyright__ = "Copyright 2026, The QGIS Project"

import gc
import os
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))

_QGIS_DLL_DIRECTORY_HANDLES = []


def _prepare_windows_qgis_dll_search_path():
    """Add the active multi-config QGIS build directory before QGIS imports."""

    if sys.platform != "win32":
        return

    prefix_path = os.environ.get("QGIS_PREFIX_PATH", "")
    bin_root = Path(prefix_path).parent if prefix_path else None
    if bin_root is None or not bin_root.is_dir():
        return

    for candidate in sorted(bin_root.iterdir()):
        if not (candidate / "qgis_core.dll").is_file():
            continue
        os.environ["PATH"] = f"{candidate}{os.pathsep}{os.environ.get('PATH', '')}"
        if hasattr(os, "add_dll_directory"):
            _QGIS_DLL_DIRECTORY_HANDLES.append(os.add_dll_directory(str(candidate)))
        return


_prepare_windows_qgis_dll_search_path()

AREAS = [float(index * index) for index in range(10)]


def _sld_document(field, boundaries):
    rules = []
    colors = ["#fee5d9", "#fb6a4a", "#a50f15", "#67000d"]
    for index, (lower, upper) in enumerate(zip(boundaries, boundaries[1:])):
        rules.append(
            f"""      <Rule>
        <Name>class{index + 1}</Name>
        <Title>{lower} - {upper}</Title>
        <ogc:Filter>
          <ogc:And>
            <ogc:PropertyIsGreaterThanOrEqualTo>
              <ogc:PropertyName>{field}</ogc:PropertyName>
              <ogc:Literal>{lower}</ogc:Literal>
            </ogc:PropertyIsGreaterThanOrEqualTo>
            <ogc:PropertyIsLessThan>
              <ogc:PropertyName>{field}</ogc:PropertyName>
              <ogc:Literal>{upper}</ogc:Literal>
            </ogc:PropertyIsLessThan>
          </ogc:And>
        </ogc:Filter>
        <PolygonSymbolizer>
          <Fill><CssParameter name="fill">{colors[index]}</CssParameter></Fill>
        </PolygonSymbolizer>
      </Rule>"""
        )
    body = "\n".join(rules)
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<StyledLayerDescriptor version="1.0.0"
    xmlns="http://www.opengis.net/sld"
    xmlns:ogc="http://www.opengis.net/ogc"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <NamedLayer>
    <Name>parcels</Name>
    <UserStyle>
      <Title>area classes</Title>
      <FeatureTypeStyle>
{body}
      </FeatureTypeStyle>
    </UserStyle>
  </NamedLayer>
</StyledLayerDescriptor>
"""


class TestQCopilotsLayerStyles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from qgis.testing import start_app

        start_app()

    def setUp(self):
        from qgis.core import QgsProject
        from qcopilots_common.bridge import QgisBridgeTools

        self.directory = tempfile.TemporaryDirectory(
            prefix="qcopilots-layer-styles-", ignore_cleanup_errors=True
        )
        self.addCleanup(self._cleanup_directory)
        self.root = Path(self.directory.name)
        self.layer = self._memory_layer("QCopilots style test")
        QgsProject.instance().addMapLayer(self.layer)
        self.addCleanup(self._remove_layer, self.layer.id())
        self.tools = QgisBridgeTools(None)

    def _cleanup_directory(self):
        gc.collect()
        self.directory.cleanup()

    def _remove_layer(self, layer_id):
        from qgis.core import QgsProject

        if QgsProject.instance().mapLayer(layer_id) is not None:
            QgsProject.instance().removeMapLayer(layer_id)

    def _memory_layer(self, name):
        from qgis.core import QgsFeature, QgsGeometry, QgsVectorLayer

        layer = QgsVectorLayer(
            "Polygon?crs=EPSG:3857&field=name:string(20)&field=pop:integer"
            "&field=area:double",
            name,
            "memory",
        )
        features = []
        for index, area in enumerate(AREAS):
            feature = QgsFeature(layer.fields())
            feature.setGeometry(
                QgsGeometry.fromWkt(
                    f"POLYGON(({index} 0,{index + 1} 0,{index + 1} 1,"
                    f"{index} 1,{index} 0))"
                )
            )
            feature.setAttributes([f"n{index % 3}", index * 10, area])
            features.append(feature)
        self.assertTrue(layer.dataProvider().addFeatures(features)[0])
        layer.updateExtents()
        self.assertTrue(layer.isValid())
        return layer

    def _set_single_symbol(self, layer, color="#123456"):
        from qgis.PyQt.QtGui import QColor
        from qgis.core import QgsSingleSymbolRenderer, QgsSymbol

        symbol = QgsSymbol.defaultSymbol(layer.geometryType())
        symbol.setColor(QColor(color))
        layer.setRenderer(QgsSingleSymbolRenderer(symbol))

    def _assert_single_symbol_kept(self, layer, color="#123456"):
        renderer = layer.renderer()
        self.assertEqual(renderer.type(), "singleSymbol")
        self.assertEqual(renderer.symbol().color().name(), color)

    def _graduated_qml(self, name="graduated.qml"):
        """Export a QGIS graduated QML with five equal-interval classes."""
        from qgis.core import (
            QgsApplication,
            QgsGraduatedSymbolRenderer,
            QgsStyle,
            QgsSymbol,
        )

        source = self._memory_layer("QCopilots QML export")
        renderer = QgsGraduatedSymbolRenderer("area")
        renderer.setSourceSymbol(QgsSymbol.defaultSymbol(source.geometryType()))
        renderer.setClassificationMethod(
            QgsApplication.classificationMethodRegistry().method("EqualInterval")
        )
        self.assertEqual(renderer.updateClasses(source, 5), "")
        renderer.updateColorRamp(QgsStyle.defaultStyle().colorRamp("Reds"))
        source.setRenderer(renderer)
        path = self.root / name
        message, saved = source.saveNamedStyle(str(path))
        self.assertTrue(saved, message)
        return path

    def _apply(self, layer, path):
        return self.tools.interactive_layer_apply_style(
            {"layer_id": layer.id(), "style_path": str(path)}
        )

    def _renderer(self, **arguments):
        return self.tools.interactive_layer_set_renderer(
            {"layer_id": self.layer.id(), **arguments}
        )

    # ------------------------------------------------------------ apply_layer_style

    def test_sld_file_loads_rule_based_renderer(self):
        self._set_single_symbol(self.layer)
        path = self.root / "area.sld"
        path.write_text(_sld_document("area", [0, 20, 50, 100]), encoding="utf-8")

        result = self._apply(self.layer, path)

        self.assertTrue(result["loaded"])
        self.assertEqual(result["style_format"], "sld")
        self.assertEqual(self.layer.renderer().type(), "RuleRenderer")
        self.assertEqual(result["renderer"]["type"], "RuleRenderer")
        self.assertEqual(result["renderer"]["class_count"], 3)
        self.assertIn("area", result["renderer"]["rules"][0]["filter"])
        self.assertEqual(result["renderer"]["rules"][0]["color"], "#fee5d9")

    def _label_by_name(self):
        result = self.tools.interactive_layer_configure_labels(
            {"layer_id": self.layer.id(), "enabled": True, "field_or_expression": "name"}
        )
        self.assertTrue(result["labels_enabled"])

    def _assert_labels_by_name(self, labeling):
        self.assertTrue(self.layer.labelsEnabled())
        self.assertEqual(self.layer.labeling().settings().fieldName, "name")
        self.assertTrue(labeling["enabled"])
        self.assertEqual(labeling["field_or_expression"], "name")

    def test_label_color_alpha_becomes_text_opacity(self):
        result = self.tools.interactive_layer_configure_labels(
            {
                "layer_id": self.layer.id(),
                "field_or_expression": "  name  ",
                "color": "#33669980",
            }
        )
        self.assertEqual(result["field_or_expression"], "name")
        text_format = self.layer.labeling().settings().format()
        # QgsTextRenderer replaces the color alpha with the format opacity,
        # so the alpha must arrive as the opacity to be drawn at all.
        self.assertEqual(text_format.color().name(), "#336699")
        self.assertEqual(text_format.color().alpha(), 255)
        self.assertAlmostEqual(text_format.opacity(), 0x80 / 255, places=3)

        opaque = self.tools.interactive_layer_configure_labels(
            {"layer_id": self.layer.id(), "field_or_expression": "name"}
        )
        self.assertTrue(opaque["labels_enabled"])
        self.assertAlmostEqual(
            self.layer.labeling().settings().format().opacity(), 1.0
        )
        with self.assertRaisesRegex(RuntimeError, "#RRGGBBAA"):
            self.tools.interactive_layer_configure_labels(
                {
                    "layer_id": self.layer.id(),
                    "field_or_expression": "name",
                    "color": "red",
                }
            )

        disabled = self.tools.interactive_layer_configure_labels(
            {"layer_id": self.layer.id(), "enabled": False}
        )
        self.assertFalse(disabled["labels_enabled"])
        self.assertIsNone(disabled["field_or_expression"])
        self.assertFalse(disabled["is_expression"])

    def test_sld_without_text_symbolizer_keeps_current_labels(self):
        self._set_single_symbol(self.layer)
        self._label_by_name()
        path = self.root / "area.sld"
        path.write_text(_sld_document("area", [0, 20, 50, 100]), encoding="utf-8")

        result = self._apply(self.layer, path)

        self.assertEqual(self.layer.renderer().type(), "RuleRenderer")
        self._assert_labels_by_name(result["labeling"])
        self.assertEqual(result["labeling"]["source"], "kept_previous")

    def test_sld_with_text_symbolizer_replaces_labels(self):
        self._set_single_symbol(self.layer)
        self._label_by_name()
        document = _sld_document("area", [0, 20, 50, 100]).replace(
            "</PolygonSymbolizer>",
            "</PolygonSymbolizer>\n        <TextSymbolizer><Label>"
            "<ogc:PropertyName>pop</ogc:PropertyName></Label></TextSymbolizer>",
            1,
        )
        path = self.root / "area-labels.sld"
        path.write_text(document, encoding="utf-8")

        result = self._apply(self.layer, path)

        self.assertEqual(result["labeling"]["source"], "style_file")
        self.assertNotEqual(result["labeling"].get("field_or_expression"), "name")

    def test_qml_without_labeling_keeps_current_labels(self):
        from qgis.core import QgsMapLayer

        self._set_single_symbol(self.layer)
        self._label_by_name()
        source = self._memory_layer("QCopilots symbology export")
        self._set_single_symbol(source, "#654321")
        path = self.root / "symbology-only.qml"
        message, saved = source.saveNamedStyle(
            str(path), QgsMapLayer.StyleCategory.Symbology
        )
        self.assertTrue(saved, message)

        result = self._apply(self.layer, path)

        self._assert_single_symbol_kept(self.layer, "#654321")
        self.assertEqual(result["style_categories"], ["symbology"])
        self._assert_labels_by_name(result["labeling"])
        self.assertEqual(result["labeling"]["source"], "kept_previous")

    def test_set_vector_renderer_reports_labeling(self):
        self._label_by_name()

        result = self._renderer(mode="single", color="#336699")

        self._assert_labels_by_name(result["labeling"])

    def test_qgis_exported_graduated_qml_loads_five_ranges(self):
        self._set_single_symbol(self.layer)
        path = self._graduated_qml()

        result = self._apply(self.layer, path)

        renderer = self.layer.renderer()
        self.assertEqual(renderer.type(), "graduatedSymbol")
        self.assertEqual(len(renderer.ranges()), 5)
        self.assertEqual(result["style_format"], "qml")
        self.assertEqual(result["style_categories"], ["symbology", "labeling"])
        summary = result["renderer"]
        self.assertEqual(summary["type"], "graduatedSymbol")
        self.assertEqual(summary["field"], "area")
        self.assertEqual(summary["class_count"], 5)
        self.assertEqual(len(summary["ranges"]), 5)
        self.assertAlmostEqual(summary["ranges"][0]["lower"], 0.0)
        self.assertAlmostEqual(summary["ranges"][-1]["upper"], 81.0)
        self.assertTrue(all(item["color"] for item in summary["ranges"]))

    def test_qml_with_symbolid_is_rejected_and_previous_renderer_kept(self):
        text = self._graduated_qml().read_text(encoding="utf-8")
        bad_text = re.sub(
            r'(<range\b[^>]*?)\bsymbol="(\d+)"',
            lambda match: f'{match.group(1)}symbolid="sym{match.group(2)}"',
            text,
        )
        self.assertNotEqual(bad_text, text)
        bad_path = self.root / "graduated_symbolid.qml"
        bad_path.write_text(bad_text, encoding="utf-8")
        self._set_single_symbol(self.layer)

        with self.assertRaisesRegex(
            RuntimeError, r'symbol="0".*<symbol name="0">'
        ) as caught:
            self._apply(self.layer, bad_path)

        self.assertIn("symbolid", str(caught.exception))
        self._assert_single_symbol_kept(self.layer)

    def test_qml_with_unmatched_category_symbol_is_rejected(self):
        from qgis.core import (
            QgsCategorizedSymbolRenderer,
            QgsRendererCategory,
            QgsSymbol,
        )

        source = self._memory_layer("QCopilots categorized export")
        geometry_type = source.geometryType()
        categories = [
            QgsRendererCategory(value, QgsSymbol.defaultSymbol(geometry_type), value)
            for value in ("n0", "n1", "n2")
        ]
        source.setRenderer(QgsCategorizedSymbolRenderer("name", categories))
        path = self.root / "categorized.qml"
        self.assertTrue(source.saveNamedStyle(str(path))[1])
        text = path.read_text(encoding="utf-8")
        bad_text = re.sub(r'(<category\b[^>]*?)\bsymbol="1"', r'\1symbol="99"', text)
        self.assertNotEqual(bad_text, text)
        path.write_text(bad_text, encoding="utf-8")
        self._set_single_symbol(self.layer)

        with self.assertRaisesRegex(RuntimeError, r'references symbol="99"'):
            self._apply(self.layer, path)

        self._assert_single_symbol_kept(self.layer)

    def test_qml_with_unknown_renderer_type_is_rejected(self):
        text = self._graduated_qml().read_text(encoding="utf-8")
        bad_text = re.sub(
            r'(<renderer-v2\b[^>]*?)type="graduatedSymbol"',
            r'\1type="graduated"',
            text,
            count=1,
        )
        self.assertNotEqual(bad_text, text)
        bad_path = self.root / "graduated_unknown_type.qml"
        bad_path.write_text(bad_text, encoding="utf-8")
        self._set_single_symbol(self.layer)

        with self.assertRaisesRegex(
            RuntimeError, r'Unknown renderer type.*type="graduated".*graduatedSymbol'
        ):
            self._apply(self.layer, bad_path)

        self._assert_single_symbol_kept(self.layer)

    def test_qml_structure_errors_are_reported(self):
        self._set_single_symbol(self.layer)
        cases = {
            "sld_in_qml.qml": (
                _sld_document("area", [0, 50, 100]),
                r"<qgis> root element.*\.sld suffix",
            ),
            "renderer_tag.qml": (
                '<qgis><renderer type="singleSymbol"/></qgis>',
                r"<renderer-v2",
            ),
            "not_xml.qml": ("<qgis><renderer-v2", r"not well-formed XML"),
        }
        for name, (text, pattern) in cases.items():
            with self.subTest(file=name):
                path = self.root / name
                path.write_text(text, encoding="utf-8")
                with self.assertRaisesRegex(RuntimeError, pattern):
                    self._apply(self.layer, path)
                self._assert_single_symbol_kept(self.layer)

    def test_rule_renderer_without_rules_is_restored_after_loading(self):
        self._set_single_symbol(self.layer)
        self.layer.setLabelsEnabled(False)
        path = self.root / "empty_rules.qml"
        path.write_text(
            '<qgis version="4.2.0"><renderer-v2 type="RuleRenderer">'
            '<rules key="root"/><symbols/></renderer-v2>'
            "<labeling type=\"simple\"/></qgis>",
            encoding="utf-8",
        )

        with self.assertRaisesRegex(
            RuntimeError,
            r"(rule-based renderer has no rules|did not apply).*previous renderer",
        ):
            self._apply(self.layer, path)

        self._assert_single_symbol_kept(self.layer)
        self.assertFalse(self.layer.labelsEnabled())

    def test_qml_file_wins_over_geopackage_default_style(self):
        from qgis.PyQt.QtCore import QMetaType
        from qgis.core import (
            QgsCategorizedSymbolRenderer,
            QgsField,
            QgsProject,
            QgsRendererCategory,
            QgsSymbol,
            QgsVectorFileWriter,
            QgsVectorLayer,
        )

        gpkg_path = self.root / "parcels.gpkg"
        options = QgsVectorFileWriter.SaveVectorOptions()
        options.driverName = "GPKG"
        options.layerName = "parcels"
        written = QgsVectorFileWriter.writeAsVectorFormatV3(
            self.layer,
            str(gpkg_path),
            QgsProject.instance().transformContext(),
            options,
        )
        self.assertEqual(written[0], QgsVectorFileWriter.WriterError.NoError, written)
        uri = f"{gpkg_path}|layername=parcels"

        stored = QgsVectorLayer(uri, "stored style", "ogr")
        self.assertTrue(stored.isValid())
        geometry_type = stored.geometryType()
        categories = [
            QgsRendererCategory(value, QgsSymbol.defaultSymbol(geometry_type), value)
            for value in ("n0", "n1", "n2")
        ]
        stored.setRenderer(QgsCategorizedSymbolRenderer("name", categories))
        _result, message = stored.saveStyleToDatabaseV2("stored", "", True, "")
        self.assertEqual(message, "")
        del stored

        layer = QgsVectorLayer(uri, "gpkg parcels", "ogr")
        self.assertTrue(layer.isValid())
        QgsProject.instance().addMapLayer(layer)
        self.addCleanup(self._remove_layer, layer.id())
        self.assertGreaterEqual(layer.listStylesInDatabase()[0], 1)
        # The stored default style is what QGIS applied when the layer opened.
        self.assertEqual(layer.renderer().type(), "categorizedSymbol")
        layer.addExpressionField(
            '"area" * 2', QgsField("area_twice", QMetaType.Type.Double)
        )
        qml_path = self._graduated_qml()

        result = self._apply(layer, qml_path)

        self.assertNotEqual(result["message"], "Loaded from Provider")
        self.assertEqual(layer.renderer().type(), "graduatedSymbol")
        self.assertEqual(result["renderer"]["class_count"], 5)
        self.assertIn("area_twice", layer.fields().names())

        sld_path = self.root / "area.sld"
        sld_path.write_text(_sld_document("area", [0, 50, 100]), encoding="utf-8")
        result = self._apply(layer, sld_path)
        self.assertEqual(layer.renderer().type(), "RuleRenderer")
        self.assertEqual(result["renderer"]["class_count"], 2)
        self.assertIn("area_twice", layer.fields().names())

    def test_provider_style_message_is_treated_as_failure(self):
        from qgis.core import QgsVectorLayer

        self._set_single_symbol(self.layer)
        path = self._graduated_qml()
        with mock.patch.object(
            QgsVectorLayer,
            "loadNamedStyle",
            autospec=True,
            return_value=("Loaded from Provider", True),
        ):
            with self.assertRaisesRegex(RuntimeError, "stored in the data source"):
                self._apply(self.layer, path)
        self._assert_single_symbol_kept(self.layer)

    # ---------------------------------------------------------- set_vector_renderer

    def test_graduated_equal_interval_matches_field_range(self):
        result = self._renderer(
            mode="graduated", field="area", method="equal_interval", classes=5,
            color_ramp="Greens",
        )

        renderer = self.layer.renderer()
        self.assertEqual(renderer.type(), "graduatedSymbol")
        self.assertEqual(result["method"], "equal_interval")
        self.assertEqual(result["color_ramp"], "Greens")
        self.assertEqual(result["value_range"], [min(AREAS), max(AREAS)])
        summary = result["renderer"]
        self.assertEqual(summary["field"], "area")
        self.assertEqual(summary["classification_method"], "EqualInterval")
        self.assertEqual(summary["class_count"], 5)
        step = (max(AREAS) - min(AREAS)) / 5
        for index, item in enumerate(summary["ranges"]):
            self.assertAlmostEqual(item["lower"], min(AREAS) + index * step)
            self.assertAlmostEqual(item["upper"], min(AREAS) + (index + 1) * step)
        colors = [item["color"] for item in summary["ranges"]]
        self.assertEqual(len(set(colors)), 5)
        self.assertEqual(colors[0], "#f7fcf5")
        self.assertEqual(colors[-1], "#00441b")

    def test_graduated_quantile_and_numeric_expression(self):
        result = self._renderer(
            mode="graduated", field="area", method="quantile", classes=4
        )
        summary = result["renderer"]
        self.assertEqual(summary["classification_method"], "Quantile")
        self.assertEqual(summary["class_count"], 4)
        self.assertAlmostEqual(summary["ranges"][0]["lower"], min(AREAS))
        self.assertAlmostEqual(summary["ranges"][-1]["upper"], max(AREAS))
        uppers = [item["upper"] for item in summary["ranges"]]
        self.assertEqual(uppers, sorted(uppers))

        result = self._renderer(
            mode="graduated", field='"pop" * 2', classes=3, invert_ramp=True
        )
        self.assertEqual(result["renderer"]["field"], '"pop" * 2')
        self.assertEqual(result["value_range"], [0.0, 180.0])
        self.assertEqual(result["renderer"]["class_count"], 3)
        self.assertTrue(result["invert_ramp"])

    def test_graduated_explicit_breaks_override_method(self):
        result = self._renderer(
            mode="graduated", field="area", breaks=[0, 10, 50, 100], method="jenks"
        )
        summary = result["renderer"]
        self.assertEqual(result["method"], "breaks")
        self.assertEqual(summary["classification_method"], "Custom")
        self.assertEqual(
            [[item["lower"], item["upper"]] for item in summary["ranges"]],
            [[0.0, 10.0], [10.0, 50.0], [50.0, 100.0]],
        )
        self.assertNotIn("warnings", result)

        result = self._renderer(mode="graduated", field="area", breaks=[10, 50, 100])
        self.assertIn("not drawn", result["warnings"][0])

    def test_categorized_and_single_renderers(self):
        result = self._renderer(mode="categorized", field="name", color_ramp="Spectral")
        summary = result["renderer"]
        self.assertEqual(self.layer.renderer().type(), "categorizedSymbol")
        self.assertEqual(summary["field"], "name")
        values = [item["value"] for item in summary["categories"]]
        self.assertEqual(
            [value for value in values if value is not None], ["n0", "n1", "n2"]
        )
        self.assertTrue(all(item["color"] for item in summary["categories"]))

        result = self._renderer(mode="categorized", field='upper("name")')
        values = [item["value"] for item in result["renderer"]["categories"]]
        self.assertIn("N2", values)
        self.assertEqual(result["color_ramp"], "Spectral")

        refreshes = []
        canvas = mock.Mock()
        canvas.refresh.side_effect = lambda: refreshes.append(True)
        iface = mock.Mock()
        iface.mapCanvas.return_value = canvas
        from qcopilots_common.bridge import QgisBridgeTools

        result = QgisBridgeTools(iface).dispatch(
            "interactive_layer_set_renderer",
            {
                "layer_id": self.layer.id(),
                "mode": "single",
                "color": "#ff0000",
                "outline_color": "#000000",
                "opacity": 0.5,
            },
        )
        self.assertEqual(
            result["renderer"],
            {"type": "singleSymbol", "class_count": 1, "color": "#ff0000"},
        )
        symbol = self.layer.renderer().symbol()
        self.assertAlmostEqual(symbol.opacity(), 0.5)
        self.assertEqual(symbol.symbolLayer(0).strokeColor().name(), "#000000")
        self.assertEqual(refreshes, [True])

    def test_set_vector_renderer_validation_errors_keep_renderer(self):
        self._set_single_symbol(self.layer)
        graduated = {"mode": "graduated", "field": "area"}
        bad_classes = "classes must be an integer from 2 to 12"
        cases = [
            ({"mode": "categorized"}, "field is required"),
            ({"mode": "graduated", "field": "nope"}, "Unknown vector field: nope"),
            (
                {"mode": "graduated", "field": "area +"},
                "not a layer field or a valid expression",
            ),
            ({"mode": "graduated", "field": "name"}, "not numeric"),
            ({**graduated, "color_ramp": "NotARamp"}, "Unknown color_ramp: NotARamp"),
            ({**graduated, "classes": 1}, bad_classes),
            ({**graduated, "classes": 13}, bad_classes),
            ({**graduated, "classes": True}, bad_classes),
            ({**graduated, "method": "natural"}, "method must be"),
            ({**graduated, "breaks": [0, 50, 20]}, "strictly ascending"),
            ({**graduated, "breaks": [0, 50]}, "breaks must list 3 to 13 numbers"),
            (
                {**graduated, "color": "#ff0000"},
                "color cannot be used with mode graduated",
            ),
            ({"mode": "single", "field": "area"}, "field cannot be used with mode single"),
            ({"mode": "single", "color": "red"}, "color must be a #RRGGBB color"),
            ({"mode": "single", "opacity": 2}, "opacity must be a number from 0 to 1"),
            ({"mode": "heatmap"}, "mode must be single, categorized or graduated"),
        ]
        for arguments, pattern in cases:
            with self.subTest(arguments=arguments):
                with self.assertRaisesRegex(RuntimeError, pattern):
                    self._renderer(**arguments)
                self._assert_single_symbol_kept(self.layer)

        with self.assertRaisesRegex(RuntimeError, "Vector layer not found"):
            self.tools.interactive_layer_set_renderer(
                {"layer_id": "missing-layer", "mode": "single"}
            )

    def test_set_vector_renderer_tool_schema_and_routing(self):
        import qcopilots_common.interactive_layer_tools as interactive_layer_tools
        from qcopilots_common.mcp_http import ToolError, _validate_tool_arguments

        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return {"tool": tool}

        with mock.patch.object(
            interactive_layer_tools, "BridgeClient", FakeBridgeClient
        ):
            tools = {
                tool.name: tool
                for tool in interactive_layer_tools.build_interactive_layer_tools()
            }
        tool = tools["set_vector_renderer"]
        names = list(tools)
        self.assertEqual(
            names.index("set_vector_renderer"), names.index("apply_layer_style") + 1
        )
        schema = tool.input_schema
        self.assertEqual(schema["required"], ["layer_id", "mode"])
        _validate_tool_arguments(
            {
                "layer_id": "a",
                "mode": "graduated",
                "field": "area",
                "breaks": [0, 1, 2],
            },
            schema,
        )
        for arguments in (
            {"layer_id": "a"},
            {"layer_id": "a", "mode": "heatmap"},
            {"layer_id": "a", "mode": "graduated", "classes": 13},
            {"layer_id": "a", "mode": "single", "color": "#ff000080"},
            {"layer_id": "a", "mode": "single", "symbol": "x"},
        ):
            with self.subTest(arguments=arguments):
                with self.assertRaisesRegex(ToolError, "Invalid tool arguments"):
                    _validate_tool_arguments(arguments, schema)
        tool.handler({"layer_id": "a", "mode": "single"})
        self.assertEqual(
            calls,
            [("interactive_layer_set_renderer", {"layer_id": "a", "mode": "single"})],
        )
        self.assertIn("SLD", tools["apply_layer_style"].description)


if __name__ == "__main__":
    unittest.main()
