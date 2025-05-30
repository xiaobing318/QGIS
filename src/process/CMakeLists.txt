########################################################
# Files
#[[
杨小兵-2024-03-22
- `json_fwd.hpp`用于优化编译过程，通过减少不必要的头文件依赖来加速编译，同时提供对JSON数据的处理能力。
- `sigwatch.cpp`使QGIS能够处理UNIX信号，以优雅地管理程序的启动和终止，特别是在UNIX和Linux系统上，增强了程序的健壮性和用户体验。
]]
set(QGIS_PROCESS_SRCS
  main.cpp
  qgsprocess.cpp
  ${CMAKE_SOURCE_DIR}/external/nlohmann/json_fwd.hpp
)

if (UNIX AND NOT ANDROID)
  set(QGIS_PROCESS_SRCS 
    ${QGIS_PROCESS_SRCS}
    ${CMAKE_SOURCE_DIR}/external/qt-unix-signals/sigwatch.cpp
  )
endif()




















########################################################
# Build
#[[
杨小兵-2024-03-22

一、add_executable参数解释
- **可执行文件名称**：在`add_executable`命令中，第一个参数是将要生成的可执行文件的名称。在上面的例子中，`qgis_process`就是最终生成的可执行文件或
应用程序的名称。
- **MACOSX_BUNDLE**（可选）：这是一个特殊的参数，仅在构建macOS应用程序包时使用。当指定`MACOSX_BUNDLE`时，CMake会构建一个.app应用程序包而不是一个
简单的可执行文件。这个应用程序包将包含可执行文件和它可能依赖的任何资源文件或库。如果你的项目是一个GUI应用程序，且打算在macOS上分发，这个选项非常有用。
- **源文件列表**：在命令的这个部分，你需要列出构建可执行文件所需的所有源文件。可以直接列出文件名，也可以使用变量。在给出的例子中，`${QGIS_PROCESS_SRCS}`
是一个变量，代表一个包含所有必需源文件路径的列表。这个列表可能是通过之前的`set`命令或其他CMake命令构建的。

二、MACOSX_BUNDLE参数解释
  在Windows或Linux平台上构建项目时，`MACOSX_BUNDLE`参数实际上是被忽略的，不会产生错误或影响构建过程。CMake智能地处理了这个参数，使得同一个
`CMakeLists.txt`文件可以在不同平台上使用，而不需要针对每个平台写不同的构建脚本。这样做的目的是为了增强CMake项目的跨平台兼容性和灵活性。
  如果你的项目只是针对Windows或Linux平台，包含`MACOSX_BUNDLE`参数并不会对构建过程产生影响，但为了保持脚本的清晰和准确，如果不打算在MacOS上构建，
你可以选择不加这个参数。相反，如果你确实打算提供跨平台的构建支持，包括这个参数是很有意义的，它能让你的CMake脚本在需要的时候为MacOS用户生成.app应用包。
]]
add_executable (qgis_process MACOSX_BUNDLE ${QGIS_PROCESS_SRCS})

#[[
杨小兵-2024-03-22

一、解释
- **命令本身**：`target_compile_features`是CMake的一个命令，用于为特定的目标（如库或可执行文件）指定必需的编译器特性。CMake根据这些特性要求自动确
定编译器标志。

- **第一个参数（目标名称）**：在这个例子中，目标是`qgis_process`。这个参数指明了哪个构建目标（可执行文件、库等）的编译选项将被设置。这意味着
`qgis_process`目标将被配置为使用C++17标准进行编译。

- **第二个参数（可见性）**：`PRIVATE`指的是这个编译特性的可见性或者说是作用范围。在CMake中，可见性分为`PRIVATE`、`INTERFACE`和`PUBLIC`。
  - `PRIVATE`意味着指定的编译特性只对当前目标有效，不会影响链接该目标的其他目标。
  - `INTERFACE`意味着特性不适用于这个目标本身，但适用于链接此目标的其他目标。
  - `PUBLIC`意味着既适用于这个目标本身，也适用于链接此目标的其他目标。

- **第三个参数（编译特性）**：`cxx_std_17`指定了编译此目标时必须使用的C++标准，即C++17。这告诉CMake自动添加适当的编译器标志，以确保使用C++17标准
进行编译，不管使用的是哪种编译器（GCC、Clang、MSVC等）。这对于使用了C++17新特性的项目非常重要，确保了代码的正常编译和运行。

]]
# require c++17
target_compile_features(qgis_process PRIVATE cxx_std_17)




