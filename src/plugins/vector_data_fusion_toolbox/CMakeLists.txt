
########################################################
# Files

set (vector_data_fusion_toolbox_SRCS

  # 第一类：语义融合处理工具集（七种）
  vector_data_semantic_fusion/JJBDX_NJBDX_semantic_fusion.cpp
  vector_data_semantic_fusion/JJBDX_NJBDX_Config_Reader.cpp

  vector_data_semantic_fusion/DZHT_NJBDX_semantic_fusion.cpp
  vector_data_semantic_fusion/DZHT_NJBDX_Config_Reader.cpp

  vector_data_semantic_fusion/JBHT_NJBDX_semantic_fusion.cpp
  vector_data_semantic_fusion/JBHT_NJBDX_Config_Reader.cpp

  vector_data_semantic_fusion/JYDH_NJBDX_semantic_fusion.cpp
  vector_data_semantic_fusion/JYDH_NJBDX_Config_Reader.cpp

  vector_data_semantic_fusion/OSM_NJBDX_semantic_fusion.cpp
  vector_data_semantic_fusion/OSM_NJBDX_Config_Reader.cpp

  vector_data_semantic_fusion/QQCT_NJBDX_semantic_fusion.cpp
  vector_data_semantic_fusion/QQCT_NJBDX_Config_Reader.cpp

  vector_data_semantic_fusion/SW_NJBDX_semantic_fusion.cpp
  vector_data_semantic_fusion/SW_NJBDX_Config_Reader.cpp


  # 第二类：矢量要素数据提取工具
  vector_data_feature_extraction/attribute_extract.cpp
  vector_data_feature_extraction/attribute_extract_thread.cpp

  # 其他工具

  # 矢量语义融合处理工具箱
  vector_data_fusion_toolbox_plugin.cpp
)

set (vector_data_fusion_toolbox_HDRS
  # 第一类：语义融合处理工具集（七种）
  vector_data_semantic_fusion/JJBDX_NJBDX_semantic_fusion.h
  vector_data_semantic_fusion/JJBDX_NJBDX_Config_Reader.h
  vector_data_semantic_fusion/JJBDX_NJBDX_semantic_fusion_common.h

  vector_data_semantic_fusion/DZHT_NJBDX_semantic_fusion.h
  vector_data_semantic_fusion/DZHT_NJBDX_Config_Reader.h
  vector_data_semantic_fusion/DZHT_NJBDX_semantic_fusion_common.h

  vector_data_semantic_fusion/JBHT_NJBDX_semantic_fusion.h
  vector_data_semantic_fusion/JBHT_NJBDX_Config_Reader.h
  vector_data_semantic_fusion/JBHT_NJBDX_semantic_fusion_common.h

  vector_data_semantic_fusion/JYDH_NJBDX_semantic_fusion.h
  vector_data_semantic_fusion/JYDH_NJBDX_Config_Reader.h
  vector_data_semantic_fusion/JYDH_NJBDX_semantic_fusion_common.h

  vector_data_semantic_fusion/OSM_NJBDX_semantic_fusion.h
  vector_data_semantic_fusion/OSM_NJBDX_Config_Reader.h
  vector_data_semantic_fusion/OSM_NJBDX_semantic_fusion_common.h

  vector_data_semantic_fusion/QQCT_NJBDX_semantic_fusion.h
  vector_data_semantic_fusion/QQCT_NJBDX_Config_Reader.h
  vector_data_semantic_fusion/QQCT_NJBDX_semantic_fusion_common.h

  vector_data_semantic_fusion/SW_NJBDX_semantic_fusion.h
  vector_data_semantic_fusion/SW_NJBDX_Config_Reader.h
  vector_data_semantic_fusion/SW_NJBDX_semantic_fusion_common.h
  
  # 第二类：矢量要素数据提取工具
  vector_data_feature_extraction/attribute_extract.h
  vector_data_feature_extraction/attribute_extract_thread.h

  # 其他工具

  # 矢量语义融合处理工具箱
  vector_data_fusion_toolbox_plugin.h
)

