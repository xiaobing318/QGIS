
########################################################
# Files
set (vector_data_check_odata_toolbox_SRCS
  ui_class/se_layer_code_standardization_check.cpp
  ui_class/se_layer_integrity_check.cpp
  ui_class/se_layer_naming_standardization_check.cpp
  ui_class/se_layer_rareword_statistics.cpp


  ui_thread/se_layer_code_standardization_check_thread.cpp
  ui_thread/se_layer_integrity_check_thread.cpp
  ui_thread/se_layer_naming_standardization_check_thread.cpp
  ui_thread/se_layer_rareword_statistics_thread.cpp


  vector_data_check_odata_plugin.cpp
)

set (vector_data_check_odata_toolbox_HDRS
  ui_class/se_layer_code_standardization_check.h
  ui_class/se_layer_integrity_check.h
  ui_class/se_layer_naming_standardization_check.h
  ui_class/se_layer_rareword_statistics.h

  ui_thread/se_layer_code_standardization_check_thread.h
  ui_thread/se_layer_integrity_check_thread.h
  ui_thread/se_layer_naming_standardization_check_thread.h
  ui_thread/se_layer_rareword_statistics_thread.h

  vector_data_check_odata_plugin.h
)

set (vector_data_check_odata_toolbox_UIS
  ui/se_layer_code_standardization_check.ui
  ui/se_layer_integrity_check.ui
  ui/se_layer_naming_standardization_check.ui
  ui/se_layer_rareword_statistics.ui
)

set (vector_data_check_odata_toolbox_RCCS
  vector_data_check_odata_plugin.qrc
)




########################################################
# Build
if (BUILD_WITH_QT6)
  QT6_WRAP_UI (vector_data_check_odata_toolbox_UIS_H  ${vector_data_check_odata_toolbox_UIS})
else()
  QT5_WRAP_UI (vector_data_check_odata_toolbox_UIS_H  ${vector_data_check_odata_toolbox_UIS})
endif()

add_library (plugin_vector_data_check_odata_toolbox MODULE ${vector_data_check_odata_toolbox_SRCS}  ${vector_data_check_odata_toolbox_RCCS} ${vector_data_check_odata_toolbox_UIS_H})

# require c++17
target_compile_features(plugin_vector_data_check_odata_toolbox PRIVATE cxx_std_17)


# 杨小兵-2024-03-27：获取为CMake临时设置的环境变量
set(OSGEO4W_ROOT_for_plugin_vector_data_check_odata_toolbox $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_plugin_vector_data_check_odata_toolbox $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_plugin_vector_data_check_odata_toolbox $ENV{O4W_QT_LIBRARIES})

# 杨小兵-2024-03-27：包含头文件 
target_include_directories(plugin_vector_data_check_odata_toolbox SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(plugin_vector_data_check_odata_toolbox SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_plugin_vector_data_check_odata_toolbox}/include

  ${O4W_QT_HEADERS_for_plugin_vector_data_check_odata_toolbox}
  ${O4W_QT_HEADERS_for_plugin_vector_data_check_odata_toolbox}/QtCore
  ${O4W_QT_HEADERS_for_plugin_vector_data_check_odata_toolbox}/QtGui
  ${O4W_QT_HEADERS_for_plugin_vector_data_check_odata_toolbox}/QtXml
  ${O4W_QT_HEADERS_for_plugin_vector_data_check_shp_toolbox}/QtWidgets

  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/gui
  ${CMAKE_SOURCE_DIR}/src/plugins

  # 基础算法库头文件
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h

  # 插件头文件
  ${CMAKE_BINARY_DIR}/src/plugins/vector_data_check_odata_toolbox
)


# 杨小兵-2024-03-27：包含库文件
target_link_libraries(plugin_vector_data_check_odata_toolbox

  ${OSGEO4W_ROOT_for_plugin_vector_data_check_odata_toolbox}/lib/gdal_i.lib

  ${O4W_QT_LIBRARIES_for_plugin_vector_data_check_odata_toolbox}/Qt5Gui.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_check_odata_toolbox}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_check_odata_toolbox}/Qt5Xml.lib
  ${O4W_QT_LIBRARIES_for_plugin_vector_data_check_odata_toolbox}/Qt5Widgets.lib

  qgis_core
  qgis_gui

  ${CMAKE_BINARY_DIR}/src/starmap_src/base_vector_format_conversion/RelWithDebInfo/base_vector_format_conversion.lib

)

# 杨小兵-2024-03-27：添加预处理器定义
target_compile_definitions(plugin_vector_data_check_odata_toolbox PRIVATE "-DOS_FAMILY_WINDOWS" "-D_HAS_STD_BYTE=0")


########################################################
# Install

install(TARGETS plugin_vector_data_check_odata_toolbox
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})

