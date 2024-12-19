set (PROCESSING_PLUGIN_DIR ${QGIS_DATA_DIR}/python/plugins/processing)

add_subdirectory(algs)
add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(images)
add_subdirectory(modeler)
add_subdirectory(script)
add_subdirectory(tests)
add_subdirectory(tools)
add_subdirectory(ui)

file(GLOB OTHER_FILES metadata.txt)
file(GLOB PY_FILES *.py)

PLUGIN_INSTALL(processing . ${PY_FILES} ${OTHER_FILES})
#[[
杨小兵-2024-04-25
一、解释
  上述的CMake命令是用来配置和安装一个QGIS的Processing插件。下面是对每一行命令的详细解释：

1. **`set (PROCESSING_PLUGIN_DIR ${QGIS_DATA_DIR}/python/plugins/processing)`**:
   - 这一行设置了一个变量 `PROCESSING_PLUGIN_DIR`，其值为 `${QGIS_DATA_DIR}/python/plugins/processing`。这里的 `${QGIS_DATA_DIR}` 是一个
  可能在其他地方定义的变量，指向QGIS的数据目录。这个设置指定了Processing插件的安装目录。

2. **`add_subdirectory(<dir>)`**:
   - `add_subdirectory` 命令用于向当前的CMake项目添加子目录。这里列出的子目录（`algs`, `core`, `gui`, `images`, `modeler`, `script`, `tests`,
   `tools`, `ui`）中包含了各自的CMakeLists.txt文件和相关代码，它们会被递归地处理。
   - 每个子目录可能包含特定的源代码文件、资源或者测试文件，它们负责处理插件的不同部分。

3. **`file(GLOB OTHER_FILES metadata.txt)`**:
   - 这条命令使用 `GLOB` 搜索当前目录下所有名为 `metadata.txt` 的文件，并将结果存储在变量 `OTHER_FILES` 中。

4. **`file(GLOB PY_FILES *.py)`**:
   - 类似地，这条命令搜索当前目录下所有扩展名为 `.py` 的Python脚本文件，并将结果存储在变量 `PY_FILES` 中。

5. **`PLUGIN_INSTALL(processing . ${PY_FILES} ${OTHER_FILES})`**:
   - `PLUGIN_INSTALL` 是一个可能在QGIS或CMake模块中定义的宏，用于安装插件。这个命令具体是将`processing` 插件的Python文件 (`${PY_FILES}`) 和其
  他文件 (`${OTHER_FILES}`，例如 `metadata.txt`) 安装到目标目录中。
   - 这里的 `.` 指的是当前源码目录（从这个目录开始查找文件）。

  总的来说，这段CMake脚本负责定义Processing插件的安装路径，添加必要的子目录，搜集所有相关文件，并通过 `PLUGIN_INSTALL` 宏将插件安装到适当的位置，
从而在QGIS中可以被加载和使用。这是一个典型的项目构建配置过程，确保所有必要的组件都被正确地编译和安装。

二、命令解释（file）
  使用 `file(GLOB ...)` 命令在CMake中搜集文件时，它默认仅搜索当前指定目录中的文件，而不会递归地进入子目录中查找。因此，如果您使用如下命令：
```cmake
file(GLOB PY_FILES *.py)
```
  这条命令只会搜集当前目录下的 `.py` 文件，而不会包括任何子目录中的 `.py` 文件。如果您希望包括子目录中的 `.py` 文件，需要使用 `GLOB_RECURSE` 选项，
像这样：
```cmake
file(GLOB_RECURSE PY_FILES *.py)
```
  使用 `GLOB_RECURSE` 会让CMake递归地搜索当前目录及所有子目录中匹配的文件，因此这会包括所有子目录下的 `.py` 文件。这在您需要构建或安装包含多层目录
结构的项目时非常有用。但请注意，递归搜索可能会使构建过程稍微变慢，并且在某些情况下可能引入不期望包括的文件。
]]