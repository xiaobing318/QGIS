########################################################
# Files

set (qcopilots_SRCS
  qcopilots_container_dock.cpp
  qcopilots_container_plugin.cpp
  qcopilots_container_utils.cpp
  qcopilots_mcp_bridge.cpp
  qcopilots_mcp_catalog.cpp
)

set (qcopilots_HDRS
  qcopilots_container_dock.h
  qcopilots_container_plugin.h
  qcopilots_container_utils.h
  qcopilots_mcp_bridge.h
  qcopilots_mcp_catalog.h
)

set (qcopilots_UIS
  ui/qcopilots_container_dock.ui
)

set (qcopilots_RCCS
  resources.qrc
)

########################################################
# Build

find_package(${QT_VERSION_BASE} COMPONENTS Network WebChannel WebEngineWidgets REQUIRED)

qt_wrap_ui(qcopilots_UIS_H ${qcopilots_UIS})

add_library (plugin_qcopilots MODULE
  ${qcopilots_SRCS}
  ${qcopilots_HDRS}
  ${qcopilots_RCCS}
  ${qcopilots_UIS_H}
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/plugins

  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(plugin_qcopilots
  qgis_core
  qgis_gui
  Qt::Network
  Qt::WebChannel
  Qt::WebEngineWidgets
)

########################################################
# Install

install(TARGETS plugin_qcopilots
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR}
)
