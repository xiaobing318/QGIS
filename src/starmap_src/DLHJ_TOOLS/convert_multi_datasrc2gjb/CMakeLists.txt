############################################################
# sources
set(DLHJ_TOOLS_convert_multi_datasrc2gjb_SRCS
  character_set_encoding_conversion.cpp
  convert_multi_datasrc2gjb.cpp
  files_or_directories_check.cpp
  log_message.cpp
  main.cpp
)

set(DLHJ_TOOLS_convert_multi_datasrc2gjb_HDRS
  character_set_encoding_conversion.h
  convert_multi_datasrc2gjb.h
  CSE_GeoExtractAndProcess.h
  files_or_directories_check.h
  convert_multi_datasrc2gjb.h
  log_message.h
  se_commontypedef.h
)


#############################################################
# DLHJ_TOOLS_convert_multi_datasrc2gjb base library
# 添加base_geoextractandprocess库的头文件
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(DLHJ_TOOLS_convert_multi_datasrc2gjb ${DLHJ_TOOLS_convert_multi_datasrc2gjb_SRCS} ${DLHJ_TOOLS_convert_multi_datasrc2gjb_HDRS})

# require c++17
target_compile_features(DLHJ_TOOLS_convert_multi_datasrc2gjb PRIVATE cxx_std_17)

set(OSGEO4W_ROOT_for_DLHJ_TOOLS_convert_multi_datasrc2gjb $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_DLHJ_TOOLS_convert_multi_datasrc2gjb $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_DLHJ_TOOLS_convert_multi_datasrc2gjb $ENV{O4W_QT_LIBRARIES})


target_include_directories(DLHJ_TOOLS_convert_multi_datasrc2gjb SYSTEM PUBLIC
  ${OSGEO4W_ROOT_for_DLHJ_TOOLS_convert_multi_datasrc2gjb}/include
  
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_convert_multi_datasrc2gjb}
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_convert_multi_datasrc2gjb}/QtCore
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_convert_multi_datasrc2gjb}/QtXml
  ${O4W_QT_HEADERS_for_DLHJ_TOOLS_convert_multi_datasrc2gjb}/QtGui

  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/external/nlohmann
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include

  ${CMAKE_BINARY_DIR}/src/core
)

target_include_directories(DLHJ_TOOLS_convert_multi_datasrc2gjb PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)


target_link_libraries(DLHJ_TOOLS_convert_multi_datasrc2gjb
  qgis_core
  qgis_gui
  ${CMAKE_BINARY_DIR}/src/starmap_src/base_geoextractandprocess/RelWithDebInfo/base_geoextractandprocess.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv.lib
)

#target_compile_definitions(DLHJ_TOOLS_convert_multi_datasrc2gjb PRIVATE "-DGEOEXTRACTANDPROCESS_EXPORT")


# install
#[[
杨小兵-2024-02-25
  安装到bin目录下
]]
install (TARGETS DLHJ_TOOLS_convert_multi_datasrc2gjb
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)
