
########################################################
# Files
set (moving_target_mapping_layout_vectorization_SRCS
  # 添加【动目标制图】功能
  moving_target_mapping_layout_vectorization.cpp
  CSE_MapSheet.cpp
  main.cpp
)

set (moving_target_mapping_layout_vectorization_HDRS
  # 添加【动目标制图】功能
  moving_target_mapping_layout_vectorization.h
  CSE_MapSheet.h
)

set (moving_target_mapping_layout_vectorization_UIS
  # 添加【动目标制图】功能
  moving_target_mapping_layout_vectorization.ui
)

set (moving_target_mapping_layout_vectorization_RCCS
  # cmake构建资源的时候出现问题
  #moving_target_mapping_layout_vectorization_plugin.qrc
)










########################################################
# Build
if (BUILD_WITH_QT6)
  QT6_WRAP_UI (moving_target_mapping_layout_vectorization_UIS_H  ${moving_target_mapping_layout_vectorization_UIS})
else()
  QT5_WRAP_UI (moving_target_mapping_layout_vectorization_UIS_H  ${moving_target_mapping_layout_vectorization_UIS})
endif()

add_executable (bin_moving_target_mapping_layout_vectorization ${moving_target_mapping_layout_vectorization_SRCS}  ${moving_target_mapping_layout_vectorization_RCCS} ${moving_target_mapping_layout_vectorization_UIS_H})


# require c++17
target_compile_features(bin_moving_target_mapping_layout_vectorization PRIVATE cxx_std_17)

# 杨小兵-2024-02-29：获取为CMake临时设置的环境变量
set(OSGEO4W_ROOT_for_moving_target_mapping_layout_vectorization $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_moving_target_mapping_layout_vectorization $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_moving_target_mapping_layout_vectorization $ENV{O4W_QT_LIBRARIES})

target_include_directories(bin_moving_target_mapping_layout_vectorization SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(bin_moving_target_mapping_layout_vectorization SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_moving_target_mapping_layout_vectorization}/include

  ${O4W_QT_HEADERS_for_moving_target_mapping_layout_vectorization}
  ${O4W_QT_HEADERS_for_moving_target_mapping_layout_vectorization}/QtCore
  ${O4W_QT_HEADERS_for_moving_target_mapping_layout_vectorization}/QtGui
  ${O4W_QT_HEADERS_for_moving_target_mapping_layout_vectorization}/QtXml
  ${O4W_QT_HEADERS_for_moving_target_mapping_layout_vectorization}/QtWidgets
  ${O4W_QT_HEADERS_for_moving_target_mapping_layout_vectorization}/QtSvg
  ${O4W_QT_HEADERS_for_moving_target_mapping_layout_vectorization}/QtPrintSupport
  ${O4W_QT_HEADERS_for_moving_target_mapping_layout_vectorization}/QtNetwork

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

  ${CMAKE_BINARY_DIR}/src/plugins/bin_moving_target_mapping_layout_vectorization
  ${CMAKE_BINARY_DIR}/src/core
  ${CMAKE_BINARY_DIR}/src/gui
  ${CMAKE_BINARY_DIR}

  
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libxlsxwriter/include
)


target_compile_definitions(bin_moving_target_mapping_layout_vectorization PRIVATE "-D_HAS_STD_BYTE=0" "-DOS_FAMILY_WINDOWS")

#[[
杨小兵-2024-10-16
  其中RelWithDebInfo是硬编码
]]
target_link_libraries(bin_moving_target_mapping_layout_vectorization
  ${OSGEO4W_ROOT_for_moving_target_mapping_layout_vectorization}/lib/gdal_i.lib

  ${O4W_QT_LIBRARIES_for_moving_target_mapping_layout_vectorization}/Qt5Gui.lib
  ${O4W_QT_LIBRARIES_for_moving_target_mapping_layout_vectorization}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_moving_target_mapping_layout_vectorization}/Qt5Xml.lib
  ${O4W_QT_LIBRARIES_for_moving_target_mapping_layout_vectorization}/Qt5Widgets.lib

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

install(TARGETS bin_moving_target_mapping_layout_vectorization
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
  LIBRARY DESTINATION ${QGIS_LIB_DIR}
  ARCHIVE DESTINATION ${QGIS_LIB_DIR}
  FRAMEWORK DESTINATION ${QGIS_FW_SUBDIR}
  PUBLIC_HEADER DESTINATION ${QGIS_INCLUDE_DIR})
