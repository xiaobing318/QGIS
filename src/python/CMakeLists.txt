set(QGISPYTHON_SRCS qgispython.cpp qgspythonutilsimpl.cpp)

add_library (qgispython SHARED ${QGISPYTHON_SRCS})

# require c++17
target_compile_features(qgispython PRIVATE cxx_std_17)

target_include_directories(qgispython PUBLIC
  ${CMAKE_SOURCE_DIR}/src/python
  ${CMAKE_BINARY_DIR}/src/python
)
#[[
杨小兵-2024-03-22
- **`SYSTEM`**：这个关键字告诉 CMake，接下来指定的包含目录应该被视为系统包含目录。这有几个影响：
  - 当编译器支持时，它可以抑制这些目录中的代码所产生的警告，因为假设系统头文件不会被修改，且任何警告都不应由用户代码修复。
  - 它可能影响编译器的搜索顺序，通常系统头文件会先于非系统头文件被搜索。

  - **`PRIVATE`**：这个关键字指定了包含目录的范围。`PRIVATE` 意味着包含目录仅用于该目标的编译，而不会影响依赖该目标的其他
目标。这关乎于目标的接口设计——即决定哪些信息是公开给其他依赖目标的，哪些是仅限于目标本身内部使用的。`PRIVATE` 的包含目录只在当前目标的编译中被使用。

- **`${Python_INCLUDE_DIRS}`**：这是一个变量，表示 Python 头文件的位置。在配置 CMake 项目时，通常会通过 `find_package(PythonLibs)` 
命令查找 Python 库，并设置一些变量，其中 `${Python_INCLUDE_DIRS}` 就是设置为 Python 头文件目录的变量。这些目录需要被添加到编译器的包含
目录中，以便在编译 `qgispython` 目标时能找到 Python 的头文件。其中Python_INCLUDE_DIRS将会指向OSGeo4W64中apps内部的python39的头文件
]]
target_include_directories(qgispython SYSTEM PRIVATE
  ${Python_INCLUDE_DIRS}
)

set_target_properties(qgispython PROPERTIES
  CLEAN_DIRECT_OUTPUT 1
)

#generate unversioned libs for android
if (NOT ANDROID)
  set_target_properties(qgispython PROPERTIES
    VERSION ${COMPLETE_VERSION}
    SOVERSION ${COMPLETE_VERSION}
  )
endif()

target_link_libraries(qgispython
  qgis_core
  ${Python_LIBRARIES}
  ${OPENPTY_LIBRARY}
)

GENERATE_EXPORT_HEADER(
   qgispython
   BASE_NAME PYTHON
   EXPORT_FILE_NAME qgis_python.h
 )

if (APPLE)
  set_target_properties(qgispython PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
endif()

install(TARGETS qgispython
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
  LIBRARY DESTINATION ${QGIS_LIB_DIR}
)
