
########################################################
# Files
set (geoextractandprocess_SRCS
    vector_spatial_accuracy_check.cpp
    set_attribute_extract_params.cpp
    multiscale_match.cpp
    multiscale_addfield.cpp
    moving_target_mapping.cpp
    image_spatial_accuracy_check.cpp
    image_mapping_layout_vectorization.cpp
    image_mapping_gdxdata.cpp
    image_mapping.cpp
    geoextractandprocess_progress_dialog.cpp
    geoextractandprocess_plugin_setmergeparams.cpp
    geoextractandprocess_plugin_gui.cpp
    geoextractandprocess_plugin.cpp
    CSE_MapSheet.cpp
    create_operationarea_range.cpp
    create_operationarea_between.cpp
    copy_shapefiles.cpp
    convert_operationarea_and_sheet.cpp
    convert_data_to_gjb.cpp
    convert_data_to_gb_or_gjb.cpp
    better_than_5w_merge_features.cpp
    better_than_5w_match.cpp
    automerge.cpp
)

set (geoextractandprocess_HDRS
    vector_spatial_accuracy_check.h
    set_attribute_extract_params.h
    se_commontypedef.h
    multiscale_match.h
    multiscale_addfield.h
    moving_target_mapping.h
    image_spatial_accuracy_check.h
    image_mapping_layout_vectorization.h
    image_mapping_gdxdata.h
    image_mapping.h
    geoextractandprocess_progress_dialog.h
    geoextractandprocess_plugin_setmergeparams.h
    geoextractandprocess_plugin_gui.h
    geoextractandprocess_plugin.h
    CSE_MapSheet.h
    CSE_GeoExtractAndProcess.h
    create_operationarea_range.h
    create_operationarea_between.h
    copy_shapefiles.h
    convert_operationarea_and_sheet.h
    convert_data_to_gjb.h
    convert_data_to_gb_or_gjb.h
    better_than_5w_merge_features.h
    better_than_5w_match.h
    automerge.h
)

set (geoextractandprocess_UIS
    vector_spatial_accuracy_check.ui
    set_attribute_extract_params.ui
    multiscale_match.ui
    multiscale_addfield.ui
    moving_target_mapping.ui
    image_spatial_accuracy_check.ui
    image_mapping_layout_vectorization.ui
    image_mapping_gdxdata.ui
    image_mapping.ui
    geoextractandprocess_progress_dialog_base.ui
    geoextractandprocess_plugin_set_merge_params.ui
    geoextractandprocess_plugin_guibase.ui
    create_operationarea_range.ui
    create_operationarea_between.ui
    copy_shapefiles.ui
    convert_operationarea_and_sheet.ui
    convert_data_to_gjb.ui
    convert_data_to_gb_or_gjb.ui
    better_than_5w_merge_features.ui
    better_than_5w_match.ui
    automerge.ui
)

set (geoextractandprocess_RCCS
    geoextractandprocess_plugin.qrc
)










########################################################
# Build
if (BUILD_WITH_QT6)
  QT6_WRAP_UI (geoextractandprocess_UIS_H  ${geoextractandprocess_UIS})
else()
  QT5_WRAP_UI (geoextractandprocess_UIS_H  ${geoextractandprocess_UIS})
endif()

add_library (plugin_geoextractandprocess   MODULE ${geoextractandprocess_SRCS}  ${geoextractandprocess_RCCS} ${geoextractandprocess_UIS_H})

# require c++17
target_compile_features(plugin_geoextractandprocess PRIVATE cxx_std_17)

# 杨小兵-2024-02-29：获取为CMake临时设置的环境变量
set(OSGEO4W_ROOT_for_plugin_geoextractandprocess $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_plugin_geoextractandprocess $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_plugin_geoextractandprocess $ENV{O4W_QT_LIBRARIES})

