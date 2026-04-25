set (qcopilots_server_SRCS
  qgsqcopilotsserverplugin.cpp
)

set (qcopilots_server_HDRS
  qgsqcopilotsserverplugin.h
)

set (qcopilots_server_RCCS
  resources.qrc
)

add_library(plugin_qcopilots_server MODULE
  ${qcopilots_server_SRCS}
  ${qcopilots_server_HDRS}
  ${qcopilots_server_RCCS}
)

target_compile_features(plugin_qcopilots_server PRIVATE cxx_std_17)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(plugin_qcopilots_server
  qgis_core
  qgis_gui
  Qt${QT_VERSION_MAJOR}::Network
)

install(TARGETS plugin_qcopilots_server
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR}
)
