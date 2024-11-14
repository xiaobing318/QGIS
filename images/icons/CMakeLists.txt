#[[
杨小兵-2024-03-05
1. **`include(CopyResources)`**:
  - 这行代码包含了`CopyResources`模块或脚本。在CMake中，`include`命令用于载入并运行CMake代码，这可以是预定义的模块或者用户自定义的脚本。
`CopyResources`可能是一个为了处理和复制资源文件（比如图标、文档等）而定义的自定义脚本或模块。（我在QGIS自定义的cmake文件中找到了这个模块）
]]
include(CopyResources)

#[[
杨小兵-2024-03-05
2. **条件语句 `if (APPLE)`**:
  - 这是一个条件判断，用于检查是否在Apple操作系统上运行。`APPLE`是CMake预定义的变量，当在Apple系统（如macOS）上运行时为真。
  - `add_subdirectory(mac)`: 如果上述条件为真，即在Apple系统上运行，这行代码会将名为`mac`的子目录添加到构建过程中。这个子目录可能包含特定
于Mac OS X应用程序和文档图标的资源和配置。

]]
# OS X app/doc icons
if (APPLE)
  add_subdirectory(mac)
endif()

#[[
杨小兵-2024-03-05
3. **查找图标文件 `file(GLOB_RECURSE ICON_FILES ...)`**:
   - `GLOB_RECURSE`指令用于递归地在给定目录及其所有子目录中查找匹配特定模式的文件。这里，它被用来查找所有的`.png`文件。
   - `ICON_FILES`是一个变量，用于存储找到的所有PNG文件路径。
   - `RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}`指定查找文件时使用的路径是相对于当前CMake脚本目录的。

]]
file(GLOB_RECURSE ICON_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.png)
# 杨小兵-2024-03-29：应该将.svg的文件同时添加到输出目录中，这样在程序启动的时候也能找到对应的icons
file(GLOB_RECURSE ICON_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.svg)

#[[
杨小兵-2024-03-05
4. **`ADD_QGIS_RESOURCES(...)`**:
   - 这个自定义命令可能是专为处理QGIS项目中的资源而设置的。它的目的是将图标文件从源目录复制到目标目录，并可能进行一些处理。
   - `"${CMAKE_CURRENT_SOURCE_DIR}" "images/icons"`指明源目录和资源的相对路径。
   - `DEST_ICON_FILES`和`"${ICON_FILES}"`指定目标变量和源文件列表。
]]
ADD_QGIS_RESOURCES("${CMAKE_CURRENT_SOURCE_DIR}" "images/icons" DEST_ICON_FILES "${ICON_FILES}")

#[[
杨小兵-2024-03-05
5. **创建自定义目标 `add_custom_target(icons ALL DEPENDS ${DEST_ICON_FILES})`**:
   - `add_custom_target`命令用于创建一个不产生输出文件的目标，这里创建的目标名为`icons`。
   - `ALL`关键字表示这个目标将被默认构建。
   - `DEPENDS ${DEST_ICON_FILES}`指定了这个目标依赖于前面`ADD_QGIS_RESOURCES`命令处理后的图标文件。这意味着只有在所有图标
  文件都被正确处理和复制后，`icons`目标才会被认为是完成的。

]]
add_custom_target(icons ALL DEPENDS ${DEST_ICON_FILES})
