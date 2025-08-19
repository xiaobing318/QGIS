# ARM Linux交叉编译工具链配置文件示例
# 使用方法: cmake .. -DCMAKE_TOOLCHAIN_FILE=../arm-toolchain.cmake

# 目标系统
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 交叉编译器位置（根据实际情况修改）
set(CMAKE_C_COMPILER   /usr/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/arm-linux-gnueabihf-g++)

# 查找程序的根路径
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)

# 调整查找库和包的行为
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 设置编译flags（可选）
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfpu=neon -mfloat-abi=hard")