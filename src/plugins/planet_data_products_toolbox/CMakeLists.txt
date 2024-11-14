
#######################################################Files###################################################
# Files
set (planet_data_products_toolbox_SRCS
  #1、矢量数据翻译工具
  vector_data_translation_tool/vector_data_translation_tool.cpp

  #2、分析就绪数据处理工具(临时的)
  analysis_ready_data_process_tool/cse_analysisreadydataprocess_tool.cpp
  analysis_ready_data_process_tool/cse_config_file_info_table.cpp


  #3、点要素分级处理工具
  point_feature_hierarchical_processing_tool/point_feature_hierarchical_processing_tool.cpp

  #3、星球数据产品工具箱插件
  planet_data_products_toolbox_plugin.cpp

)

set (planet_data_products_toolbox_HDRS
  #1、矢量数据翻译工具
  vector_data_translation_tool/vector_data_translation_tool.h

  #2、分析就绪数据处理工具(临时的)
  analysis_ready_data_process_tool/cse_analysisreadydataprocess_tool.h
  analysis_ready_data_process_tool/cse_config_file_info_table.h

  #3、点要素分级处理工具
  point_feature_hierarchical_processing_tool/point_feature_hierarchical_processing_tool.h

  #3、星球数据产品工具箱插件
  planet_data_products_toolbox_plugin.h

)

set (planet_data_products_toolbox_UIS
  #1、矢量数据翻译工具
  vector_data_translation_tool/vector_data_translation_tool.ui

  #2、分析就绪数据处理工具(临时的)
  analysis_ready_data_process_tool/cse_analysisreadydataprocess_tool.ui
  analysis_ready_data_process_tool/cse_config_file_info_table.ui

  #3、点要素分级处理工具
  point_feature_hierarchical_processing_tool/point_feature_hierarchical_processing_tool.ui

  #3、星球数据产品工具箱插件
)

set (planet_data_products_toolbox_RCCS
  planet_data_products_toolbox_plugin.qrc
)
#######################################################Files###################################################







#######################################################Build###################################################
# Build
if (BUILD_WITH_QT6)
  QT6_WRAP_UI (planet_data_products_toolbox_UIS_H  ${planet_data_products_toolbox_UIS})
else()
  QT5_WRAP_UI (planet_data_products_toolbox_UIS_H  ${planet_data_products_toolbox_UIS})
endif()

add_library (plugin_planet_data_products_toolbox MODULE 
  ${planet_data_products_toolbox_SRCS}  
  ${planet_data_products_toolbox_RCCS} 
  ${planet_data_products_toolbox_UIS_H})

# require c++17
target_compile_features(plugin_planet_data_products_toolbox PRIVATE cxx_std_17)


# 杨小兵-2024-02-29：获取为CMake临时设置的环境变量
set(OSGEO4W_ROOT_for_plugin_planet_data_products_toolbox $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_plugin_planet_data_products_toolbox $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_plugin_planet_data_products_toolbox $ENV{O4W_QT_LIBRARIES})

#杨小兵-2024-03-27：包含头文件
target_include_directories(plugin_planet_data_products_toolbox PRIVATE
  ${OSGEO4W_ROOT_for_plugin_planet_data_products_toolbox}/include

  ${O4W_QT_HEADERS_for_plugin_planet_data_products_toolbox}
  ${O4W_QT_HEADERS_for_plugin_planet_data_products_toolbox}/QtCore
  ${O4W_QT_HEADERS_for_plugin_planet_data_products_toolbox}/QtGui
  ${O4W_QT_HEADERS_for_plugin_planet_data_products_toolbox}/QtXml
  ${O4W_QT_HEADERS_for_plugin_planet_data_products_toolbox}/QtWidgets

  ${CMAKE_BINARY_DIR}/src/core
  ${CMAKE_BINARY_DIR}/src/gui
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_BINARY_DIR}

  #星球数据产品工具箱插件头文件
  ${CMAKE_CURRENT_SOURCE_DIR}

  #1、矢量数据翻译工具头文件
  ${CMAKE_CURRENT_SOURCE_DIR}/vector_data_translation_tool

  #2、分析就绪数据处理工具头文件（临时的）
  ${CMAKE_CURRENT_SOURCE_DIR}/analysis_ready_data_process_tool

  #3、点要素分级处理工具头文件
  ${CMAKE_CURRENT_SOURCE_DIR}/point_feature_hierarchical_processing_tool
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
  ${CMAKE_SOURCE_DIR}/src/starmap_src/base_point_feature_hierarchical_processing

  #由UI文件产生的.h头文件
  ${CMAKE_BINARY_DIR}/src/plugins/planet_data_products_toolbox

  #基础库头文件


  #第三方库头文件
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/include
  ${CMAKE_SOURCE_DIR}/external/starmap_external/curl-8.8.0/include
)


#杨小兵-2024-03-27：包含库文件
target_link_libraries(plugin_planet_data_products_toolbox

  ${OSGEO4W_ROOT_for_plugin_planet_data_products_toolbox}/lib/gdal_i.lib

  ${O4W_QT_LIBRARIES_for_plugin_planet_data_products_toolbox}/Qt5Gui.lib
  ${O4W_QT_LIBRARIES_for_plugin_planet_data_products_toolbox}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_plugin_planet_data_products_toolbox}/Qt5Xml.lib

  qgis_core
  qgis_gui

  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/curl-8.8.0/lib/libcurl_imp.lib
  ${CMAKE_BINARY_DIR}/src/starmap_src/base_point_feature_hierarchical_processing/RelWithDebInfo/base_point_feature_hierarchical_processing.lib
)
#######################################################Build###################################################


target_compile_definitions(plugin_planet_data_products_toolbox PRIVATE "-DOS_FAMILY_WINDOWS" "-D_HAS_STD_BYTE=0")

#######################################################Install###################################################
# Install
install(TARGETS plugin_planet_data_products_toolbox
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})
#######################################################Install###################################################

