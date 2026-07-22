file(GLOB MD_FILES *.md)
file(GLOB PY_FILES *.py)
file(GLOB SVG_FILES *.svg)
file(GLOB TXT_FILES *.txt)

PLUGIN_INSTALL(qcopilots_common . ${MD_FILES} ${PY_FILES} ${SVG_FILES} ${TXT_FILES})