set (vector_data_fusion_toolbox_UIS
  # 第一类：语义融合处理工具集（七种）
  vector_data_semantic_fusion/JJBDX_NJBDX_semantic_fusion.ui
  vector_data_semantic_fusion/DZHT_NJBDX_semantic_fusion.ui
  vector_data_semantic_fusion/JBHT_NJBDX_semantic_fusion.ui
  vector_data_semantic_fusion/JYDH_NJBDX_semantic_fusion.ui
  vector_data_semantic_fusion/OSM_NJBDX_semantic_fusion.ui
  vector_data_semantic_fusion/QQCT_NJBDX_semantic_fusion.ui
  vector_data_semantic_fusion/SW_NJBDX_semantic_fusion.ui

  # 第二类：矢量要素数据提取工具
  vector_data_feature_extraction/attribute_extract.ui

  # 其他工具
)

set (vector_data_fusion_toolbox_RCCS
  vector_data_fusion_toolbox_plugin.qrc
)



########################################################
# Build

if (BUILD_WITH_QT6)
  QT6_WRAP_UI (vector_data_fusion_toolbox_UIS_H  ${vector_data_fusion_toolbox_UIS})
else()
  QT5_WRAP_UI (vector_data_fusion_toolbox_UIS_H  ${vector_data_fusion_toolbox_UIS})
endif()

add_library (plugin_vector_data_fusion_toolbox MODULE ${vector_data_fusion_toolbox_SRCS}  ${vector_data_fusion_toolbox_RCCS} ${vector_data_fusion_toolbox_UIS_H})

# require c++17
target_compile_features(plugin_vector_data_fusion_toolbox PRIVATE cxx_std_17)


# 杨小兵-2024-02-29：获取为CMake临时设置的环境变量
set(OSGEO4W_ROOT_for_plugin_vector_data_fusion_toolbox $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_plugin_vector_data_fusion_toolbox $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_plugin_vector_data_fusion_toolbox $ENV{O4W_QT_LIBRARIES})

#杨小兵-2024-03-27：包含头文件
target_include_directories(plugin_vector_data_fusion_toolbox SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(plugin_vector_data_fusion_toolbox SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_plugin_vector_data_fusion_toolbox}/include

  ${O4W_QT_HEADERS_for_plugin_vector_data_fusion_toolbox}
  ${O4W_QT_HEADERS_for_plugin_vector_data_fusion_toolbox}/QtCore
  ${O4W_QT_HEADERS_for_plugin_vector_data_fusion_toolbox}/QtGui
  ${O4W_QT_HEADERS_for_plugin_vector_data_fusion_toolbox}/QtXml
  ${O4W_QT_HEADERS_for_plugin_vector_data_fusion_toolbox}/QtWidgets

  ${CMAKE_BINARY_DIR}/src/core
  ${CMAKE_BINARY_DIR}/src/gui
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_BINARY_DIR}

  # 工具头文件
  ${CMAKE_CURRENT_SOURCE_DIR}/vector_data_semantic_fusion
  ${CMAKE_CURRENT_SOURCE_DIR}/vector_data_feature_extraction

  # 基础库头文件
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
  ${CMAKE_SOURCE_DIR}/src/starmap_src/base_geoextractandprocess

  # 生成插件头文件
  ${CMAKE_BINARY_DIR}/src/plugins/vector_data_fusion_toolbox

  # 第三方库头文件
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/include
)


# 杨小兵-2025-01-13：包含库文件
target_link_libraries(plugin_vector_data_fusion_toolbox

  ${OSGEO4W_ROOT_for_plugin_vector_data_fusion_toolbox}/lib/gdal_i.lib

  ${O4W_QT_LIBRARIES_for_plugin_vector_data_fusion_toolbox}/Qt5Gui.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_fusion_toolbox}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_fusion_toolbox}/Qt5Xml.lib

  qgis_core
  qgis_gui

  ${CMAKE_BINARY_DIR}/src/starmap_src/base_geoextractandprocess/RelWithDebInfo/base_geoextractandprocess.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv.lib

)

target_compile_definitions(plugin_vector_data_fusion_toolbox PRIVATE "-DOS_FAMILY_WINDOWS" "-D_HAS_STD_BYTE=0")


########################################################
# Install

install(TARGETS plugin_vector_data_fusion_toolbox
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})
