cmake_minimum_required(VERSION 3.10)
project(ShapefileEncodingConverter VERSION 1.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 禁用自动化的Qt处理（如果不需要Qt）
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)

# 设置编译选项
if(MSVC)
    # Windows MSVC编译器选项
    add_compile_options(/W4 /utf-8)
    # 定义宏以避免Windows.h包含过多内容
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
else()
    # GCC/Clang编译器选项
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 主程序
add_executable(shapefile_converter main.cpp)

# 链接必要的库
if(WIN32)
    # Windows平台可能需要的库
    target_link_libraries(shapefile_converter)
else()
    # Linux/Unix平台
    # 某些系统可能需要显式链接filesystem库
    find_library(FILESYSTEM_LIB stdc++fs)
    if(FILESYSTEM_LIB)
        target_link_libraries(shapefile_converter ${FILESYSTEM_LIB})
    endif()

    # ARM Linux可能需要pthread
    find_package(Threads REQUIRED)
    target_link_libraries(shapefile_converter Threads::Threads)
endif()

# 设置输出目录
set_target_properties(shapefile_converter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 安装规则
install(TARGETS shapefile_converter
    RUNTIME DESTINATION bin
)

# 创建一个简单的测试
enable_testing()
add_test(NAME help_test COMMAND shapefile_converter --help)

# 打印配置信息
message(STATUS "====================================")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "====================================")

# 为不同平台设置特定选项
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    message(STATUS "Configuring for ARM Linux")
    # ARM Linux特定配置
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    message(STATUS "Configuring for x86-64 Windows")
    # x86-64 Windows特定配置
endif()
