############################################################
# sources
set(DLHJ_TOOLS_write_shp_data_into_gpkg_SRCS
  character_set_encoding_conversion.cpp
  create_gpkg_database_and_store_shp_data2gpkg.cpp
  files_or_directories_check.cpp
  log_message.cpp
  main.cpp
)

set(DLHJ_TOOLS_write_shp_data_into_gpkg_HDRS
  character_set_encoding_conversion.h
  create_gpkg_database_and_store_shp_data2gpkg.h
  CSE_GeoExtractAndProcess.h
  files_or_directories_check.h
  log_message.h
  se_commontypedef.h
)


#############################################################
# DLHJ_TOOLS_write_shp_data_into_gpkg base library
# 添加base_geoextractandprocess库的头文件
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(DLHJ_TOOLS_write_shp_data_into_gpkg ${DLHJ_TOOLS_write_shp_data_into_gpkg_SRCS} ${DLHJ_TOOLS_write_shp_data_into_gpkg_HDRS})

# require c++17
target_compile_features(DLHJ_TOOLS_write_shp_data_into_gpkg PRIVATE cxx_std_17)

set(OSGEO4W_ROOT_for_DLHJ_TOOLS_write_shp_data_into_gpkg $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_DLHJ_TOOLS_write_shp_data_into_gpkg $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_DLHJ_TOOLS_write_shp_data_into_gpkg $ENV{O4W_QT_LIBRARIES})

#[[
target_include_directories(DLHJ_TOOLS_write_shp_data_into_gpkg SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/include
  
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_write_shp_data_into_gpkg}
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/QtCore
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/QtXml
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/QtGui

  ${CMAKE_SOURCE_DIR}/external/nlohmann
)
]]

target_include_directories(DLHJ_TOOLS_write_shp_data_into_gpkg SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/include
  
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_write_shp_data_into_gpkg}
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/QtCore
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/QtXml
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/QtGui

  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/external/nlohmann
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include

  ${CMAKE_BINARY_DIR}/src/core
)

target_include_directories(DLHJ_TOOLS_write_shp_data_into_gpkg PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)


#[[
杨小兵-2024-02-25
  根据具体的情况动态添加库文件
target_link_libraries(DLHJ_TOOLS_write_shp_data_into_gpkg
  ${OSGEO4W_ROOT_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/lib/gdal_i.lib
  ${O4W_QT_LIBRARIES_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/Qt5Core.lib
  ${O4W_QT_LIBRARIES_for_DLHJ_TOOLS_write_shp_data_into_gpkg}/Qt5Xml.lib
  qgis_core
)
]]
target_link_libraries(DLHJ_TOOLS_write_shp_data_into_gpkg
  qgis_core
  qgis_gui
  ${CMAKE_BINARY_DIR}/src/starmap_src/base_geoextractandprocess/RelWithDebInfo/base_geoextractandprocess.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv.lib
)

target_compile_definitions(DLHJ_TOOLS_write_shp_data_into_gpkg PRIVATE "-D_HAS_STD_BYTE=0")


# install
#[[
杨小兵-2024-02-25
  安装到bin目录下
]]
install (TARGETS DLHJ_TOOLS_write_shp_data_into_gpkg
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)
