############################################################
# sources

set(dataprocess_SRCS
  vector/vector_format_conversion/src/cse_annotation_pointer_reverse_extraction.cpp
  vector/vector_format_conversion/src/cse_geoarithmetic.cpp
  vector/vector_format_conversion/src/cse_mapsheet.cpp
  vector/vector_format_conversion/src/cse_quality_inspection_tools.cpp
  vector/vector_format_conversion/src/cse_vector_datacheck.cpp
  vector/vector_format_conversion/src/cse_vector_format_conversion.cpp
  vector/vector_format_conversion/src/cse_vector_format_conversion_Imp.cpp

)

set(dataprocess_HDRS
  vector/vector_format_conversion/src/cse_annotation_pointer_reverse_extraction.h
  vector/vector_format_conversion/src/cse_geoarithmetic.h
  vector/vector_format_conversion/src/cse_mapsheet.h
  vector/vector_format_conversion/src/cse_quality_inspection_tools.h
  vector/vector_format_conversion/src/cse_vector_format_conversion_Imp.h
)


#############################################################
# se_vector_format_conversion library
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(se_vector_format_conversion ${LIBRARY_TYPE} ${dataprocess_SRCS} ${dataprocess_HDRS})


# require c++17
target_compile_features(se_vector_format_conversion PRIVATE cxx_std_17)


target_include_directories(se_vector_format_conversion SYSTEM PUBLIC
  ${GDAL_INCLUDE_DIR}
  ${GDAL_INCLUDE_DIR}/libxml2
)


target_include_directories(se_vector_format_conversion PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ../../geo_algorithm_h/ai
  ../../geo_algorithm_h/big_data
  ../../geo_algorithm_h/commontype
  ../../geo_algorithm_h/geoarithmetic
  ../../geo_algorithm_h/naviinfo
  ../../geo_algorithm_h/project
  ../../geo_algorithm_h/ptp
  ../../geo_algorithm_h/raster
  ../../geo_algorithm_h/vector

  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/include

)
#[[
杨小兵-2024-02-25

GENERATE_EXPORT_HEADER(
  qgis_core
  BASE_NAME CORE
  EXPORT_FILE_NAME qgis_core.h
)
set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS} ${CMAKE_CURRENT_BINARY_DIR}/qgis_core.h)

  这段CMake代码使用了`GENERATE_EXPORT_HEADER`函数，它是CMake的一部分，用于自动生成导出宏的头文件。这个功能对于跨平台的动态链接库（DLL）
或共享对象（SO）的开发尤其重要，因为它们需要明确指定哪些符号（函数、类、变量等）是可供外部使用的（即导出的）。

### 函数解析
- `GENERATE_EXPORT_HEADER`：这是CMake提供的一个函数，用于自动生成一个包含导出宏定义的头文件。这个宏定义通常用于在编译库的时候导出符号，
在使用库时导入符号。
- `qgis_core`：这是函数的第一个参数，指定了目标库的名称。在这个上下文中，`qgis_core`是需要生成导出宏头文件的目标库。
- `BASE_NAME CORE`：这个参数指定了生成的宏名称的基础部分。在这里，`BASE_NAME`被设置为`CORE`，意味着生成的宏可能会以`CORE`作为前缀。
这有助于确保宏名称的唯一性，特别是在一个项目中存在多个库时。
- `EXPORT_FILE_NAME qgis_core.h`：这个参数指定了生成的头文件的名称。在这个例子中，头文件将被命名为`qgis_core.h`。这个头文件将包含用于导
出和导入符号的宏定义，使得`qgis_core`库可以被其他项目作为动态链接库使用。

]]

#[[
杨小兵-2024-02-25
  根据具体的情况动态添加库文件
]]
target_link_libraries(se_vector_format_conversion
  ${OPTIONAL_QTWEBKIT}
  ${QCA_LIBRARY}
  ${QTKEYCHAIN_LIBRARY}
  ${PROJ_LIBRARY}
  ${GEOS_LIBRARY}
  ${GDAL_LIBRARY}
  ${SPATIALINDEX_LIBRARY}
  ${EXPAT_LIBRARY}
  ${SQLITE3_LIBRARY}
  ${LIBZIP_LIBRARY}
  ${Protobuf_LITE_LIBRARY}
  ${ZLIB_LIBRARIES}
  ${EXIV2_LIBRARY}
  ${CMAKE_SOURCE_DIR}/src/starmap_src/winx64_vc16/lib/debug/se_project
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv
  ${CMAKE_SOURCE_DIR}/../lib/gdal_i
  ${CMAKE_SOURCE_DIR}../lib/libxml2
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/lib/spdlog
)


if (BUILD_WITH_QT6)
  target_link_libraries(se_vector_format_conversion Qt6::Core5Compat)
endif()


if (MSVC)
  #needed for linking to gdal which needs odbc
  set(TARGET_LINK_LIBRARIES se_vector_format_conversion odbc32 odbccp32)
endif()

#[[
杨小兵-2024-02-25
  这行CMake代码是用于向指定目标（在这例子中是`qgis_core`库）添加编译时定义的。`target_compile_definitions`命令允许你为目标添加预处理器定义，
这些定义将在编译时可用。这对于条件编译或启用特定的代码路径非常有用。

### 命令解析：target_compile_definitions(qgis_core PRIVATE "-DQT_NO_FOREACH")
  
- `target_compile_definitions`：这是CMake的一个命令，用于为给定的目标添加编译器定义。
- `qgis_core`：这是命令的第一个参数，指定了要添加编译器定义的目标。在这个例子中，目标是`qgis_core`，即QGIS的核心库。
- `PRIVATE`：这个关键字指定了定义的作用域。在这里，`PRIVATE`意味着定义只会影响`qgis_core`目标本身的编译，而不会影响链接到`qgis_core`的其他目标。
这是一种封装，确保其他目标的编译环境不会受到不必要的影响。
- `"-DQT_NO_FOREACH"`：这是要添加的具体编译器定义。`-D`是一个常见的编译器选项，用于定义预处理器宏。在这个例子中，`QT_NO_FOREACH`宏被定义了，
这是Qt框架的一个特定定义，用于禁用`foreach`宏。

### `QT_NO_FOREACH`的作用
`QT_NO_FOREACH`是一个用于Qt应用程序和库开发的预处理器宏，当定义了这个宏，Qt的`foreach`宏将不可用。Qt推荐使用C++11的范围基`for`循环
（`for(auto item : items)`）代替`foreach`，因为它提供了更好的类型安全性和兼容性。从Qt 5.7开始，`foreach`已被标记为不推荐使用，并在
Qt 6中完全移除。
  在QGIS项目中使用`-DQT_NO_FOREACH`的目的是确保代码基不依赖于即将被废弃的`foreach`宏，促进使用现代C++语言特性，同时也是为了确保代码能
够与未来的Qt版本兼容。
  总结，这行CMake代码为`qgis_core`库的编译过程添加了一个定义，禁用了Qt的`foreach`宏，这是向现代C++迁移和确保与未来Qt版本兼容性的一部分努力。
]]

target_compile_definitions(se_vector_format_conversion PRIVATE "-DDLLPROJECT_EXPORT")


# install
install(TARGETS se_vector_format_conversion
  RUNTIME DESTINATION vector/vector_format_conversion/lib
  LIBRARY DESTINATION vector/vector_format_conversion/lib
)

