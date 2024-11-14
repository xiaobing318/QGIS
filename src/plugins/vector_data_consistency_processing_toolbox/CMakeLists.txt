
########################################################
# Files

set (vector_data_consistency_processing_toolbox_SRCS

  #一致性匹配工具（自动化）
  consistency_matching_tool/consistency_matching_tool.cpp
  consistency_matching_tool/consistency_processing_base.cpp

  #一致性属连接处理工具（人工交互）

  #一致性匹配表单检查工具（人工交互）

  #一致性几何编辑处理工具（人机交互）

  #其他工具

  #矢量语义融合处理工具箱
  consistency_processing_toolbox_plugin.cpp

)

set (vector_data_consistency_processing_toolbox_HDRS

  #一致性匹配工具（自动化）
  consistency_matching_tool/consistency_matching_tool.h
  consistency_matching_tool/consistency_processing_base.h

  #一致性属连接处理工具（人工交互）

  #一致性匹配表单检查工具（人工交互）

  #一致性几何编辑处理工具（人机交互）

  #其他工具

  #矢量语义融合处理工具箱
  consistency_processing_toolbox_plugin.h

)

set (vector_data_consistency_processing_toolbox_UIS

  #一致性匹配工具（自动化）
  consistency_matching_tool/consistency_matching_tool_guibase.ui

  #一致性属连接处理工具（人工交互）

  #一致性匹配表单检查工具（人工交互）

  #一致性几何编辑处理工具（人机交互）

  #其他工具

  #矢量语义融合处理工具箱
  
)

set (vector_data_consistency_processing_toolbox_RCCS
  consistency_processing_toolbox_plugin.qrc
)

########################################################
# Build

if (BUILD_WITH_QT6)
  QT6_WRAP_UI (vector_data_consistency_processing_toolbox_UIS_H  ${vector_data_consistency_processing_toolbox_UIS})
else()
  QT5_WRAP_UI (vector_data_consistency_processing_toolbox_UIS_H  ${vector_data_consistency_processing_toolbox_UIS})
endif()

add_library (plugin_vector_data_consistency_processing_toolbox MODULE ${vector_data_consistency_processing_toolbox_SRCS}  ${vector_data_consistency_processing_toolbox_RCCS} ${vector_data_consistency_processing_toolbox_UIS_H})

# require c++17
target_compile_features(plugin_vector_data_consistency_processing_toolbox PRIVATE cxx_std_17)


# 杨小兵-2024-02-29：获取为CMake临时设置的环境变量
set(OSGEO4W_ROOT_for_plugin_vector_data_consistency_processing_toolbox $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_plugin_vector_data_consistency_processing_toolbox $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_plugin_vector_data_consistency_processing_toolbox $ENV{O4W_QT_LIBRARIES})

#杨小兵-2024-03-27：包含头文件
target_include_directories(plugin_vector_data_consistency_processing_toolbox SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(plugin_vector_data_consistency_processing_toolbox SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_plugin_vector_data_consistency_processing_toolbox}/include

  ${O4W_QT_HEADERS_for_plugin_vector_data_consistency_processing_toolbox}
  ${O4W_QT_HEADERS_for_plugin_vector_data_consistency_processing_toolbox}/QtCore
  ${O4W_QT_HEADERS_for_plugin_vector_data_consistency_processing_toolbox}/QtGui
  ${O4W_QT_HEADERS_for_plugin_vector_data_consistency_processing_toolbox}/QtXml
  ${O4W_QT_HEADERS_for_plugin_vector_data_consistency_processing_toolbox}/QtWidgets

  ${CMAKE_BINARY_DIR}/src/core
  ${CMAKE_BINARY_DIR}/src/gui
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_BINARY_DIR}

  #工具头文件
  ${CMAKE_CURRENT_SOURCE_DIR}/consistency_attribute_connection_processing_tool
  ${CMAKE_CURRENT_SOURCE_DIR}/consistency_matching_form_checking_tool
  ${CMAKE_CURRENT_SOURCE_DIR}/consistency_matching_tool
  ${CMAKE_CURRENT_SOURCE_DIR}/consistent_geometry_editing_and_processing_tool

  #基础库头文件
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h

  #生成插件头文件
  ${CMAKE_BINARY_DIR}/src/plugins/vector_data_consistency_processing_toolbox
  ${CMAKE_SOURCE_DIR}/src/plugins/vector_data_consistency_processing_toolbox

  #第三方库头文件
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/include
)


#杨小兵-2024-03-27：包含库文件
target_link_libraries(plugin_vector_data_consistency_processing_toolbox

  ${OSGEO4W_ROOT_for_plugin_vector_data_consistency_processing_toolbox}/lib/gdal_i.lib

  ${O4W_QT_LIBRARIES_for_plugin_vector_data_consistency_processing_toolbox}/Qt5Gui.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_consistency_processing_toolbox}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_consistency_processing_toolbox}/Qt5Xml.lib

  qgis_core
  qgis_gui

  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv.lib

  ${CMAKE_BINARY_DIR}/src/starmap_src/base_vector_feature_matching/RelWithDebInfo/base_vector_feature_matching.lib
)

target_compile_definitions(plugin_vector_data_consistency_processing_toolbox PRIVATE "-DOS_FAMILY_WINDOWS" "-D_HAS_STD_BYTE=0")

########################################################
# Install

install(TARGETS plugin_vector_data_consistency_processing_toolbox
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})

