
########################################################
# Files
#[[
杨小兵-2024-02-24
  设置offline_editing_plugin_SRCS、offline_editing_plugin_UIS、offline_editing_plugin_RCCS三个变量用来存储该cmakelists文件
所在目录下的多个文件的路径：cpp文件、ui文件、qrc资源文件
]]

set (offline_editing_plugin_SRCS
  offline_editing_plugin.cpp
  offline_editing_plugin_gui.cpp
  offline_editing_progress_dialog.cpp
)

set (offline_editing_plugin_UIS
  offline_editing_plugin_guibase.ui
  offline_editing_progress_dialog_base.ui
)

set (offline_editing_plugin_RCCS
  offline_editing_plugin.qrc)

########################################################
# Build
#[[
杨小兵-2024-02-24
  这段代码是一个CMake构建脚本的一部分，用于配置和编译一个名为`plugin_offlineediting`的插件模块。它展示了如何基于不同的条件（如Qt版本）、
如何编译源代码、如何设置编译目标的特定属性（如C++标准），以及如何链接依赖库。下面将从处理逻辑和作用两个方面进行详细解释。

### 处理逻辑

1. **条件编译Qt版本**:
   - `if (BUILD_WITH_QT6)`检查是否定义了`BUILD_WITH_QT6`变量，这是一个根据项目需求选择Qt版本的条件编译指令。如果条件为真（即项目希望使用Qt 6），
  则使用`QT6_WRAP_UI`命令处理UI文件；否则，使用`QT5_WRAP_UI`命令。这两个命令分别为Qt 5和Qt 6版本的UI文件生成C++头文件。（这部分内容将会对ui文件
  进行处理，但是具体是怎么进行处理的还需要查看具体的代码）

2. **添加库**:
   - `add_library`命令用于创建名为`plugin_offlineediting`的模块库（动态库），并指定它由源文件、资源文件和UI头文件组成。这些文件分别由
  `offline_editing_plugin_SRCS`、`offline_editing_plugin_RCCS`和`offline_editing_plugin_UIS_H`变量提供路径。

3. **设置C++17标准**:
   - `target_compile_features`命令指定编译此库时使用C++17标准。`PRIVATE`关键字表示这一编译特性仅适用于此目标，而不影响其他链接的库。

4. **链接依赖库**:
   - `target_link_libraries`命令将`plugin_offlineediting`库与其依赖库链接（这个插件库利用到了其他的库文件）。依赖库包括`qgis_core`、
  `qgis_gui`和`SPATIALITE_LIBRARY`，这些库提供了开发QGIS插件所需的核心功能、图形用户界面组件和空间数据库支持。

5. **包含目录**:
   - 使用`include_directories`和`include_directories(SYSTEM)`命令添加编译时的头文件搜索路径。`SYSTEM`参数指定的目录在编译时会被当作
  系统库目录对待，这样可以避免编译器对这些目录中的代码发出的警告。这些路径包括Spatialite和SQLite3的头文件目录，以及项目内其他相关目录。

### 作用

- **条件编译**：允许插件根据Qt版本动态选择合适的UI文件处理方式，确保与Qt 5和Qt 6的兼容性。
- **动态库构建**：通过编译指定的源代码和资源文件，生成一个动态链接库（DLL或so文件），作为QGIS的插件。
- **编译标准设置**：确保插件使用C++17标准进行编译，利用C++17提供的语言特性和标准库改进。
- **依赖管理**：确保插件能够链接到其依赖的库，如QGIS核心库、GUI库和Spatialite空间数据库库，这对于插件的功能实现至关重要。
- **包含路径设置**：通过明确指定编译器在编译过程中查找头文件的路径，解决头文件查找和依赖关系问题，避免编译错误和警告。
  
  综上所述，这段CMake脚本在确保代码与不同版本的Qt兼容、遵循现代C++标准、正确管理依赖关系及其编译环境配置方面发挥了重要作用，为开发和维护
QGIS插件提供了一个结构化和自动化的方式。

### 处理UI文件命令解释
  `QT5_WRAP_UI`是CMake中用于处理Qt5用户界面（UI）文件的宏（首先这是CMake中的一个宏）。在开发使用Qt框架的应用程序时，通常会使用Qt Designer
来设计用户界面，这个过程会生成`.ui`文件。`.ui`文件是用XML格式描述的用户界面布局和元素（ui文件是一种XML格式的文件）。为了在C++代码中使用这些
界面，需要将`.ui`文件转换成C++源代码。`QT5_WRAP_UI`宏正是用于这个转换过程的（总结：Cmake中的QT5_WRAP_UI是用来将*.ui文件转化成C++源代码的
一个命令）。

### 处理原理(当`QT5_WRAP_UI`宏被调用时，它会执行以下步骤)
1. **扫描指定的UI文件**：它接收两个参数：第一个参数是输出变量（在这个例子中是`offline_editing_plugin_UIS_H`），用于存储生成的头文件列表；
第二个参数是输入的`.ui`文件列表（在这个例子中是`${offline_editing_plugin_UIS}`变量所指定的）。(第一个参数是用来存储转化后的C++源代码)

