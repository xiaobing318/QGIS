if (NOT WITH_QTWEBENGINE)
  message(STATUS "Skipping llama web UI plugin (QtWebEngine disabled)")
  return()
endif()

set (llama_webui_SRCS
  qgsllamawebuiplugin.cpp
  qgsllamawebuidock.cpp
)

set (llama_webui_HDRS
  qgsllamawebuiplugin.h
  qgsllamawebuidock.h
)

add_library (plugin_llama_webui MODULE ${llama_webui_SRCS})

# require c++17
target_compile_features(plugin_llama_webui PRIVATE cxx_std_17)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(plugin_llama_webui
  qgis_core
  qgis_gui
  Qt${QT_VERSION_MAJOR}::WebEngineWidgets
)

install(TARGETS plugin_llama_webui
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})
