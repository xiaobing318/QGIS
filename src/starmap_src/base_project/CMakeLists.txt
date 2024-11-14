############################################################
# sources
set(base_project_SRCS
  cse_geotransformation.cpp
  cse_projectimp.cpp
)

set(base_project_HDRS
  cse_geotransformation.h
  cse_projectimp.h
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h/project/se_projectcommondef.h
)


#############################################################
# project base library
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(base_project ${LIBRARY_TYPE} ${base_project_SRCS} ${base_project_HDRS})

# require c++17
target_compile_features(base_project PRIVATE cxx_std_17)


set(OSGEO4W_ROOT_for_base_project $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_base_project $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_base_project $ENV{O4W_QT_LIBRARIES})

target_include_directories(base_project SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}

  #基础算法库头文件
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
)


target_include_directories(base_project PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(base_project

)


target_compile_definitions(base_project PRIVATE "-DOS_FAMILY_WINDOWS" "-DDLLPROJECT_EXPORT")


# install
#[[
杨小兵-2024-02-29
  安装到bin目录下
]]
install (TARGETS base_project
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)