"""QGIS plugin wrapper for shared QCopilots Python support.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""


class QCopilotsCommonPlugin:
    """No-op plugin wrapper so shared QCopilots support is not reported broken."""

    def __init__(self, iface):
        self.iface = iface

    def initGui(self):
        pass

    def unload(self):
        pass
