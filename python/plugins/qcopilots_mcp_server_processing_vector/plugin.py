"""QGIS plugin wrapper for the QCopilots vector Processing service.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from pathlib import Path

from qcopilots_common.service_plugin import QCopilotsServicePlugin


class QCopilotsMCPServerProcessingVectorPlugin(QCopilotsServicePlugin):
    def __init__(self, iface):
        super().__init__(iface, Path(__file__).resolve().parent)
