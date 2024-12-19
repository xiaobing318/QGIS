############################################################
# sources
set(DLHJ_TOOLS_annotation_pointer_SRCS
  annotation_pointer.cpp
  character_set_encoding_conversion.cpp
  files_or_directories_check.cpp
  log_message.cpp
  main.cpp
)

set(DLHJ_TOOLS_annotation_pointer_HDRS
  annotation_pointer.h
  character_set_encoding_conversion.h
  cse_geotransformation.h
  files_or_directories_check.h
  log_message.h
  se_commondef.h
  se_commontypedef.h
  se_config.h
  se_projectcommondef.h
)


#############################################################
# DLHJ_TOOLS_annotation_pointer base library
# 添加base_geoextractandprocess库的头文件
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(DLHJ_TOOLS_annotation_pointer ${DLHJ_TOOLS_annotation_pointer_SRCS} ${DLHJ_TOOLS_annotation_pointer_HDRS})

# require c++17
target_compile_features(DLHJ_TOOLS_annotation_pointer PRIVATE cxx_std_17)

set(OSGEO4W_ROOT_for_DLHJ_TOOLS_annotation_pointer $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_DLHJ_TOOLS_annotation_pointer $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_DLHJ_TOOLS_annotation_pointer $ENV{O4W_QT_LIBRARIES})


target_include_directories(DLHJ_TOOLS_annotation_pointer SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_DLHJ_TOOLS_annotation_pointer}/include
  
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_annotation_pointer}
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_annotation_pointer}/QtCore
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_annotation_pointer}/QtXml
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_annotation_pointer}/QtGui

  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/external/nlohmann
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/include

  ${CMAKE_BINARY_DIR}/src/core

  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
  ${CMAKE_SOURCE_DIR}/src/starmap_src/base_vector_format_conversion
  
)

target_include_directories(DLHJ_TOOLS_annotation_pointer PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)


target_link_libraries(DLHJ_TOOLS_annotation_pointer
  qgis_core
  qgis_gui
  ${CMAKE_BINARY_DIR}/src/starmap_src/base_vector_format_conversion/RelWithDebInfo/base_vector_format_conversion.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/lib/spdlog.lib
)

#target_compile_definitions(DLHJ_TOOLS_annotation_pointer PRIVATE "-DGEOEXTRACTANDPROCESS_EXPORT")


# install
#[[
杨小兵-2024-02-25
  安装到bin目录下
]]
install (TARGETS DLHJ_TOOLS_annotation_pointer
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)