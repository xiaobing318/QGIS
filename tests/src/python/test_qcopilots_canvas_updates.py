"""Canvas updates after QCopilots interactive MCP operations.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

import tempfile
import time
import unittest
from pathlib import Path
from types import SimpleNamespace

# The Windows CTest wrapper can leave its multi-config DLL path unexpanded.
import test_qcopilots_mcp_server_interactive_tools  # noqa: F401
from qcopilots_common.bridge import (
    DestructiveActionConfirmationStore,
    QgisBridgeTools,
    _native_resolution_zoom_factor,
)
from qgis.core import (
    Qgis,
    QgsCoordinateReferenceSystem,
    QgsFeature,
    QgsFillSymbol,
    QgsGeometry,
    QgsMapSettings,
    QgsMapThemeCollection,
    QgsProject,
    QgsProperty,
    QgsRectangle,
    QgsSingleSymbolRenderer,
    QgsSymbolLayer,
    QgsVectorFileWriter,
    QgsVectorLayer,
)
from qgis.gui import QgsLayerTreeMapCanvasBridge, QgsMapCanvas
from qgis.PyQt import sip
from qgis.PyQt.QtCore import QSize
from qgis.PyQt.QtGui import QAction, QColor
from qgis.PyQt.QtTest import QSignalSpy, QTest
from qgis.testing import start_app

RED = "#dc0000"
GREEN = "#00b400"
BLUE = "#0050dc"
WHITE = "#ffffff"
POLYGON = "Polygon?crs=EPSG:4326&field=color:string"
GEOMETRY = QgsRectangle(-3, -3, 3, 3)
VIEW = QgsRectangle(-1, -1, 1, 1)


class TestQCopilotsCanvasUpdates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        start_app()

    def setUp(self):
        self.project = QgsProject.instance()
        self.project.setCrs(QgsCoordinateReferenceSystem("EPSG:4326"))
        self.canvases = [QgsMapCanvas(), QgsMapCanvas()]
        for canvas in self.canvases:
            canvas.setCanvasColor(QColor(WHITE))
            canvas.setDestinationCrs(self.project.crs())
            canvas.setCachingEnabled(True)
            canvas.resize(200, 150)
            canvas.setExtent(VIEW)
        self.tree_bridge = QgsLayerTreeMapCanvasBridge(
            self.project.layerTreeRoot(), self.canvases[0]
        )
        self.tree_bridge.setAutoSetupOnFirstLayer(False)
        self.tree_bridge.canvasLayersChanged.connect(self.canvases[1].setLayers)
        self.project.crsChanged.connect(self._set_main_crs)
        self.frames = [
            QSignalSpy(canvas.mapCanvasRefreshed) for canvas in self.canvases
        ]
        self.confirmation_time = 0
        self.tools = QgisBridgeTools(
            SimpleNamespace(
                mapCanvas=lambda: self.canvases[0],
                mapCanvases=lambda: self.canvases,
            ),
            destructive_action_confirmations=DestructiveActionConfirmationStore(
                ttl_seconds=1,
                clock=lambda: self.confirmation_time,
            ),
        )

    def tearDown(self):
        self.project.crsChanged.disconnect(self._set_main_crs)
        sip.delete(self.tree_bridge)
        for canvas in self.canvases:
            sip.delete(canvas)
        self.project.clear()

    def _set_main_crs(self):
        self.canvases[0].setDestinationCrs(self.project.crs())

    def _set_color(self, layer, color, color_expression=None):
        symbol = QgsFillSymbol.createSimple({"color": color, "outline_style": "no"})
        if color_expression:
            symbol.symbolLayer(0).setDataDefinedProperty(
                QgsSymbolLayer.Property.PropertyFillColor,
                QgsProperty.fromExpression(color_expression),
            )
        layer.setRenderer(QgsSingleSymbolRenderer(symbol))

    def _add_layer(self, name, color, color_expression=None, *, group=None):
        layer = QgsVectorLayer(POLYGON, name, "memory")
        self.assertTrue(layer.isValid())
        feature = QgsFeature(layer.fields())
        feature.setGeometry(QgsGeometry.fromRect(GEOMETRY))
        self.assertTrue(layer.dataProvider().addFeatures([feature])[0])
        layer.updateExtents()
        self._set_color(layer, color, color_expression)
        self.project.addMapLayer(layer, group is None)
        if group is not None:
            group.addLayer(layer)
        return layer.id()

    def _wait_for(self, condition, message):
        deadline = time.monotonic() + 3
        while not condition() and time.monotonic() < deadline:
            QTest.qWait(15)
        self.assertTrue(condition(), message)

    def _colors(self):
        colors = []
        for canvas in self.canvases:
            image = canvas.viewport().grab().toImage()
            self.assertFalse(image.isNull())
            center = image.pixelColor(image.width() // 2, image.height() // 2)
            colors.append(center.name())
        return colors

    def _settle_view(self, color):
        for canvas in self.canvases:
            canvas.show()
            canvas.refresh()
        # A resize schedules a delayed refresh even if the first frame finished.
        QTest.qWait(750)
        self._wait_for(
            lambda: not any(canvas.isDrawing() for canvas in self.canvases),
            "Canvas setup did not settle",
        )
        self.assertEqual(self._colors(), [color, color])

    def _frame_counts(self):
        return [len(spy) for spy in self.frames]

    def _assert_frame(self, before, color=None):
        self._wait_for(
            lambda: all(
                current > previous
                for current, previous in zip(self._frame_counts(), before)
            ),
            "Expected a new frame in both canvases",
        )
        if color is not None:
            self.assertEqual(self._colors(), [color, color])

    def _preview_removal(self, *layer_ids):
        return self.tools.interactive_layer_remove_layers(
            {"layer_ids": list(layer_ids)}
        )

    def _confirm_removal(self, preview):
        return self.tools.interactive_layer_remove_layers(
            {"confirmation_token": preview["confirmation_token"]}
        )

    def test_removal_updates_both_canvases(self):
        bottom = self._add_layer("Bottom", BLUE)
        middle = self._add_layer("Middle", GREEN)
        top = self._add_layer("Top", RED)
        self._settle_view(RED)

        before = self._frame_counts()
        expired_preview = self._preview_removal(top)
        self.confirmation_time += 1
        with self.assertRaisesRegex(RuntimeError, "token has expired"):
            self._confirm_removal(expired_preview)

        preview = self._preview_removal(top)
        self.assertTrue(preview["preview"])
        with self.assertRaisesRegex(RuntimeError, "invalid or already used"):
            self.tools.interactive_layer_remove_layers(
                {"confirmation_token": "unknown-token"}
            )
        QTest.qWait(100)
        self.assertEqual(self._frame_counts(), before)
        self.assertEqual(self._colors(), [RED, RED])

        self.assertEqual(self._confirm_removal(preview)["removed_count"], 1)
        self._assert_frame(before, GREEN)
        self.assertEqual(self._frame_counts(), [count + 1 for count in before])

        before = self._frame_counts()
        preview = self._preview_removal(middle, bottom)
        self.assertEqual(self._confirm_removal(preview)["removed_count"], 2)
        self._assert_frame(before, WHITE)
        self.assertEqual(self._frame_counts(), [count + 1 for count in before])
        self.assertFalse(self.project.mapLayers())

    def test_removal_preserves_frozen_and_disabled_rendering(self):
        top = self._add_layer("Top", RED)
        self._settle_view(RED)
        before = self._frame_counts()
        self.canvases[0].freeze(True)
        self.canvases[1].setRenderFlag(False)

        preview = self._preview_removal(top)
        self.assertEqual(self._confirm_removal(preview)["removed_count"], 1)
        QTest.qWait(100)
        self.assertTrue(self.canvases[0].isFrozen())
        self.assertFalse(self.canvases[1].renderFlag())
        self.assertEqual(self._frame_counts(), before)
        self.assertEqual(self._colors(), [RED, RED])
        self.assertIsNone(self.project.mapLayer(top))

    def test_visibility_and_removal_respect_hidden_parent_group(self):
        root = self.project.layerTreeRoot()
        self._add_layer("Bottom", BLUE)
        group = root.insertGroup(0, "Parent")
        top = self._add_layer("Top", RED, group=group)
        group.setItemVisibilityChecked(False)
        self._settle_view(BLUE)

        before = self._frame_counts()
        node = root.findLayer(top)
        for visible in (False, True):
            result = self.tools.set_layer_visibility(
                {"layer_id": top, "visible": visible}
            )
            self.assertEqual(result["visible"], visible)
            self.assertEqual(node.itemVisibilityChecked(), visible)
            self.assertFalse(node.isVisible())
            QTest.qWait(100)
            self.assertEqual(self._frame_counts(), before)
            self.assertEqual(self._colors(), [BLUE, BLUE])

        group.setItemVisibilityChecked(True)
        self._assert_frame(before, RED)
        before = self._frame_counts()
        group.setItemVisibilityChecked(False)
        self._assert_frame(before, BLUE)

        preview = self._preview_removal(top)
        self.assertEqual(self._confirm_removal(preview)["removed_count"], 1)
        self.assertIsNone(root.findLayer(top))
        self.assertIsNone(self.project.mapLayer(top))
        self.assertFalse(group.itemVisibilityChecked())
        self.assertFalse(group.children())
        QTest.qWait(100)
        self.assertEqual(self._colors(), [BLUE, BLUE])

    def test_auxiliary_canvas_keeps_its_map_theme(self):
        self._add_layer("Bottom", BLUE)
        top = self._add_layer("Top", RED)
        theme = QgsMapThemeCollection.MapThemeRecord()
        theme.setLayerRecords(
            [QgsMapThemeCollection.MapThemeLayerRecord(self.project.mapLayer(top))]
        )
        self.project.mapThemeCollection().insert("Top only", theme)
        auxiliary = self.canvases[1]
        auxiliary.setTheme("Top only")
        self._settle_view(RED)

        before = self._frame_counts()
        self.tools.set_layer_visibility({"layer_id": top, "visible": False})
        self._wait_for(
            lambda: self._frame_counts()[0] > before[0],
            "Expected the main canvas to hide the top layer",
        )
        self.assertEqual(self._colors(), [BLUE, RED])
        self.assertEqual(auxiliary.theme(), "Top only")

        before = self._frame_counts()
        preview = self._preview_removal(top)
        self.assertEqual(self._confirm_removal(preview)["removed_count"], 1)
        self._assert_frame(before)
        self.assertEqual(self._colors(), [BLUE, WHITE])
        self.assertEqual(auxiliary.theme(), "Top only")

    def test_project_crs_change_redraws_fixed_crs_canvas(self):
        self._add_layer(
            "Project CRS color",
            RED,
            "if(@project_crs = 'EPSG:3857', '#00b400', '#dc0000')",
        )
        self._settle_view(RED)
        before = self._frame_counts()

        self.tools.interactive_project_set_crs({"crs": "EPSG:3857"})

        self._assert_frame(before, GREEN)
        destinations = [
            canvas.mapSettings().destinationCrs().authid() for canvas in self.canvases
        ]
        self.assertEqual(destinations, ["EPSG:3857", "EPSG:4326"])
        before = self._frame_counts()
        self.tools.interactive_project_set_crs({"crs": "EPSG:3857"})
        QTest.qWait(100)
        self.assertEqual(self._frame_counts(), before)

    def test_memory_edits_styles_labels_and_selection_update_both_canvases(self):
        layer = QgsVectorLayer(POLYGON, "Memory edits", "memory")
        self.assertTrue(layer.isValid())
        self._set_color(layer, RED, '"color"')
        layer_id = layer.id()
        self.project.addMapLayer(layer)
        for canvas in self.canvases:
            canvas.setSelectionColor(QColor(RED))
        self._settle_view(WHITE)

        before = self._frame_counts()
        added = self.tools.add_vector_features(
            {
                "layer_id": layer_id,
                "features": [
                    {
                        "geometry_wkt": QgsGeometry.fromRect(GEOMETRY).asWkt(),
                        "attributes": {"color": RED},
                    }
                ],
            }
        )
        self.assertTrue(added["staged"])
        self._assert_frame(before, RED)
        feature_id = next(layer.getFeatures()).id()

        before = self._frame_counts()
        self.tools.update_vector_features(
            {
                "layer_id": layer_id,
                "updates": [{"feature_id": feature_id, "attributes": {"color": GREEN}}],
            }
        )
        self._assert_frame(before, GREEN)

        before = self._frame_counts()
        self.tools.interactive_layer_set_renderer(
            {"layer_id": layer_id, "mode": "single", "color": BLUE}
        )
        self._assert_frame(before, BLUE)

        before = self._frame_counts()
        with tempfile.TemporaryDirectory() as directory:
            style_path = str(Path(directory) / "green.qml")
            source = QgsVectorLayer(POLYGON, "Green style", "memory")
            self._set_color(source, GREEN)
            message, saved = source.saveNamedStyle(style_path)
            self.assertTrue(saved, message)
            self.tools.interactive_layer_apply_style(
                {"layer_id": layer_id, "style_path": style_path}
            )
        self._assert_frame(before, GREEN)

        before = self._frame_counts()
        self.tools.interactive_layer_set_selection(
            {"layer_id": layer_id, "mode": "replace", "feature_ids": [feature_id]}
        )
        self.assertEqual(layer.selectedFeatureIds(), [feature_id])
        self._assert_frame(before, RED)

        before = self._frame_counts()
        self.tools.interactive_layer_set_selection(
            {"layer_id": layer_id, "mode": "clear"}
        )
        self.assertFalse(layer.selectedFeatureIds())
        self._assert_frame(before, GREEN)

        before = self._frame_counts()
        self.tools.interactive_layer_configure_labels(
            {
                "layer_id": layer_id,
                "field_or_expression": "'QCopilots'",
                "is_expression": True,
            }
        )
        self.assertTrue(layer.labelsEnabled())
        self._assert_frame(before)
        for canvas in self.canvases:
            labels = canvas.labelingResults().allLabels()
            self.assertEqual([label.labelText for label in labels], ["QCopilots"])

        before = self._frame_counts()
        preview = self.tools.interactive_layer_delete_features(
            {"layer_id": layer_id, "feature_ids": [feature_id]}
        )
        deleted = self.tools.interactive_layer_delete_features(
            {"layer_id": layer_id, "confirmation_token": preview["confirmation_token"]}
        )
        self.assertTrue(deleted["staged"])
        self._assert_frame(before, WHITE)

    def test_transaction_edits_update_both_canvases(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        path = str(Path(directory.name) / "edits.gpkg")
        template = QgsVectorLayer(POLYGON, "Edits", "memory")
        options = QgsVectorFileWriter.SaveVectorOptions()
        options.driverName = "GPKG"
        result = QgsVectorFileWriter.writeAsVectorFormatV3(
            template, path, self.project.transformContext(), options
        )
        self.assertEqual(result[0], QgsVectorFileWriter.WriterError.NoError, result)
        layer = QgsVectorLayer(path, "Edits", "ogr")
        self.assertTrue(layer.isValid())
        self.project.setTransactionMode(Qgis.TransactionMode.AutomaticGroups)
        self._set_color(layer, RED, '"color"')
        self.project.addMapLayer(layer)
        self.assertTrue(layer.startEditing())
        self.assertIsNotNone(layer.dataProvider().transaction())
        self._settle_view(WHITE)

        before = self._frame_counts()
        added = self.tools.add_vector_features(
            {
                "layer_id": layer.id(),
                "features": [
                    {
                        "geometry_wkt": QgsGeometry.fromRect(GEOMETRY).asWkt(),
                        "attributes": {"color": RED},
                    }
                ],
            }
        )
        self.assertTrue(added["staged"])
        self.assertEqual(added["added_count"], 1)
        self._assert_frame(before, RED)
        feature_id = next(layer.getFeatures()).id()

        before = self._frame_counts()
        updated = self.tools.update_vector_features(
            {
                "layer_id": layer.id(),
                "updates": [{"feature_id": feature_id, "attributes": {"color": GREEN}}],
            }
        )
        self.assertTrue(updated["staged"])
        self._assert_frame(before, GREEN)

        before = self._frame_counts()
        preview = self.tools.interactive_layer_delete_features(
            {"layer_id": layer.id(), "feature_ids": [feature_id]}
        )
        self.assertTrue(preview["preview"])
        QTest.qWait(100)
        self.assertEqual(self._frame_counts(), before)
        deleted = self.tools.interactive_layer_delete_features(
            {
                "layer_id": layer.id(),
                "confirmation_token": preview["confirmation_token"],
            }
        )
        self.assertTrue(deleted["staged"])
        self._assert_frame(before, WHITE)
        self.assertTrue(layer.isEditable())
        self.assertTrue(layer.rollBack())


class TestQCopilotsNativeCanvasActions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        start_app()

    def test_refresh_delegates_to_native_action(self):
        action = QAction()
        triggered = QSignalSpy(action.triggered)
        tools = QgisBridgeTools(
            SimpleNamespace(
                actionDraw=lambda: action,
                mapCanvas=lambda: SimpleNamespace(refresh=lambda: None),
            )
        )
        self.assertEqual(tools.refresh_canvas({}), {"refreshed": True})
        self.assertEqual(len(triggered), 1)

    def test_zoom_uses_nearest_native_resolution_and_device_pixel_ratio(self):
        settings = QgsMapSettings()
        settings.setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:3857"))
        settings.setOutputSize(QSize(100, 100))
        settings.setExtent(QgsRectangle(-100, -100, 100, 100))
        settings.setDevicePixelRatio(2)
        canvas = SimpleNamespace(
            mapSettings=lambda: settings,
            mapUnitsPerPixel=settings.mapUnitsPerPixel,
            extent=settings.visibleExtent,
        )
        resolutions = [0.25, 1.5, 4]
        layer = SimpleNamespace(
            crs=settings.destinationCrs,
            dataProvider=lambda: SimpleNamespace(nativeResolutions=lambda: resolutions),
            rasterUnitsPerPixelX=lambda: 1,
            rasterUnitsPerPixelY=lambda: 1,
        )
        self.assertAlmostEqual(_native_resolution_zoom_factor(layer, canvas), 0.75)
        resolutions.clear()
        self.assertAlmostEqual(_native_resolution_zoom_factor(layer, canvas), 1)

    def test_zoom_projects_the_canvas_pixel_to_the_raster_crs(self):
        settings = QgsMapSettings()
        settings.setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:3857"))
        settings.setOutputSize(QSize(1000, 1000))
        settings.setExtent(QgsRectangle(-100000, -100000, 100000, 100000))
        canvas = SimpleNamespace(
            mapSettings=lambda: settings,
            mapUnitsPerPixel=settings.mapUnitsPerPixel,
            extent=settings.visibleExtent,
        )
        layer = SimpleNamespace(
            crs=lambda: QgsCoordinateReferenceSystem("EPSG:4326"),
            dataProvider=lambda: None,
            rasterUnitsPerPixelX=lambda: 0.001,
            rasterUnitsPerPixelY=lambda: 0.001,
        )
        self.assertAlmostEqual(
            _native_resolution_zoom_factor(layer, canvas), 0.55659745
        )


if __name__ == "__main__":
    unittest.main()
