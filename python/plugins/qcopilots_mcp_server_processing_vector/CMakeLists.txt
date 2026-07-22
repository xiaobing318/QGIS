file(GLOB MD_FILES *.md)
file(GLOB PY_FILES *.py)
file(GLOB JSON_FILES *.json)
file(GLOB LOCK_FILES *.lock)
file(GLOB SVG_FILES *.svg)
file(GLOB TXT_FILES *.txt)

PLUGIN_INSTALL(qcopilots_mcp_server_processing_vector . ${MD_FILES} ${PY_FILES} ${JSON_FILES} ${LOCK_FILES} ${SVG_FILES} ${TXT_FILES})
