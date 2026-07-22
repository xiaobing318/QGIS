"""QCopilots MCP servers manager plugin.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""


def classFactory(iface):
    from qcopilots_mcp_servers_manager.plugin import QCopilotsMCPServersManagerPlugin

    return QCopilotsMCPServersManagerPlugin(iface)

