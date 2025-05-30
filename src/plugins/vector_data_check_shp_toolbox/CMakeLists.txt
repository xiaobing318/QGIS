
########################################################
# Files
set (vector_data_check_shp_toolbox_SRCS
  ui_class/se_feature_attribute_check.cpp
  ui_class/se_feature_attribute_check_yth.cpp
  ui_class/se_feature_category_statistics.cpp
  ui_class/se_feature_extent_check.cpp
  ui_class/se_layer_integrity_check.cpp
  ui_class/se_layer_validity_check.cpp

  ui_thread/se_feature_attribute_check_thread.cpp
  ui_thread/se_feature_attribute_check_thread_yth.cpp
  ui_thread/se_feature_category_check_thread.cpp
  ui_thread/se_feature_extent_check_thread.cpp
  ui_thread/se_layer_integrity_check_thread.cpp
  ui_thread/se_layer_validity_check_thread.cpp

  vector_data_check_shp_plugin.cpp
)

set (vector_data_check_shp_toolbox_HDRS
  ui_class/se_feature_attribute_check.h
  ui_class/se_feature_attribute_check_yth.h
  ui_class/se_feature_category_statistics.h
  ui_class/se_feature_extent_check.h
  ui_class/se_layer_integrity_check.h
  ui_class/se_layer_validity_check.h

  ui_thread/se_feature_attribute_check_thread.h
  ui_thread/se_feature_attribute_check_thread_yth.h
  ui_thread/se_feature_category_check_thread.h
  ui_thread/se_feature_extent_check_thread.h
  ui_thread/se_layer_integrity_check_thread.h
  ui_thread/se_layer_validity_check_thread.h

  vector_data_check_shp_plugin.h
)

set (vector_data_check_shp_toolbox_UIS
  ui/se_feature_attribute_check.ui
  ui/se_feature_attribute_check_yth.ui
  ui/se_feature_category_statistics.ui
  ui/se_feature_extent_check.ui
  ui/se_layer_integrity_check.ui
  ui/se_layer_validity_check.ui
)

set (vector_data_check_shp_toolbox_RCCS
  vector_data_check_shp_plugin.qrc
)




########################################################
# Build
if (BUILD_WITH_QT6)
  QT6_WRAP_UI (vector_data_check_shp_toolbox_UIS_H  ${vector_data_check_shp_toolbox_UIS})
else()
  QT5_WRAP_UI (vector_data_check_shp_toolbox_UIS_H  ${vector_data_check_shp_toolbox_UIS})
endif()

add_library (plugin_vector_data_check_shp_toolbox MODULE ${vector_data_check_shp_toolbox_SRCS}  ${vector_data_check_shp_toolbox_RCCS} ${vector_data_check_shp_toolbox_UIS_H})

# require c++17
target_compile_features(plugin_vector_data_check_shp_toolbox PRIVATE cxx_std_17)


# 杨小兵-2024-03-27：获取为CMake临时设置的环境变量
set(OSGEO4W_ROOT_for_plugin_vector_data_check_shp_toolbox $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_plugin_vector_data_check_shp_toolbox $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_plugin_vector_data_check_shp_toolbox $ENV{O4W_QT_LIBRARIES})

# 杨小兵-2024-03-27：包含头文件 
target_include_directories(plugin_vector_data_check_shp_toolbox SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(plugin_vector_data_check_shp_toolbox SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_plugin_vector_data_check_shp_toolbox}/include
  ${OSGEO4W_ROOT_for_plugin_vector_data_check_shp_toolbox}/include/libxml2

  ${O4W_QT_HEADERS_for_plugin_vector_data_check_shp_toolbox}
  ${O4W_QT_HEADERS_for_plugin_vector_data_check_shp_toolbox}/QtCore
  ${O4W_QT_HEADERS_for_plugin_vector_data_check_shp_toolbox}/QtGui
  ${O4W_QT_HEADERS_for_plugin_vector_data_check_shp_toolbox}/QtXml
  ${O4W_QT_HEADERS_for_plugin_vector_data_check_shp_toolbox}/QtWidgets

  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/gui
  ${CMAKE_SOURCE_DIR}/src/plugins

  ${CMAKE_BINARY_DIR}

  # 基础算法库头文件
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h

  # 插件头文件
  ${CMAKE_BINARY_DIR}/src/plugins/vector_data_check_shp_toolbox 
)


# 杨小兵-2024-03-27：包含库文件
target_link_libraries(plugin_vector_data_check_shp_toolbox

  ${OSGEO4W_ROOT_for_plugin_vector_data_check_shp_toolbox}/lib/gdal_i.lib
  ${OSGEO4W_ROOT_for_plugin_vector_data_check_shp_toolbox}/lib/libxml2.lib

  ${O4W_QT_LIBRARIES_for_plugin_vector_data_check_shp_toolbox}/Qt5Gui.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_check_shp_toolbox}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_check_shp_toolbox}/Qt5Xml.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_check_shp_toolbox}/Qt5Widgets.lib

  qgis_core
  qgis_gui

  ${CMAKE_BINARY_DIR}/src/starmap_src/base_vector_format_conversion/RelWithDebInfo/base_vector_format_conversion.lib

)

# 杨小兵-2024-03-27：添加预处理器定义
target_compile_definitions(plugin_vector_data_check_shp_toolbox PRIVATE "-DOS_FAMILY_WINDOWS" "-D_HAS_STD_BYTE=0")






########################################################
# Install

install(TARGETS plugin_vector_data_check_shp_toolbox
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})

