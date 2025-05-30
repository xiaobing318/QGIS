############################################################
# sources
set(base_coordinate_transformation_SRCS
  cse_coordinate_transformation.cpp
  cse_imp.cpp
)

set(base_coordinate_transformation_HDRS
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h/vector/cse_coordinate_transformation.h
  cse_imp.h
)


#############################################################
# coordinate transformation base library


add_library(base_coordinate_transformation ${LIBRARY_TYPE} ${base_coordinate_transformation_SRCS} ${base_coordinate_transformation_HDRS})


# require c++17
target_compile_features(base_coordinate_transformation PRIVATE cxx_std_17)

set(OSGEO4W_ROOT_for_base_coordinate_transformation $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_base_coordinate_transformation $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_base_coordinate_transformation $ENV{O4W_QT_LIBRARIES})


# 杨小兵-2024-03-28：包含头文件
target_include_directories(base_coordinate_transformation SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_base_coordinate_transformation}/include
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
)

target_include_directories(base_coordinate_transformation PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

# 杨小兵-2024-03-28：包含头文件
target_link_libraries(base_coordinate_transformation
  ${OSGEO4W_ROOT_for_base_coordinate_transformation}/lib/gdal_i.lib
)


if (MSVC)
  #needed for linking to gdal which needs odbc
  set(TARGET_LINK_LIBRARIES base_coordinate_transformation odbc32 odbccp32)
endif()

target_compile_definitions(base_coordinate_transformation PRIVATE "-DOS_FAMILY_WINDOWS" "-DDLLPROJECT_EXPORT")

# install
#[[
杨小兵-2024-02-25
  安装到bin目录下，BUNDLE是为了MACOS平台中的设置，而RUNTIME则是指定安装的位置
]]
# install
install (TARGETS base_coordinate_transformation
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)
