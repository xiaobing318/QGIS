
########################################################
# Files
set (odata_format_conversion_toolbox_SRCS
  ui_class/se_annotation_pointer_reverse_extraction.cpp
  ui_class/se_odata2shp.cpp
  ui_class/se_pbf2gpkg.cpp
  ui_class/se_text_files_gbk2utf8.cpp

  ui_thread/se_annotation_pointer_reverse_extraction_thread.cpp
  ui_thread/se_odata2shp_thread.cpp


  odata_format_conversion_plugin.cpp
)

set (odata_format_conversion_toolbox_HDRS
  ui_class/se_annotation_pointer_reverse_extraction.h
  ui_class/se_odata2shp.h
  ui_class/se_pbf2gpkg.h
  ui_class/se_text_files_gbk2utf8.h

  ui_thread/se_annotation_pointer_reverse_extraction_thread.h
  ui_thread/se_odata2shp_thread.h

  odata_format_conversion_plugin.h
)

set (odata_format_conversion_toolbox_UIS
  ui/se_annotation_pointer_reverse_extraction.ui
  ui/se_odata2shp.ui
  ui/se_pbf2gpkg.ui
  ui/se_text_files_gbk2utf8.ui
)

set (odata_format_conversion_toolbox_RCCS
  odata_format_conversion_plugin.qrc
)




########################################################
# Build
if (BUILD_WITH_QT6)
  QT6_WRAP_UI (odata_format_conversion_toolbox_UIS_H  ${odata_format_conversion_toolbox_UIS})
else()
  QT5_WRAP_UI (odata_format_conversion_toolbox_UIS_H  ${odata_format_conversion_toolbox_UIS})
endif()

add_library (plugin_odata_format_conversion_toolbox MODULE ${odata_format_conversion_toolbox_SRCS}  ${odata_format_conversion_toolbox_RCCS} ${odata_format_conversion_toolbox_UIS_H})

# require c++17
target_compile_features(plugin_odata_format_conversion_toolbox PRIVATE cxx_std_17)


# -2024-03-27：获取为CMake临时设置的环境变量
set(OSGEO4W_ROOT_for_plugin_odata_format_conversion_toolbox $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_plugin_odata_format_conversion_toolbox $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_plugin_odata_format_conversion_toolbox $ENV{O4W_QT_LIBRARIES})

# -2024-03-27：包含头文件 
target_include_directories(plugin_odata_format_conversion_toolbox SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(plugin_odata_format_conversion_toolbox SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_plugin_odata_format_conversion_toolbox}/include

  ${O4W_QT_HEADERS_for_plugin_odata_format_conversion_toolbox}
  ${O4W_QT_HEADERS_for_plugin_odata_format_conversion_toolbox}/QtCore
  ${O4W_QT_HEADERS_for_plugin_odata_format_conversion_toolbox}/QtGui
  ${O4W_QT_HEADERS_for_plugin_odata_format_conversion_toolbox}/QtWidgets

  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/gui
  ${CMAKE_SOURCE_DIR}/src/plugins

  # 第三方库头文件
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/include

  # 基础算法库头文件
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
  ${CMAKE_SOURCE_DIR}/src/starmap_src/base_vector_format_conversion

  # 插件头文件
  ${CMAKE_BINARY_DIR}/src/plugins/odata_format_conversion_toolbox
  
)


# -2024-03-27：包含库文件
target_link_libraries(plugin_odata_format_conversion_toolbox

  ${OSGEO4W_ROOT_for_plugin_odata_format_conversion_toolbox}/lib/gdal_i.lib

  ${O4W_QT_LIBRARIES_for_plugin_odata_format_conversion_toolbox}/Qt5Gui.lib
  ${O4W_QT_LIBRARIES_for_plugin_odata_format_conversion_toolbox}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_plugin_odata_format_conversion_toolbox}/Qt5Widgets.lib

  qgis_core
  qgis_gui

  ${CMAKE_BINARY_DIR}/src/starmap_src/base_vector_format_conversion/RelWithDebInfo/base_vector_format_conversion.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv.lib

)

# -2024-03-27：添加预处理器定义
target_compile_definitions(plugin_odata_format_conversion_toolbox PRIVATE "-DOS_FAMILY_WINDOWS" "-D_HAS_STD_BYTE=0")

########################################################
# Install

install(TARGETS plugin_odata_format_conversion_toolbox
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})

