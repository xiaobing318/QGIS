############################################################
# sources
set(base_vector_format_conversion_SRCS
  cse_annotation_pointer_reverse_extraction.cpp
  cse_geoarithmetic.cpp
  cse_mapsheet.cpp
  cse_quality_inspection_tools.cpp
  cse_vector_datacheck.cpp
  cse_vector_format_conversion.cpp
  cse_vector_format_conversion_Imp.cpp
)

set(base_vector_format_conversion_HDRS
  cse_annotation_pointer_reverse_extraction.h
  cse_geoarithmetic.h
  cse_mapsheet.h
  cse_quality_inspection_tools.h
  cse_vector_datacheck.h
  cse_vector_format_conversion.h
  cse_vector_format_conversion_Imp.h
)





#############################################################
# vector_format_conversion base library
#[[
-2024-02-28
  LIBRARY_TYPE这个变量是在顶层cmakelists.txt中被定义，在Windows平台上将会被设置为SHARED,其他平台则是STATIC，不过可以进行修改
]]
add_library(base_vector_format_conversion ${LIBRARY_TYPE} ${base_vector_format_conversion_SRCS} ${base_vector_format_conversion_HDRS})


# require c++17
target_compile_features(base_vector_format_conversion PRIVATE cxx_std_17)

#[[
-2024-02-28
  这段CMake代码使用`target_include_directories`命令为名为`base_vector_format_conversion`的目标（可能是一个可执行文件或库）添加包含目录。
在编译这个目标时，指定的目录将被编译器用来查找头文件。`target_include_directories`命令用于为指定的目标添加包含目录（即头文件搜索路径）。
这些路径在编译时被添加到编译器的包含路径中，让编译器能够找到源代码中引用的头文件。

### 参数解释
- **base_vector_format_conversion**: 这是目标的名称，CMake会为这个目标添加后面指定的包含路径。
- **SYSTEM PUBLIC**: 这里使用了两个关键字，`SYSTEM`和`PUBLIC`。
  - `SYSTEM`指示给定的包含路径应该被视为系统包含路径，这会影响编译器如何处理这些路径中的警告。对于GCC和Clang，这意味着在这些目录中找到的头
  文件中的警告不会被显示，这对于抑制第三方库头文件中的警告很有用。
  - `PUBLIC`指示这些包含路径不仅对当前目标有效，而且对链接了这个目标的其他目标也有效。这意味着，如果其他目标通过`target_link_libraries`命
  令与`base_vector_format_conversion`链接，那么这些包含路径也会被添加到那些目标的编译器包含路径中。

]]

set(OSGEO4W_ROOT_for_base_vector_format_conversion $ENV{OSGEO4W_ROOT})
set(O4W_QT_HEADERS_for_base_vector_format_conversion $ENV{O4W_QT_HEADERS})
set(O4W_QT_LIBRARIES_for_base_vector_format_conversion $ENV{O4W_QT_LIBRARIES})

# -2024-03-28：添加头文件
target_include_directories(base_vector_format_conversion PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(base_vector_format_conversion SYSTEM PUBLIC
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
  ${OSGEO4W_ROOT_for_base_vector_format_conversion}/include
  ${OSGEO4W_ROOT_for_base_vector_format_conversion}/include/libxml2
  
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/include
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/include
  ${CMAKE_SOURCE_DIR}/external

  
)

#[[
-2024-02-25
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

# -2024-03-28：添加头文件
target_link_libraries(base_vector_format_conversion
  ${OSGEO4W_ROOT_for_base_vector_format_conversion}/lib/gdal_i.lib
  ${OSGEO4W_ROOT_for_base_vector_format_conversion}/lib/libxml2.lib

  ${CMAKE_BINARY_DIR}/src/starmap_src/base_project/RelWithDebInfo/base_project.lib
  
  ${CMAKE_SOURCE_DIR}/external/starmap_external/spdlog_01_13_00/lib/spdlog.lib
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libiconv_build_1_17/lib/libiconv.lib
)


if (MSVC)
  #needed for linking to gdal which needs odbc
  set(TARGET_LINK_LIBRARIES base_vector_format_conversion odbc32 odbccp32)
endif()

#[[
-2024-02-25
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

target_compile_definitions(base_vector_format_conversion PRIVATE "-DOS_FAMILY_WINDOWS" "-DDLLPROJECT_EXPORT" "-D_HAS_STD_BYTE=0")


# install
#[[
-2024-02-25
  安装到bin目录下
)
]]
install (TARGETS base_vector_format_conversion
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)
