#[[
杨小兵-2024-02-24
  这段代码主要用于一个基于CMake的构建系统中，用来组织和编译一个较大的项目（如QGIS），其中包含多个组件和模块。`add_subdirectory`命令
在CMake中被广泛使用，用于将项目中的子目录加入到主项目的构建过程中。每个子目录通常包含一个`CMakeLists.txt`文件，定义了该目录中的构建
规则和依赖关系。（总结：用来将项目中的子目录加入到主项目的构建过程中，每一个子目录通常都会包含一个CMakeLists.txt，这个CMakeLists.txt
定义了如何构建子项目中的规则和依赖关系）

### 作用
  上述代码段通过条件编译的方式，根据不同的配置选项（如`WITH_AUTH`、`WITH_ANALYSIS`等），选择性地添加子目录到构建过程中。这种方法使得
项目可以根据需求或者配置选项来包含或排除特定的功能模块，从而提高构建的灵活性和定制性。例如，如果项目配置为支持认证模块（`WITH_AUTH`为真），
则`auth`目录被加入到构建过程中。

### `add_subdirectory`命令的使用方式（`add_subdirectory`命令的基本语法如下）
```cmake
add_subdirectory(source_dir [binary_dir] [EXCLUDE_FROM_ALL])
```
- `source_dir`：指定要加入构建的子目录路径。这个路径是相对于当前`CMakeLists.txt`文件的路径。
- `binary_dir`：（可选）指定编译输出的目录。如果不指定，默认在当前构建目录下创建与子目录同名的构建目录。
- `EXCLUDE_FROM_ALL`：（可选）如果设置，这个子目录将不会被包含在顶级的`make all`目标中，意味着除非特别请求，否则不会被构建。

### 处理原理
  当CMake处理到`add_subdirectory`命令时，它会进入指定的子目录，执行那里的`CMakeLists.txt`文件。这意味着每个子目录可以独立定义自己的
构建规则、目标（如库或可执行文件）、依赖关系等。这样做的好处是可以将大型项目分解成多个小的、易于管理的单元，同时保持项目结构的清晰和模块化。

### 总结
  上述代码展示了如何通过`add_subdirectory`命令和条件编译指令，将一个复杂项目的不同部分（如核心库、用户界面、插件、Python绑定等）组织起来，
使得项目可以根据配置选项灵活地编译不同的组件。这种方式大大提高了项目的可配置性和可扩展性，使得开发者可以根据具体需求定制构建过程，同时也便于
项目的维护和开发。
]]
add_subdirectory(core)

if (WITH_AUTH)
  add_subdirectory(auth)
endif()

if (WITH_ANALYSIS)
  add_subdirectory(analysis)
endif()

if (WITH_GUI)
  add_subdirectory(ui)
  add_subdirectory(gui)
  add_subdirectory(native)
endif()

add_subdirectory(providers)

add_subdirectory(crssync)

if (WITH_CRASH_HANDLER)
  add_subdirectory(crashhandler)
endif()

add_subdirectory(test)

if (WITH_DESKTOP)
  add_subdirectory(app)
  add_subdirectory(plugins)
endif()

if (WITH_3D)
  add_subdirectory(3d)
endif()

if (WITH_BINDINGS)
  add_subdirectory(python)
endif()

if (WITH_SERVER)
  add_subdirectory(server)
endif()

if (WITH_CUSTOM_WIDGETS)
  add_subdirectory(customwidgets)
endif()

if (WITH_QUICK)
  add_subdirectory(quickgui)
endif()

if (WITH_QGIS_PROCESS)
  add_subdirectory(process)
endif()

add_subdirectory(starmap_src)
