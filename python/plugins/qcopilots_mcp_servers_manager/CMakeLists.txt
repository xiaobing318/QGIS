file(GLOB PY_FILES *.py)
file(GLOB JSON_FILES *.json)
file(GLOB SVG_FILES *.svg)
file(GLOB TXT_FILES *.txt)
file(GLOB MD_FILES *.md)

PLUGIN_INSTALL(qcopilots_mcp_servers_manager . ${PY_FILES} ${JSON_FILES} ${SVG_FILES} ${TXT_FILES} ${MD_FILES})