2. **生成C++源代码**：对于每个`.ui`文件，`QT5_WRAP_UI`宏会调用`uic`工具（UI编译器）。`uic`读取`.ui`文件，并生成一个对应的C++头文件，这个
头文件中包含了创建和布局UI组件所需的代码。

3. **更新输出变量**：生成的C++头文件路径被添加到输出变量（`offline_editing_plugin_UIS_H`）中。这使得这些文件可以在项目的其他部分被引用和编译。

### 处理细节
- **文件命名**：生成的头文件通常会根据原始`.ui`文件的名称命名，但是会替换扩展名为`.h`。例如，如果UI文件名为`mainwindow.ui`，生成的C++头文件名
可能会是`ui_mainwindow.h`。

- **代码生成**：生成的C++代码包含了构建UI所需的所有Qt widgets和布局。这意味着开发者可以直接在C++源文件中包含这些生成的头文件，通过编程方式操作
UI元素，而无需手动编写布局代码。

### 谁来进行处理
- **CMake和uic工具**：`QT5_WRAP_UI`宏由CMake处理，但实际的UI文件到C++代码的转换是由Qt的`uic`工具执行的。CMake负责在构建过程中自动调用`uic`，
确保UI文件的改动能够及时反映在生成的C++代码中。（CMake处理QT5_WRAP_UI`宏，然后*.ui---->*.h将会通过uic编译器工具得到）

### 总结
  `QT5_WRAP_UI`宏简化了Qt应用程序开发流程，自动化了将设计师创建的UI布局转换为开发者可以在代码中直接使用的C++类的过程。这一自动化处理极大地提升了
开发效率，允许开发者专注于业务逻辑的实现，而不是UI布局的手动编码。通过CMake和`uic`工具的协作，确保了UI设计的改动可以无缝集成到应用程序的构建过程中。
]]

if (BUILD_WITH_QT6)
  QT6_WRAP_UI(offline_editing_plugin_UIS_H ${offline_editing_plugin_UIS})
else()
  QT5_WRAP_UI(offline_editing_plugin_UIS_H ${offline_editing_plugin_UIS})
endif()

add_library (plugin_offlineediting MODULE
  ${offline_editing_plugin_SRCS}
  ${offline_editing_plugin_RCCS}
  ${offline_editing_plugin_UIS_H}
)

# require c++17
target_compile_features(plugin_offlineediting PRIVATE cxx_std_17)

target_link_libraries(plugin_offlineediting
  qgis_core
  qgis_gui
  ${SPATIALITE_LIBRARY}
)
# 头文件的搜索路径
include_directories(SYSTEM
  ${SPATIALITE_INCLUDE_DIR}
  ${SQLITE3_INCLUDE_DIR}
)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/plugins

  ${CMAKE_CURRENT_BINARY_DIR}
)

########################################################
# Install
#[[
杨小兵-2024-02-24
  这段代码是CMake脚本的一部分，用于安装名为`plugin_offlineediting`的目标（通常是一个动态库或模块），并指定其安装路径。在讨论软件开发和
部署时，"安装"是将编译生成的可执行文件、库或其他资源复制到系统的指定位置，以便用户或其他程序可以找到并使用它们的过程。下面将从不同角度解释
这段代码的含义和作用。

### 处理逻辑
- `install(TARGETS plugin_offlineediting ...)`命令用于安装名为`plugin_offlineediting`的目标。这里的目标是指由之前`add_library`命令创建的库。

- `RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}`和`LIBRARY DESTINATION ${QGIS_PLUGIN_DIR}`指定了安装目标的位置。`RUNTIME DESTINATION`用于可执
行文件（在Windows上是.dll文件），而`LIBRARY DESTINATION`用于动态链接库（在Linux和macOS上是.so或.dylib文件）。这两个选项确保无论目标是可执行文
件还是库文件，都会被安装到`${QGIS_PLUGIN_DIR}`指定的目录中。

### 作用
- **插件部署**：通过将编译后的插件安装到QGIS的插件目录（`${QGIS_PLUGIN_DIR}`变量指定），使得QGIS在启动时能够自动加载和识别该插件。这是将开发的插
件集成到QGIS环境中，使其对最终用户可用的关键步骤。

- **路径灵活性**：使用变量`${QGIS_PLUGIN_DIR}`来指定安装路径，提高了脚本的灵活性和可配置性。这意味着如果安装环境或需求变化，只需修改这个变量的值，
而不需要修改安装命令本身。

- **跨平台兼容**：通过区分`RUNTIME`和`LIBRARY`目的地，这段代码适用于多种操作系统。这体现了CMake脚本在跨平台构建和部署方面的优势，能够自动处理不同
平台间的差异。

  综上所述，这段安装命令使得开发者可以轻松地将QGIS插件部署到正确的位置，无论在开发环境还是用户环境中。这样不仅提升了开发和部署的效率，也保证了插件能
够被QGIS正确加载和使用。
]]
install(TARGETS plugin_offlineediting
	RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
	LIBRARY DESTINATION ${QGIS_PLUGIN_DIR}
)
