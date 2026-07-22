"""Minimal QGIS plugin wrapper for QCopilots service plugins.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

from pathlib import Path

from qcopilots_common.manifest import load_service_manifest


class QCopilotsServicePlugin:
    manifest_filename = "qcopilots_service.json"

    def __init__(self, iface, plugin_dir: str | Path):
        self.iface = iface
        self.plugin_dir = Path(plugin_dir)
        self.action = None
        self.manifest = load_service_manifest(self.plugin_dir / self.manifest_filename)

    def initGui(self):
        # Service plugins are discovered and controlled by QCopilotsMCPServersManager.
        self.action = None

    def unload(self):
        self.action = None
