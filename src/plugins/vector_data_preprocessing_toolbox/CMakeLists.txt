
########################################################
# Files

set (vector_data_preprocessing_toolbox_SRCS
  #矢量数据坐标转化工具
  vector_data_coordinate_transformation/se_geocoordsys_trans.cpp
  vector_data_coordinate_transformation/se_geocoordsys_trans_thread.cpp

  #源数据检查工具
  source_data_inspection_tool/source_data_inspection_tool.cpp
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_lib/character_set_encoding/character_set_encoding_conversion.cpp

  #其他工具

  #矢量语义融合处理工具箱
  vector_data_preprocessing_toolbox_plugin.cpp
)

set (vector_data_preprocessing_toolbox_HDRS
  #矢量数据坐标转化工具
  vector_data_coordinate_transformation/se_geocoordsys_trans.h
  vector_data_coordinate_transformation/se_geocoordsys_trans_thread.h

  #源数据检查工具
  source_data_inspection_tool/source_data_inspection_tool.h
  source_data_inspection_tool/source_data_inspection_tool_data_structure.h
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h/character_set_encoding/character_set_encoding_conversion.h

  #其他工具

  #矢量语义融合处理工具箱
  vector_data_preprocessing_toolbox_plugin.h
)

set (vector_data_preprocessing_toolbox_UIS
  #矢量数据坐标转化工具
  vector_data_coordinate_transformation/se_geocoordsys_trans.ui

  #源数据检查工具
  source_data_inspection_tool/source_data_inspection_tool.ui

  #其他工具
)

set (vector_data_preprocessing_toolbox_RCCS
  vector_data_preprocessing_toolbox_plugin.qrc
)

########################################################
# Build

if (BUILD_WITH_QT6)
  QT6_WRAP_UI (vector_data_preprocessing_toolbox_UIS_H  ${vector_data_preprocessing_toolbox_UIS})
else()
  QT5_WRAP_UI (vector_data_preprocessing_toolbox_UIS_H  ${vector_data_preprocessing_toolbox_UIS})
endif()

add_library (plugin_vector_data_preprocessing_toolbox MODULE ${vector_data_preprocessing_toolbox_SRCS}  ${vector_data_preprocessing_toolbox_RCCS} ${vector_data_preprocessing_toolbox_UIS_H})

# require c++17
target_compile_features(plugin_vector_data_preprocessing_toolbox PRIVATE cxx_std_17)


# 杨小兵-2024-02-29：获取为CMake临时设置的环境变量
set(OSGEO4W_ROOT_for_plugin_vector_data_preprocessing_toolbox $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_plugin_vector_data_preprocessing_toolbox $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_plugin_vector_data_preprocessing_toolbox $ENV{O4W_QT_LIBRARIES})

#杨小兵-2024-03-27：包含头文件
target_include_directories(plugin_vector_data_preprocessing_toolbox SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(plugin_vector_data_preprocessing_toolbox SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_plugin_vector_data_preprocessing_toolbox}/include

  ${O4W_QT_HEADERS_for_plugin_vector_data_preprocessing_toolbox}
  ${O4W_QT_HEADERS_for_plugin_vector_data_preprocessing_toolbox}/QtCore
  ${O4W_QT_HEADERS_for_plugin_vector_data_preprocessing_toolbox}/QtGui
  ${O4W_QT_HEADERS_for_plugin_vector_data_preprocessing_toolbox}/QtXml
  ${O4W_QT_HEADERS_for_plugin_vector_data_preprocessing_toolbox}/QtWidgets

  ${CMAKE_BINARY_DIR}/src/core
  ${CMAKE_BINARY_DIR}/src/gui
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_BINARY_DIR}

  ${CMAKE_CURRENT_SOURCE_DIR}/vector_data_coordinate_transformation
  ${CMAKE_CURRENT_SOURCE_DIR}/source_data_inspection_tool

  #基础库头文件
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
  ${CMAKE_SOURCE_DIR}/src/starmap_src/base_coordinate_transformation

  #输出插件头文件
  ${CMAKE_BINARY_DIR}/src/plugins/vector_data_preprocessing_toolbox

  #第三方库头文件
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/include
)


#杨小兵-2024-03-27：包含库文件
target_link_libraries(plugin_vector_data_preprocessing_toolbox

  ${OSGEO4W_ROOT_for_plugin_vector_data_preprocessing_toolbox}/lib/gdal_i.lib

  ${O4W_QT_LIBRARIES_for_plugin_vector_data_preprocessing_toolbox}/Qt5Gui.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_preprocessing_toolbox}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_preprocessing_toolbox}/Qt5Xml.lib

  qgis_core
  qgis_gui

  ${CMAKE_BINARY_DIR}/src/starmap_src/base_coordinate_transformation/RelWithDebInfo/base_coordinate_transformation.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv.lib

)

target_compile_definitions(plugin_vector_data_preprocessing_toolbox PRIVATE "-DOS_FAMILY_WINDOWS" "-D_HAS_STD_BYTE=0")

########################################################
# Install

install(TARGETS plugin_vector_data_preprocessing_toolbox
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})

