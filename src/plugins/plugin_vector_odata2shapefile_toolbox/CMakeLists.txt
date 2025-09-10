
########################################################
# Files WithSpecification
set (plugin_vector_odata2shapefile_toolbox_SRCS
  ui_class/tool_JBDX2Shapefile_class.cpp
  ui_class/tool_DZB2Shapefile_class.cpp
  ui_class/tool_DZB2ShapefileWithSpecification_class.cpp

  ui_thread/tool_JBDX2Shapefile_thread.cpp
  ui_thread/tool_DZB2Shapefile_thread.cpp
  ui_thread/tool_DZB2ShapefileWithSpecification_thread.cpp

  plugin_vector_odata2shapefile.cpp
)

set (plugin_vector_odata2shapefile_toolbox_HDRS
  ui_class/tool_JBDX2Shapefile_class.h
  ui_class/tool_DZB2Shapefile_class.h
  ui_class/tool_DZB2ShapefileWithSpecification_class.h

  ui_thread/tool_JBDX2Shapefile_thread.h
  ui_thread/tool_DZB2Shapefile_thread.h
  ui_thread/tool_DZB2ShapefileWithSpecification_class_thread.h

  plugin_vector_odata2shapefile.h
)

set (plugin_vector_odata2shapefile_toolbox_UIS
  ui/tool_DZB2Shapefile.ui
  ui/tool_DZB2ShapefileWithSpecification.ui
  ui/tool_JBDX2Shapefile.ui
)

set (plugin_vector_odata2shapefile_toolbox_RCCS
  plugin_vector_odata2shapefile.qrc
)




########################################################
# Build
if (BUILD_WITH_QT6)
  QT6_WRAP_UI (plugin_vector_odata2shapefile_toolbox_UIS_H  ${plugin_vector_odata2shapefile_toolbox_UIS})
else()
  QT5_WRAP_UI (plugin_vector_odata2shapefile_toolbox_UIS_H  ${plugin_vector_odata2shapefile_toolbox_UIS})
endif()

add_library (plugin_vector_odata2shapefile_toolbox MODULE ${plugin_vector_odata2shapefile_toolbox_SRCS}  ${plugin_vector_odata2shapefile_toolbox_RCCS} ${plugin_vector_odata2shapefile_toolbox_UIS_H})

# require c++17
target_compile_features(plugin_vector_odata2shapefile_toolbox PRIVATE cxx_std_17)


# 获取为CMake临时设置的环境变量
set(OSGEO4W_ROOT_for_plugin_vector_odata2shapefile_toolbox $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_plugin_vector_odata2shapefile_toolbox $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_plugin_vector_odata2shapefile_toolbox $ENV{O4W_QT_LIBRARIES})

# 包含头文件
target_include_directories(plugin_vector_odata2shapefile_toolbox SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(plugin_vector_odata2shapefile_toolbox SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_plugin_vector_odata2shapefile_toolbox}/include

  ${O4W_QT_HEADERS_for_plugin_vector_odata2shapefile_toolbox}
  ${O4W_QT_HEADERS_for_plugin_vector_odata2shapefile_toolbox}/QtCore
  ${O4W_QT_HEADERS_for_plugin_vector_odata2shapefile_toolbox}/QtGui
  ${O4W_QT_HEADERS_for_plugin_vector_odata2shapefile_toolbox}/QtWidgets

  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/gui
  ${CMAKE_SOURCE_DIR}/src/plugins

  # 第三方库头文件
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/include

  # 基础算法库头文件
  ${CMAKE_SOURCE_DIR}/src/starmap_src/base_common_h
  ${CMAKE_SOURCE_DIR}/src/starmap_src/base_vector_odata2shapefile

  # 插件头文件
  ${CMAKE_BINARY_DIR}/src/plugins/plugin_vector_odata2shapefile_toolbox
)


# 包含库文件
target_link_libraries(plugin_vector_odata2shapefile_toolbox

  ${OSGEO4W_ROOT_for_plugin_vector_odata2shapefile_toolbox}/lib/gdal_i.lib

  ${O4W_QT_LIBRARIES_for_plugin_vector_odata2shapefile_toolbox}/Qt5Gui.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_odata2shapefile_toolbox}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_odata2shapefile_toolbox}/Qt5Widgets.lib

  qgis_core
  qgis_gui

  ${CMAKE_BINARY_DIR}/src/starmap_src/base_vector_odata2shapefile/RelWithDebInfo/base_vector_odata2shapefile.lib
)

# 添加预处理器定义
target_compile_definitions(plugin_vector_odata2shapefile_toolbox PRIVATE "-DOS_FAMILY_WINDOWS" "-D_HAS_STD_BYTE=0")

########################################################
# Install

install(TARGETS plugin_vector_odata2shapefile_toolbox
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})

