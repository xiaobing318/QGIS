############################################################
# sources
set(base_point_feature_hierarchical_processing_SRCS
  cse_dataprocessimp.cpp
  cse_poi_process.cpp
)

set(base_point_feature_hierarchical_processing_HDRS
  cse_dataprocessimp.h
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h/naviinfo/cse_poi_process.h
)


#############################################################
# vector data consistency processing base library


add_library(base_point_feature_hierarchical_processing ${LIBRARY_TYPE} ${base_point_feature_hierarchical_processing_SRCS} ${base_point_feature_hierarchical_processing_HDRS})


# require c++17
target_compile_features(base_point_feature_hierarchical_processing PRIVATE cxx_std_17)

set(OSGEO4W_ROOT_for_base_point_feature_hierarchical_processing $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_base_point_feature_hierarchical_processing $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_base_point_feature_hierarchical_processing $ENV{O4W_QT_LIBRARIES})


# 杨小兵-2024-06-25：包含头文件
target_include_directories(base_point_feature_hierarchical_processing SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_base_point_feature_hierarchical_processing}/include
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
)

target_include_directories(base_point_feature_hierarchical_processing PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

# 杨小兵-2024-06-25：包含头文件
target_link_libraries(base_point_feature_hierarchical_processing
  ${OSGEO4W_ROOT_for_base_point_feature_hierarchical_processing}/lib/gdal_i.lib
  ${CMAKE_BINARY_DIR}/src/starmap_src/base_project/RelWithDebInfo/base_project.lib
)


if (MSVC)
  #needed for linking to gdal which needs odbc
  set(TARGET_LINK_LIBRARIES base_point_feature_hierarchical_processing odbc32 odbccp32)
endif()

target_compile_definitions(base_point_feature_hierarchical_processing PRIVATE "-DDLLPROJECT_EXPORT")

# install
#[[
杨小兵-2024-06-25
  安装到bin目录下，BUNDLE是为了MACOS平台中的设置，而RUNTIME则是指定安装的位置
]]
# install
install (TARGETS base_point_feature_hierarchical_processing
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)
