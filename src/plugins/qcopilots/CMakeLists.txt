# 定义插件的源文件
set (qcopilots_SRCS
  qgsqcopilotsplugin.cpp
  qgsqcopilotsdock.cpp
)
# 定义插件的头文件
set (qcopilots_HDRS
  qgsqcopilotsplugin.h
  qgsqcopilotsdock.h
)
# 定义插件的UI文件
set(qcopilots_UIS
  ui/qgsqcopilotsdock.ui
)
# 定义插件的资源文件
set (qcopilots_RCCS
  qgsqcopilotsplugin_resources.qrc
)

# 生成UI头文件、构建插件库
qt_wrap_ui(qcopilots_UIS_H ${qcopilots_UIS})
add_library(plugin_qcopilots MODULE
  ${qcopilots_SRCS}
  ${qcopilots_HDRS}
  ${qcopilots_RCCS}
  ${qcopilots_UIS_H}
)

# require c++17
target_compile_features(plugin_qcopilots PRIVATE cxx_std_17)

# 包含必要的目录
include_directories(
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(plugin_qcopilots
  qgis_core
  qgis_gui
  Qt${QT_VERSION_MAJOR}::WebEngineWidgets
)

install(TARGETS plugin_qcopilots
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})