target_include_directories(plugin_geoextractandprocess SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(plugin_geoextractandprocess SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_plugin_geoextractandprocess}/include

  ${O4W_QT_HEADERS_for_plugin_geoextractandprocess}
  ${O4W_QT_HEADERS_for_plugin_geoextractandprocess}/QtCore
  ${O4W_QT_HEADERS_for_plugin_geoextractandprocess}/QtGui
  ${O4W_QT_HEADERS_for_plugin_geoextractandprocess}/QtXml
  ${O4W_QT_HEADERS_for_plugin_geoextractandprocess}/QtWidgets
  ${O4W_QT_HEADERS_for_plugin_geoextractandprocess}/QtSvg
  ${O4W_QT_HEADERS_for_plugin_geoextractandprocess}/QtPrintSupport
  ${O4W_QT_HEADERS_for_plugin_geoextractandprocess}/QtNetwork

  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/core/editform
  ${CMAKE_SOURCE_DIR}/src/core/expression
  ${CMAKE_SOURCE_DIR}/src/core/fieldformatter
  ${CMAKE_SOURCE_DIR}/src/core/geometry
  ${CMAKE_SOURCE_DIR}/src/core/labeling
  ${CMAKE_SOURCE_DIR}/src/core/layertree
  ${CMAKE_SOURCE_DIR}/src/core/layout
  ${CMAKE_SOURCE_DIR}/src/core/maprenderer
  ${CMAKE_SOURCE_DIR}/src/core/metadata
  ${CMAKE_SOURCE_DIR}/src/core/proj
  ${CMAKE_SOURCE_DIR}/src/core/project
  ${CMAKE_SOURCE_DIR}/src/core/providers
  ${CMAKE_SOURCE_DIR}/src/core/raster
  ${CMAKE_SOURCE_DIR}/src/core/settings
  ${CMAKE_SOURCE_DIR}/src/core/symbology
  ${CMAKE_SOURCE_DIR}/src/core/textrenderer
  ${CMAKE_SOURCE_DIR}/src/core/vector
  ${CMAKE_SOURCE_DIR}/src/gui
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_SOURCE_DIR}/external/nlohmann

  ${CMAKE_BINARY_DIR}/src/plugins/geoextractandprocess
  ${CMAKE_BINARY_DIR}/src/core
  ${CMAKE_BINARY_DIR}/src/gui
  ${CMAKE_BINARY_DIR}

  
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libxlsxwriter/include
)


target_compile_definitions(plugin_geoextractandprocess PRIVATE "-D_HAS_STD_BYTE=0" "-DOS_FAMILY_WINDOWS")

#[[
杨小兵-2024-02-29
  其中RelWithDebInfo是硬编码
]]
target_link_libraries(plugin_geoextractandprocess
  ${OSGEO4W_ROOT_for_plugin_geoextractandprocess}/lib/gdal_i.lib

  ${O4W_QT_LIBRARIES_for_plugin_geoextractandprocess}/Qt5Gui.lib
  ${O4W_QT_LIBRARIES_for_plugin_geoextractandprocess}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_plugin_geoextractandprocess}/Qt5Xml.lib
  ${O4W_QT_LIBRARIES_for_plugin_geoextractandprocess}/Qt5Widgets.lib

  qgis_core
  qgis_gui

  ${CMAKE_SOURCE_DIR}/external/starmap_external/zlib/lib/zlib.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libxlsxwriter/lib/xlsxwriter.lib

  ${CMAKE_BINARY_DIR}/src/starmap_src/base_project/RelWithDebInfo/base_project.lib

#[[
杨小兵-2024-02-29
  ${CMAKE_BINARY_DIR}/src/starmap_src/base_geoextractandprocess/${CMAKE_BUILD_TYPE}/base_geoextractandprocess.lib
  这种方式目前不可以，因为在顶层cmakelists中没有将CMAKE_BUILD_TYPE cache到文件中，所以在子目录中找不到CMAKE_BUILD_TYPE这个值，
目前使用硬编码的方式，等对cmake熟练之后再找解决办法
]]
  ${CMAKE_BINARY_DIR}/src/starmap_src/base_geoextractandprocess/RelWithDebInfo/base_geoextractandprocess.lib
  
  
)

########################################################
# Install

install(TARGETS plugin_geoextractandprocess
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})