#[[
一、解释
### `target_include_directories`命令详解
- **命令本身**：`target_include_directories`命令用于为目标（如可执行文件或库）添加包含（或头文件）目录。这些目录在编译时被编译器用于搜索头文件。

### 参数详解
- **第一个参数（目标名称）**：`qgis_process`是命令的目标，即要为其添加包含目录的可执行文件或库。
- **第二个参数（可见性）**：`PUBLIC`表示添加的这些目录不仅仅会影响到当前目标在编译时的头文件搜索路径，而且也会影响到任何链接了这个目标的其他目标。
这是一种传播性质，确保了依赖关系中的其他目标也能够找到需要的头文件。
- **后面的参数**：`CMAKE_CURRENT_BINARY_DIR`指定了一个包含目录，这个目录是当前项目二进制文件（编译生成的文件）的路径。

### `CMAKE_CURRENT_BINARY_DIR`与`CMAKE_CURRENT_SOURCE_DIR`的区别
- **`CMAKE_CURRENT_SOURCE_DIR`**：指的是当前处理的`CMakeLists.txt`文件所在的目录。这个目录包含你的源代码和项目文件。使用
`CMAKE_CURRENT_SOURCE_DIR`可以引用源代码树中的位置，不论构建过程在哪里执行。

- **`CMAKE_CURRENT_BINARY_DIR`**：指的是当前项目的二进制目录的路径，即CMake进行构建过程的地方。对于out-of-source构建
（源代码和生成的构建文件在不同目录的构建方式），这个目录和`CMAKE_CURRENT_SOURCE_DIR`是不同的。使用`CMAKE_CURRENT_BINARY_DIR`可以引用生成的文件，
比如编译生成的对象文件或者CMake配置文件等。
]]
target_include_directories(qgis_process PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}
)

if (UNIX AND NOT ANDROID)
  include_directories(SYSTEM
    ${CMAKE_SOURCE_DIR}/external/qt-unix-signals
  )
endif()

if (WITH_3D)
  include_directories(SYSTEM
    ${QT5_3DEXTRA_INCLUDE_DIR}
  )
endif()

target_link_libraries(qgis_process
  qgis_core
  qgis_analysis
  ${QT_VERSION_BASE}::Core
  ${PROJ_LIBRARY}
  ${GEOS_LIBRARY}
  ${GDAL_LIBRARY}
)

if (WITH_3D)
  target_link_libraries(qgis_process
    qgis_3d
  )
endif()

#[[
杨小兵-2024-03-22
一、解释
  如果定义了WITH_BINDINGS变量，那么应该向qgis_process目标链接一个名为qgispython的库，并且添加预处理定义
]]
if (WITH_BINDINGS)
  target_link_libraries(qgis_process qgispython)
  add_definitions(-DWITH_BINDINGS)
endif()





#[[
杨小兵-2024-03-22
### `install`命令详解
- **命令本身**：`install`是CMake的一个命令，用于定义安装规则。它告诉CMake在构建项目的安装阶段应该做什么。这包括将可执行文件、库文件、
头文件等复制到指定的目录，可以是系统级的目录或者用户定义的任何位置。

### 参数详解
- **`TARGETS`**：这个关键字后面跟随的是要安装的目标名称，在此例中是`qgis_process`。这表示`qgis_process`是一个构建目标（比如可执行文件
或库），CMake将为其生成安装规则。

- **`BUNDLE DESTINATION`**：这个选项专门用于MacOS，指定了当目标是一个应用程序包（bundle）时应该被安装到的目录。这里使用的变量`${QGIS_BIN_DIR}`
表示安装目录的路径。这个路径是在CMake配置阶段之前定义的，通常指向了安装二进制文件的目录。

- **`RUNTIME DESTINATION`**：这个选项指定了可执行文件在安装时的目的地。对于非MacOS平台，这意味着`qgis_process`可执行文件将被安装到
`${QGIS_BIN_DIR}`指定的目录。同样，`${QGIS_BIN_DIR}`是一个变量，需要在CMake配置中提前定义。

### 总结
  这段代码的作用是确保无论在哪个平台上构建`qgis_process`，都能够将其正确安装到指定的二进制文件目录`${QGIS_BIN_DIR}`中。对于MacOS，
如果`qgis_process`是一个应用程序包，它将被安装到`BUNDLE DESTINATION`指定的位置；对于其他平台，如Windows或Linux，`qgis_process`的可执行文件将
被安装到`RUNTIME DESTINATION`指定的位置。

]]
# install
install (TARGETS qgis_process
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)

