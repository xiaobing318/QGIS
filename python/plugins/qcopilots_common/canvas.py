"""Refresh helpers for QGIS desktop map canvases.

.. note:: This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

from typing import Any


def refresh_map_canvases(iface: Any) -> None:
    """Schedule a refresh of each desktop map canvas from the GUI thread."""
    if iface is not None:
        for canvas in iface.mapCanvases():
            canvas.refresh()


class LayerTreeUpdateBarrier:
    """Wait across queued GUI deliveries until the layer tree is stable.

    Call is_settled() on the GUI thread. If it returns false, queue the next
    check with Qt.QueuedConnection. Close the barrier if the request is canceled.
    """

    def __init__(self, iface: Any):
        self._generation = 0
        self._queued_generation: int | None = None
        self._connections = []
        canvas_bridge = getattr(iface, "layerTreeCanvasBridge", None)
        bridge = canvas_bridge() if canvas_bridge is not None else None
        if bridge is None:
            return

        from qgis.core import QgsProject

        root = bridge.rootGroup()
        project = QgsProject.instance()
        try:
            self._connect(root.visibilityChanged, self._changed)
            self._connect(root.layerOrderChanged, self._changed)
            self._connect(root.customPropertyChanged, self._property_changed)
            self._connect(project.layersAdded, self._layers_added)
            self._watch_layers(project.mapLayers().values())
        except Exception:
            self.close()
            raise

    def is_settled(self) -> bool:
        if self._generation == self._queued_generation:
            self.close()
            return True
        self._queued_generation = self._generation
        return False

    def close(self) -> None:
        for signal, slot in self._connections:
            try:
                signal.disconnect(slot)
            except (TypeError, RuntimeError):
                # Layers can be removed while this barrier is queued.
                pass
        self._connections.clear()

    def _connect(self, signal, slot) -> None:
        signal.connect(slot)
        self._connections.append((signal, slot))

    def _changed(self, *_args) -> None:
        self._generation += 1

    def _property_changed(self, _node, key: str) -> None:
        if key == "overview":
            self._changed()

    def _watch_layers(self, layers) -> None:
        for layer in layers:
            self._connect(layer.dataSourceChanged, self._changed)

    def _layers_added(self, layers) -> None:
        self._watch_layers(layers)
        self._changed()
