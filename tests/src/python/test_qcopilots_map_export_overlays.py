"""GUI snapshots of decorations and legacy annotations for map image export."""

import math
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

# Keep native tests mandatory, including Windows multi-configuration CTest.
import test_qcopilots_mcp_server_interactive_tools  # noqa: F401
from qcopilots_common.map_export_overlays import snapshot_map_overlays
from qgis.core import (
    QgsApplication,
    QgsCoordinateReferenceSystem,
    QgsFillSymbol,
    QgsMapDecoration,
    QgsMapRendererTask,
    QgsMapSettings,
    QgsMarkerSymbol,
    QgsPointXY,
    QgsProject,
    QgsRectangle,
    QgsRenderContext,
    QgsTextAnnotation,
    QgsVectorLayer,
)
from qgis.PyQt import sip
from qgis.PyQt.QtCore import QEventLoop, QPointF, QRectF, QSize, QSizeF, QThread, QTimer
from qgis.PyQt.QtGui import QColor, QImage, QPainter, QTextDocument
from qgis.testing import start_app


class TestQCopilotsMapExportOverlays(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = start_app()

    def setUp(self):
        self.project = QgsProject.instance()
        self.project.clear()
        self.decorations = []
        self.annotations_visible = True
        self.iface = SimpleNamespace(
            activeDecorations=lambda: list(self.decorations),
            mapCanvas=lambda: SimpleNamespace(
                annotationsVisible=lambda: self.annotations_visible
            ),
        )

    def tearDown(self):
        self.project.clear()

    def _settings(self, *, rotation=0, dpr=2):
        settings = QgsMapSettings()
        settings.setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:3857"))
        settings.setOutputSize(QSize(320, 240))
        settings.setOutputDpi(120)
        settings.setDevicePixelRatio(dpr)
        settings.setExtent(QgsRectangle(110000, 110000, 114000, 113000))
        settings.setRotation(rotation)
        settings.setBackgroundColor(QColor("white"))
        settings.setTransformContext(self.project.transformContext())
        return settings

    def _render_overlay(self, overlay, settings):
        image = QImage(
            settings.outputSize() * settings.devicePixelRatio(),
            QImage.Format.Format_ARGB32,
        )
        image.setDevicePixelRatio(settings.devicePixelRatio())
        image.fill(QColor("white"))
        painter = QPainter(image)
        try:
            context = QgsRenderContext.fromMapSettings(settings)
            context.setPainter(painter)
            if overlay is not None:
                overlay.render(settings, context)
        finally:
            painter.end()
        return image

    def _native_export(self, overlay, settings):
        # Holding overlay until task termination is intentional. QgsMapRendererTask
        # borrows decoration pointers, so the production runtime must do the same.
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "overlay.png"
            task = QgsMapRendererTask(settings, str(target), "PNG")
            task.setSaveWorldFile(False)
            task.addDecorations([overlay])
            loop = QEventLoop()
            outcome = []
            heartbeats = []
            timer = QTimer()
            timer.timeout.connect(lambda: heartbeats.append(True))
            watchdog = QTimer()
            watchdog.setSingleShot(True)
            watchdog.timeout.connect(loop.quit)
            task.renderingComplete.connect(
                lambda: (outcome.append("complete"), loop.quit())
            )
            task.errorOccurred.connect(lambda code: (outcome.append(code), loop.quit()))
            task.taskTerminated.connect(
                lambda: (outcome.append("terminated"), loop.quit())
            )
            self.assertGreater(QgsApplication.taskManager().addTask(task), 0)
            timer.start(0)
            watchdog.start(5000)
            try:
                loop.exec()
            finally:
                timer.stop()
                watchdog.stop()
                if not outcome:
                    task.cancel()
            self.assertEqual(outcome, ["complete"])
            self.assertTrue(heartbeats)
            image = QImage(str(target))
            self.assertFalse(image.isNull())
            return image

    def _annotation(self, *, relative=False):
        annotation = QgsTextAnnotation()
        annotation.setDocument(QTextDocument(""))
        annotation.setFrameSizeMm(QSizeF(0, 0))
        annotation.setFrameOffsetFromReferencePointMm(QPointF())
        annotation.setFillSymbol(
            QgsFillSymbol.createSimple({"color": "transparent", "outline_style": "no"})
        )
        annotation.setMarkerSymbol(
            QgsMarkerSymbol.createSimple(
                {
                    "name": "square",
                    "color": "#ff00ff",
                    "outline_style": "no",
                    "size": "5",
                }
            )
        )
        annotation.setHasFixedMapPosition(not relative)
        annotation.setMapPositionCrs(QgsCoordinateReferenceSystem("EPSG:4326"))
        annotation.setMapPosition(QgsPointXY(1, 1))
        if relative:
            annotation.setRelativePosition(QPointF(0.25, 0.75))
            annotation.setFrameSizeMm(QSizeF(8, 8))
            annotation.setFillSymbol(
                QgsFillSymbol.createSimple({"color": "#ff00ff", "outline_style": "no"})
            )
        self.project.annotationManager().addAnnotation(annotation)
        return annotation

    def test_decoration_is_snapshotted_on_gui_and_survives_original_deletion(self):
        calls = []
        gui_thread = self.app.thread()

        class GuiDecoration(QgsMapDecoration):
            def __init__(self):
                super().__init__()
                self.color = QColor("red")

            def render(self, settings, context):
                calls.append(QThread.currentThread() == gui_thread)
                context.painter().fillRect(QRectF(10, 10, 30, 20), self.color)

        decoration = GuiDecoration()
        self.decorations.append(decoration)
        settings = self._settings(dpr=2)
        overlay = snapshot_map_overlays(self.iface, settings)
        self.assertIsNotNone(overlay)
        self.assertEqual(calls, [True])
        decoration.color = QColor("blue")
        self.decorations.clear()
        sip.delete(decoration)
        image = self._native_export(overlay, settings)
        self.assertEqual(image.size(), QSize(640, 480))
        self.assertEqual(image.pixelColor(40, 40), QColor("red"))
        self.assertEqual(image.pixelColor(100, 100), QColor("white"))
        self.assertEqual(
            calls, [True], "The worker must never revisit a live decoration"
        )

    def test_fixed_annotation_uses_its_crs_and_rotated_map_position(self):
        annotation = self._annotation()
        settings = self._settings(rotation=37)
        overlay = snapshot_map_overlays(self.iface, settings)
        self.assertIsNotNone(overlay)
        # Independent Web Mercator and rotated viewport oracle for (1°, 1°).
        x = 6378137 * math.pi / 180
        y = 6378137 * math.log(math.tan(math.pi / 4 + math.pi / 360))
        dx = (x - 112000) / 12.5
        dy = (111500 - y) / 12.5
        angle = math.radians(37)
        pixel_x = 160 + math.cos(angle) * dx - math.sin(angle) * dy
        pixel_y = 120 + math.sin(angle) * dx + math.cos(angle) * dy
        # Subsequent GUI edits must not move the previously captured annotation.
        annotation.setMapPosition(QgsPointXY(20, 20))
        image = self._render_overlay(overlay, settings)
        self.assertEqual(
            image.pixelColor(round(pixel_x * 2), round(pixel_y * 2)), QColor("magenta")
        )
        self.assertEqual(image.pixelColor(320, 240), QColor("white"))

    def test_relative_annotations_and_visibility_follow_the_map_snapshot(self):
        annotation = self._annotation(relative=True)
        settings = self._settings(rotation=37)
        overlay = snapshot_map_overlays(self.iface, settings)
        image = self._render_overlay(overlay, settings)
        self.assertEqual(image.pixelColor(170, 370), QColor("magenta"))
        annotation.setVisible(False)
        self.assertIsNone(snapshot_map_overlays(self.iface, settings))
        annotation.setVisible(True)
        self.annotations_visible = False
        self.assertIsNone(snapshot_map_overlays(self.iface, settings))
        self.annotations_visible = True
        linked = QgsVectorLayer("Point?crs=EPSG:3857", "linked", "memory")
        self.project.addMapLayer(linked)
        annotation.setMapLayer(linked)
        self.assertIsNone(snapshot_map_overlays(self.iface, settings))
        settings.setLayers([linked])
        self.assertIsNotNone(snapshot_map_overlays(self.iface, settings))

    def test_empty_snapshot_avoids_allocating_an_overlay(self):
        self.assertIsNone(snapshot_map_overlays(self.iface, self._settings()))


if __name__ == "__main__":
    unittest.main()
