############################################################
# sources
set(base_vector_feature_matching_SRCS
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_lib/vector/cse_vector_feature_matching.cpp
)

set(base_vector_feature_matching_HDRS
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h/vector/cse_vector_feature_matching.h
)


#############################################################
# vector data consistency processing base library


add_library(base_vector_feature_matching ${LIBRARY_TYPE} ${base_vector_feature_matching_SRCS} ${base_vector_feature_matching_HDRS})


# require c++17
target_compile_features(base_vector_feature_matching PRIVATE cxx_std_17)

set(OSGEO4W_ROOT_for_base_vector_feature_matching $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_base_vector_feature_matching $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_base_vector_feature_matching $ENV{O4W_QT_LIBRARIES})


# 杨小兵-2024-05-09：包含头文件
target_include_directories(base_vector_feature_matching SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_base_vector_feature_matching}/include
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
)

target_include_directories(base_vector_feature_matching PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

# 杨小兵-2024-05-09：包含头文件
target_link_libraries(base_vector_feature_matching
  ${OSGEO4W_ROOT_for_base_vector_feature_matching}/lib/gdal_i.lib
)


if (MSVC)
  #needed for linking to gdal which needs odbc
  set(TARGET_LINK_LIBRARIES base_vector_feature_matching odbc32 odbccp32)
endif()

target_compile_definitions(base_vector_feature_matching PRIVATE "-DOS_FAMILY_WINDOWS" "-DDLLPROJECT_EXPORT")

# install
#[[
杨小兵-2024-02-25
  安装到bin目录下，BUNDLE是为了MACOS平台中的设置，而RUNTIME则是指定安装的位置
]]
# install
install (TARGETS base_vector_feature_matching
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)
