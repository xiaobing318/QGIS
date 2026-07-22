file(GLOB MD_FILES *.md)
file(GLOB PY_FILES *.py)
file(GLOB JSON_FILES *.json)
file(GLOB LOCK_FILES *.lock)
file(GLOB SVG_FILES *.svg)
file(GLOB TXT_FILES *.txt)

PLUGIN_INSTALL(qcopilots_mcp_server_skills . ${MD_FILES} ${PY_FILES} ${JSON_FILES} ${LOCK_FILES} ${SVG_FILES} ${TXT_FILES})

file(GLOB_RECURSE SKILL_FILES CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "Skills/*")
foreach(SKILL_FILE ${SKILL_FILES})
  get_filename_component(SKILL_SUBDIR "${SKILL_FILE}" DIRECTORY)
  PLUGIN_INSTALL(qcopilots_mcp_server_skills ${SKILL_SUBDIR} ${SKILL_FILE})
endforeach()
