file(GLOB PY_FILES *.py)

PLUGIN_INSTALL(processing processingmcpserver ${PY_FILES})
