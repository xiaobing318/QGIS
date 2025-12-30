file(GLOB PY_FILES *.py)

PLUGIN_INSTALL(processing mcpserver ${PY_FILES})
