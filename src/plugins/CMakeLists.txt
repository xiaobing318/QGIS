#[[
杨小兵-2024-02-24
  这段代码是使用CMake编写的构建脚本，主要用于配置和准备编译一个C++项目（配置和编译一个C++项目）。CMake是一个跨平台的自动化构建系统，
它使用CMakeLists.txt文件作为项目的配置文件，用于指定编译参数、目标文件夹以及依赖关系等信息。下面将从应用场景、数学角度和计算机实现角
度来解释这段代码的内容。

### 应用场景
  这段代码主要应用于配置编译环境，尤其是针对C++项目中不同编译器的特定选项、输出目录的设置，以及根据不同条件编译不同的子目录（不同编译
器的特定选项、输出目录的设置、编译不同的子目录）。这对于大型项目如QGIS（一个开源的地理信息系统软件）非常重要，因为它需要灵活地管理不同
的编译器选项和模块的编译。

### 数学角度
  从数学的角度看，这段代码主要涉及的是逻辑运算。例如，使用`OR`逻辑运算符来检查编译器ID是否匹配`"GNU"`或`"Clang"`。这里的逻辑是，如果
当前的编译器是GNU或Clang，那么执行一系列的设置操作。这种逻辑判断是编程中常用的条件控制语句，用于根据不同的条件执行不同的代码路径。

### 计算机实现角度
从计算机实现的角度来看，这段代码使用了CMake的语法和命令来完成以下任务：
1. **编译器选项设置**:
   - `if`语句用于检测当前使用的C++编译器是否为GNU或Clang。如果是，通过`set`命令修改`CMAKE_CXX_FLAGS`变量，加入`-fvisibility=hidden`
和`-fvisibility-inlines-hidden`选项，这些选项用于控制符号的可见性，以减少动态库的大小并提高加载速度。

2. **输出目录设置**:
   - 使用`set`命令设置`CMAKE_RUNTIME_OUTPUT_DIRECTORY`和`CMAKE_LIBRARY_OUTPUT_DIRECTORY`变量，指定编译产物的输出目录。这样做可以方
便地组织和管理编译后的可执行文件和库文件，尤其是在不安装QGIS的情况下运行它时。

3. **子目录添加**:
   - 通过`add_subdirectory`命令添加多个子目录到项目中。这意味着CMake会进入这些子目录，执行里面的CMakeLists.txt文件，来配置和编译这些子
目录中的项目。这对于模块化开发非常有用，允许开发者将项目分解成多个较小的部分独立管理和编译。

4. **条件编译**:
   - 根据`WITH_SPATIALITE`、`GRASS_FOUND`和`CUSTOM_PLUGINS`这些条件变量的值，选择性地添加一些特定的子目录到编译过程中。这使得编译过程
可以根据环境或用户的选择来包含或排除某些功能模块。

5. **头文件安装**:
   - 最后一行注释提到了“头文件安装在qgis_core目标中”，这意味着相关的头文件会被安装到指定的位置，使得它们可以被项目中其他部分或其他项目使用。
整体而言，这段代码展示了CMake在项目配置和模块化编译管理中的强大功能，特别是在处理大型、复杂项目时的灵活性和可扩展性。（这一部分内容还需要进
一步的了解）
]]

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS_OLD "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
endif()
# override default path where built files are put to allow running qgis without installing
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/${QGIS_PLUGIN_SUBDIR})
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/${QGIS_PLUGIN_SUBDIR})

add_subdirectory(topology)
add_subdirectory(geometry_checker)

if (WITH_SPATIALITE)
  add_subdirectory(offline_editing)
endif()

if (GRASS_FOUND)
  add_subdirectory(grass)
endif()

if (CUSTOM_PLUGINS)
  add_subdirectory(${CUSTOM_PLUGINS})
endif()


add_subdirectory(analysis_ready_data_toolbox)
#add_subdirectory(bin_moving_target_mapping_layout_vectorization)
add_subdirectory(geoextractandprocess)
#add_subdirectory(odata_format_conversion_toolbox)
#add_subdirectory(planet_data_products_toolbox)
#add_subdirectory(vector_data_check_odata_toolbox)
#add_subdirectory(vector_data_check_shp_toolbox)
#add_subdirectory(vector_data_preprocessing_toolbox)
#add_subdirectory(vector_data_tile_customization_toolbox)

# headers installed in qgis_core target
