#[[
杨小兵-2024-03-05
  这段代码是CMake脚本的一部分，用于设置构建QGIS插件时生成文件的输出目录。

应用场景：
  在开发QGIS插件时，通常需要编译插件并将其放置在特定目录下，以便QGIS能够识别和加载这些插件。默认情况下，CMake可能会将构建的可执行文件
和库文件放置在构建目录下的预设路径中。然而，在开发过程中，直接将构建好的插件放置在QGIS的插件目录中，可以避免安装步骤，从而加速开发和测试过程。

计算机实现角度：
- `set`命令：在CMake中，`set`命令用于定义一个变量及其值。这里，它被用来设置两个目录变量，`CMAKE_RUNTIME_OUTPUT_DIRECTORY` 和 
`CMAKE_LIBRARY_OUTPUT_DIRECTORY`。

- `${QGIS_OUTPUT_DIRECTORY}/${QGIS_PLUGIN_SUBDIR}`：这是一个表达式，用于指定输出目录的路径。`${QGIS_OUTPUT_DIRECTORY}` 和
 `${QGIS_PLUGIN_SUBDIR}` 是变量，它们分别存储了QGIS输出目录和插件子目录的路径。使用这两个变量，可以灵活地定义插件的输出路径，无
 论QGIS安装在计算机上的什么位置。

  - `CMAKE_RUNTIME_OUTPUT_DIRECTORY`：这个变量指定了可执行文件（例如，插件的可执行部分）的输出目录。
  
  - `CMAKE_LIBRARY_OUTPUT_DIRECTORY`：这个变量指定了库文件（例如，插件的动态链接库或共享对象文件）的输出目录。

  通过设置这些目录，开发者可以确保编译后的插件直接放置在QGIS能够识别的插件目录中，从而使得开发和测试过程更加高效。这种方法避免了将插
件手动复制到插件目录的步骤，简化了插件的迭代开发流程。

]]
# override default path where built files are put to allow running qgis without installing
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/${QGIS_PLUGIN_SUBDIR})
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/${QGIS_PLUGIN_SUBDIR})

#[[
杨小兵-2024-03-05
  根据不同的标志设置，包含不同的子目录进行处理各自子目录中的cmaklists.txt文件
]]
# providers with implemented both static and dynamic building
add_subdirectory(wms)
add_subdirectory(wcs)
add_subdirectory(delimitedtext)
add_subdirectory(arcgisrest)

#[[
杨小兵-2024-03-05
  如果标志存在，那么包含子目录
]]
if (WITH_SPATIALITE)
  add_subdirectory(spatialite)
  add_subdirectory(wfs)
  add_subdirectory(virtual)
endif()

#[[
杨小兵-2024-03-05
  如果标志存在，那么包含子目录
]]
if (POSTGRES_FOUND)
  add_subdirectory(postgres)
endif()

#[[
杨小兵-2024-03-05
  如果标志不存在，那么包含子目录
]]
if (NOT FORCE_STATIC_LIBS)
  add_subdirectory(geonode)
  add_subdirectory(mssql)
  add_subdirectory(gpx)
  add_subdirectory(mdal)
  if (WITH_ANALYSIS)
    add_subdirectory(virtualraster)
  endif()

  if (WITH_ORACLE)
    add_subdirectory(oracle)
  endif()

  if (WITH_HANA)
    add_subdirectory(hana)
  endif()

  if (WITH_PDAL)
    add_subdirectory(pdal)
  endif()

  if (WITH_GRASS OR WITH_GRASS7 OR WITH_GRASS8)
    add_subdirectory(grass)
  endif()
endif()
