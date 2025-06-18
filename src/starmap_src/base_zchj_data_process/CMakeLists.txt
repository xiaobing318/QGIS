cmake_minimum_required(VERSION 3.16)
project(base_zchj_data_process LANGUAGES CXX)

#------------------------------------------------------------
# 0. 可选：未传入 LIBRARY_TYPE 时默认生成 SHARED
#------------------------------------------------------------
if(NOT DEFINED LIBRARY_TYPE)
    set(LIBRARY_TYPE SHARED)
endif()

include(GNUInstallDirs)

#------------------------------------------------------------
# 1. 源文件 / 头文件
#------------------------------------------------------------
set(base_zchj_data_process_SRCS
    cse_zchj_data_process.cpp
)

set(base_zchj_data_process_HDRS
    ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h/zchj/cse_zchj_data_process.h
)

add_library(base_zchj_data_process ${LIBRARY_TYPE}
    ${base_zchj_data_process_SRCS}
    ${base_zchj_data_process_HDRS}
)

target_compile_features(base_zchj_data_process PRIVATE cxx_std_17)

#------------------------------------------------------------
# 2. 公共 Include 目录（业务 + spdlog 副本）
#------------------------------------------------------------
target_include_directories(base_zchj_data_process
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
        ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/include
)

#------------------------------------------------------------
# 3. Windows (OSGeo4W) 额外 Include
#------------------------------------------------------------
if(WIN32 AND DEFINED ENV{OSGEO4W_ROOT})
    target_include_directories(base_zchj_data_process
        SYSTEM PUBLIC $ENV{OSGEO4W_ROOT}/include)
endif()

#------------------------------------------------------------
# 4. GDAL —— 同时兼容“现代 CMake” 与 QGIS 内置旧版 FindGDAL
#------------------------------------------------------------
find_package(GDAL REQUIRED)

if(TARGET GDAL::GDAL)
    # 新版 FindGDAL.cmake 已经生成 IMPORTED target
    set(_GDAL_TARGET GDAL::GDAL)
else()
    message(STATUS "GDAL::GDAL target not found – falling back to variables")
    add_library(_gdal_unknown INTERFACE)

    # ========== 头文件 ==========
    if(GDAL_INCLUDE_DIRS)
        target_include_directories(_gdal_unknown INTERFACE ${GDAL_INCLUDE_DIRS})
    elseif(GDAL_INCLUDE_DIR)
        target_include_directories(_gdal_unknown INTERFACE ${GDAL_INCLUDE_DIR})
    endif()

    # ========== 库文件 ==========
    if(GDAL_LIBRARIES)
        target_link_libraries(_gdal_unknown INTERFACE ${GDAL_LIBRARIES})
    elseif(GDAL_LIBRARY)
        target_link_libraries(_gdal_unknown INTERFACE ${GDAL_LIBRARY})
    endif()

    add_library(GDAL::GDAL ALIAS _gdal_unknown)
    set(_GDAL_TARGET GDAL::GDAL)
endif()

#---- 补充：确保能找到 gdal.h (/usr/include/gdal 等) ----
find_path(GDAL_HEADER_DIR
          NAMES gdal.h
          HINTS ${GDAL_INCLUDE_DIRS} ${GDAL_INCLUDE_DIR} /usr/include
          PATH_SUFFIXES gdal)
if(GDAL_HEADER_DIR)
    target_include_directories(base_zchj_data_process PUBLIC ${GDAL_HEADER_DIR})
endif()

target_link_libraries(base_zchj_data_process PUBLIC ${_GDAL_TARGET})

#------------------------------------------------------------
# 5. Windows 专用链接库
#------------------------------------------------------------
if(WIN32)
    target_link_libraries(base_zchj_data_process PRIVATE odbc32 odbccp32)
endif()

#------------------------------------------------------------
# 6. 导出符号 (配合 STAREARTH_CONFIG_H_)
#------------------------------------------------------------
target_compile_definitions(base_zchj_data_process PRIVATE DLLPROJECT_EXPORT)

#------------------------------------------------------------
# 7. 安装规则
#------------------------------------------------------------
if(WIN32)
    install(TARGETS base_zchj_data_process
            RUNTIME DESTINATION ${QGIS_BIN_DIR}
            BUNDLE  DESTINATION ${QGIS_BIN_DIR})
else()
    install(TARGETS base_zchj_data_process
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
