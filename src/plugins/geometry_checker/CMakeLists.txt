
########################################################
# Files

set (geometrychecker_SRCS
    qgsgeometrycheckerplugin.cpp
    qgsgeometrycheckfactory.cpp
    qgsgeometrycheckerdialog.cpp
    qgsgeometrycheckersetuptab.cpp
    qgsgeometrycheckerresulttab.cpp
    qgsgeometrycheckfixdialog.cpp
    qgsgeometrycheckerfixsummarydialog.cpp
)

set (geometrychecker_HDRS
    qgsgeometrycheckerplugin.h
    qgsgeometrycheckfactory.h
)

set (geometrychecker_UIS
    qgsgeometrycheckersetuptab.ui
    qgsgeometrycheckerresulttab.ui
    qgsgeometrycheckerfixsummarydialog.ui
)

set (geometrychecker_RCCS
    pluginres.qrc
)

########################################################
# Build

if (BUILD_WITH_QT6)
  QT6_WRAP_UI (geometrychecker_UIS_H  ${geometrychecker_UIS})
else()
  QT5_WRAP_UI (geometrychecker_UIS_H  ${geometrychecker_UIS})
endif()

add_library (plugin_geometrychecker MODULE ${geometrychecker_SRCS}  ${geometrychecker_RCCS} ${geometrychecker_UIS_H})

# require c++17
target_compile_features(plugin_geometrychecker PRIVATE cxx_std_17)

# 杨小兵-2024-02-24：头文件的搜索路径（自定义头文件路径）
#[[
  在CMake中，`include_directories`命令用于向编译器的头文件搜索路径中添加指定的目录。这使得在编译过程中，编译器可以在这些目录下查找到必要的头文件。
`${CMAKE_CURRENT_BINARY_DIR}`和`${CMAKE_CURRENT_SOURCE_DIR}`是CMake提供的变量，它们在这个上下文中被用来指定包含目录的路径。

### `${CMAKE_CURRENT_BINARY_DIR}`
- **作用**：这个变量指向当前处理的CMakeLists.txt文件对应的二进制目录，在编译项目时生成的所有中间文件（如编译产物和Makefile）通常放在这个目录下。
如果你使用的是out-of-source构建（推荐的方式，即构建目录与源代码目录分离），那么`${CMAKE_CURRENT_BINARY_DIR}`就指向这个外部构建目录中当前
CMakeLists.txt对应的位置。
  
- **用途**：使用`${CMAKE_CURRENT_BINARY_DIR}`作为包含目录的一个原因是，有些项目在配置（configure）阶段会生成配置文件或者其他代码文件
（如通过`configure_file`命令）。这些生成的文件通常放在构建目录中，所以需要将它们的路径添加到包含目录中，以便编译器能够找到它们。

### `${CMAKE_CURRENT_SOURCE_DIR}`

- **作用**：这个变量指向当前处理的CMakeLists.txt文件所在的源代码目录。这意味着无论你在哪里启动构建过程，`${CMAKE_CURRENT_SOURCE_DIR}`总是
指向包含当前CMakeLists.txt的那个目录。

- **用途**：将`${CMAKE_CURRENT_SOURCE_DIR}`添加到包含目录中的原因是，一些项目的源代码目录中可能包含了必要的头文件，或者在源代码树中散布的
其他位置。使用这个变量可以确保编译器能够在编译过程中访问到这些头文件。

### 综合说明
  在CMake项目中，通常需要将源代码目录和二进制目录（构建目录）都添加到编译器的包含路径中，以确保编译器可以找到所有需要的头文件。这对于那些在
配置阶段生成头文件的项目尤为重要，同时也方便管理和引用项目中分散的源代码文件。通过在`include_directories`命令中使用这些变量，你可以灵活地控
制编译器的搜索路径，无论你的项目结构如何变化，或者你在何处进行构建，都能够正确地引用到需要的头文件，从而提高项目的可移植性和灵活性。

]]
include_directories(
  ${CMAKE_SOURCE_DIR}/src/plugins

  ${CMAKE_BINARY_DIR}/src/analysis

  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)
# 杨小兵-2024-02-24：系统指定的头文件搜索路径
include_directories(SYSTEM
  ${GDAL_INCLUDE_DIR}
)

target_link_libraries(plugin_geometrychecker
  qgis_core
  qgis_gui
  qgis_analysis
)

########################################################
# Install

install(TARGETS plugin_geometrychecker
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})

