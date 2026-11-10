"""Snapshot GUI-owned map decorations and legacy annotations for export.

This program is free software under the GNU General Public License, version 2
or (at your option) any later version.
"""

from qgis.core import (
    QgsCoordinateTransform,
    QgsCsException,
    QgsMapDecoration,
    QgsProject,
    QgsRenderContext,
)
from qgis.PyQt.QtCore import QCoreApplication, QPointF, Qt, QThread
from qgis.PyQt.QtGui import QImage, QPainter


class _MapExportOverlay(QgsMapDecoration):
    """Replay an owned image without accessing the desktop from a render task."""

    def __init__(self, image):
        super().__init__()
        self._image = image

    def render(self, _settings, context):
        painter = context.painter()
        painter.save()
        try:
            painter.drawImage(QPointF(0, 0), self._image)
        finally:
            painter.restore()


def snapshot_map_overlays(iface, settings):
    """Return a task-safe decoration, or None if the current map has no overlays.

    Call this on the GUI thread and retain its result until the renderer task
    terminates. Native decoration rendering can access layout designer widgets.
    Legacy form annotations also render QWidget instances, so neither the live
    objects nor their clones may be rendered by the background map task.
    """
    app = QCoreApplication.instance()
    if app is None or QThread.currentThread() != app.thread():
        raise RuntimeError("Map export overlays require the QGIS GUI thread")

    decorations = list(iface.activeDecorations())
    canvas = iface.mapCanvas()
    visible_layers = settings.layers(True)
    annotations = []
    if canvas.annotationsVisible():
        for annotation in QgsProject.instance().annotationManager().annotations():
            if annotation.isVisible() and (
                annotation.mapLayer() is None or annotation.mapLayer() in visible_layers
            ):
                annotations.append(annotation.clone())
    if not decorations and not annotations:
        return None

    image = QImage(
        settings.deviceOutputSize(), QImage.Format.Format_ARGB32_Premultiplied
    )
    if image.isNull():
        raise RuntimeError("Could not allocate the map export overlay image")
    image.setDevicePixelRatio(settings.devicePixelRatio())
    dots_per_meter = int(1000 * settings.outputDpi() / 25.4)
    image.setDotsPerMeterX(dots_per_meter)
    image.setDotsPerMeterY(dots_per_meter)
    image.fill(Qt.GlobalColor.transparent)

    painter = QPainter(image)
    if not painter.isActive():
        raise RuntimeError("Could not paint the map export overlay image")
    context = QgsRenderContext.fromMapSettings(settings)
    context.setPainter(painter)
    try:
        # Match QgsMapRendererTask's composition order while all GUI-owned
        # objects are still accessed on their owning thread.
        for decoration in decorations:
            decoration.render(settings, context)
        for annotation in annotations:
            if annotation.hasFixedMapPosition():
                position = annotation.mapPosition()
                transform = QgsCoordinateTransform(
                    annotation.mapPositionCrs(),
                    settings.destinationCrs(),
                    settings.transformContext(),
                )
                try:
                    position = transform.transform(position)
                except QgsCsException:
                    # QgsMapCanvasAnnotationItem uses the untransformed point
                    # when a coordinate transformation is unavailable.
                    pass
                pixel = settings.mapToPixel().transform(position)
                x, y = pixel.x(), pixel.y()
            else:
                position = annotation.relativePosition()
                x = position.x() * settings.outputSize().width()
                y = position.y() * settings.outputSize().height()
            painter.save()
            try:
                context.setPainterFlagsUsingContext()
                painter.translate(x, y)
                annotation.render(context)
            finally:
                painter.restore()
    finally:
        painter.end()
    return _MapExportOverlay(image)
