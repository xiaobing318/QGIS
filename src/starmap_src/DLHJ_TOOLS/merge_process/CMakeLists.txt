############################################################
# sources
set(DLHJ_TOOLS_merge_process_SRCS
  merge_process.cpp
  character_set_encoding_conversion.cpp
  files_or_directories_check.cpp
  log_message.cpp
  main.cpp
)

set(DLHJ_TOOLS_merge_process_HDRS
  character_set_encoding_conversion.h
  CSE_GeoExtractAndProcess.h
  files_or_directories_check.h
  log_message.h
  se_commontypedef.h
  merge_process.h
)


#############################################################
# DLHJ_TOOLS_merge_process base library
# 添加base_geoextractandprocess库的头文件
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(DLHJ_TOOLS_merge_process ${DLHJ_TOOLS_merge_process_SRCS} ${DLHJ_TOOLS_merge_process_HDRS})

# require c++17
target_compile_features(DLHJ_TOOLS_merge_process PRIVATE cxx_std_17)

set(OSGEO4W_ROOT_for_DLHJ_TOOLS_merge_process $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_DLHJ_TOOLS_merge_process $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_DLHJ_TOOLS_merge_process $ENV{O4W_QT_LIBRARIES})


target_include_directories(DLHJ_TOOLS_merge_process SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_DLHJ_TOOLS_merge_process}/include
  
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_merge_process}
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_merge_process}/QtCore
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_merge_process}/QtXml
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_merge_process}/QtGui

  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/external/nlohmann
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include

  ${CMAKE_BINARY_DIR}/src/core
)

target_include_directories(DLHJ_TOOLS_merge_process PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)


target_link_libraries(DLHJ_TOOLS_merge_process
  qgis_core
  qgis_gui
  ${CMAKE_BINARY_DIR}/src/starmap_src/base_geoextractandprocess/RelWithDebInfo/base_geoextractandprocess.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv.lib
)

#target_compile_definitions(DLHJ_TOOLS_merge_process PRIVATE "-DGEOEXTRACTANDPROCESS_EXPORT")


# install
#[[
杨小兵-2024-02-25
  安装到bin目录下
]]
install (TARGETS DLHJ_TOOLS_merge_process
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)