########################################################
# Files

set (CopilotsToolbox_SRCS
  PluginCopilotsToolbox.cpp
)

set (CopilotsToolbox_HDRS
  PluginCopilotsToolbox.h
)

set (CopilotsToolbox_UIS
  # 这里不需要添加任何东西，因为我们没有任何UI文件
)

set (CopilotsToolbox_RCCS
  PluginCopilotsToolbox.qrc
)

########################################################
# Build

if (BUILD_WITH_QT6)
  QT6_WRAP_UI (CopilotsToolbox_UIS_H  ${CopilotsToolbox_UIS})
else()
  QT5_WRAP_UI (CopilotsToolbox_UIS_H  ${CopilotsToolbox_UIS})
endif()

add_library (plugin_Copilots_toolbox MODULE ${CopilotsToolbox_SRCS}  ${CopilotsToolbox_RCCS} ${CopilotsToolbox_UIS_H})

# require c++17
target_compile_features(plugin_Copilots_toolbox PRIVATE cxx_std_17)


# 获取环境变量配置
set(OSGEO4W_ROOT_for_plugin_Copilots_toolbox $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_plugin_Copilots_toolbox $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_plugin_Copilots_toolbox $ENV{O4W_QT_LIBRARIES})

# 包含头文件配置
target_include_directories(plugin_Copilots_toolbox SYSTEM PRIVATE
  # 当前源码和构建目录
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}

  # OSGEO4W 环境头文件
  ${OSGEO4W_ROOT_for_plugin_Copilots_toolbox}/include

  # Qt 头文件目录
  ${O4W_QT_HEADERS_for_plugin_Copilots_toolbox}
  ${O4W_QT_HEADERS_for_plugin_Copilots_toolbox}/QtCore
  ${O4W_QT_HEADERS_for_plugin_Copilots_toolbox}/QtGui
  ${O4W_QT_HEADERS_for_plugin_Copilots_toolbox}/QtXml
  ${O4W_QT_HEADERS_for_plugin_Copilots_toolbox}/QtWidgets

  # QGIS 相关头文件目录
  ${CMAKE_BINARY_DIR}/src/core
  ${CMAKE_BINARY_DIR}/src/gui
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_BINARY_DIR}

  # 插件子模块目录
  ${CMAKE_CURRENT_SOURCE_DIR}/DLHJ_Copilots
)


# 链接库文件配置
if (BUILD_WITH_QT6)
  target_link_libraries(plugin_Copilots_toolbox PRIVATE
    # Qt6 库文件
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Xml

    # QGIS 库文件
    qgis_core
    qgis_gui
  )
else()
  target_link_libraries(plugin_Copilots_toolbox PRIVATE
    # Qt5 库文件
    ${O4W_QT_LIBRARIES_for_plugin_Copilots_toolbox}/Qt5Core.lib
    ${O4W_QT_LIBRARIES_for_plugin_Copilots_toolbox}/Qt5Gui.lib
    ${O4W_QT_LIBRARIES_for_plugin_Copilots_toolbox}/Qt5Widgets.lib
    ${O4W_QT_LIBRARIES_for_plugin_Copilots_toolbox}/Qt5Xml.lib

    # QGIS 库文件
    qgis_core
    qgis_gui
  )
endif()

target_compile_definitions(plugin_Copilots_toolbox PRIVATE "-DOS_FAMILY_WINDOWS" "-DDLLPROJECT_EXPORT")

########################################################
# Install

install(TARGETS plugin_Copilots_toolbox
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})

