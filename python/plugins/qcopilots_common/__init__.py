"""QCopilots Common support plugin.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__version__ = "0.1.0"


def classFactory(iface):
    from qcopilots_common.plugin import QCopilotsCommonPlugin

    return QCopilotsCommonPlugin(iface)
