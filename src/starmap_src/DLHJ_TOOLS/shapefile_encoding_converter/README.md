# Shapefile Encoding Converter

一个跨平台的命令行工具，用于将Shapefile矢量数据的字符集编码从GBK/GB2312转换为UTF-8。支持在ARM Linux和x86-64 Windows 10平台上运行。

## 功能特性

- ✅ 支持单个Shapefile文件转换
- ✅ 支持批量转换目录中的所有Shapefile
- ✅ 支持递归处理子目录
- ✅ 自动处理Shapefile相关文件（.shx, .prj, .dbf等）
- ✅ 跨平台支持（ARM Linux, x86-64 Windows）
- ✅ 保持原始目录结构（可选）
- ✅ 自定义源编码和目标编码

## 系统要求

### 必须安装的依赖

1. **GDAL/OGR** (2.0或更高版本)
   - 必须包含`ogr2ogr`命令行工具
   - 需要添加到系统PATH环境变量

2. **C++编译器** (支持C++17)
   - Windows: MSVC 2017或更高版本
   - Linux: GCC 7.0或更高版本

3. **CMake** (3.10或更高版本)

### 安装GDAL

#### Windows (x86-64)
```bash
# 使用OSGeo4W安装器
# 下载地址: https://trac.osgeo.org/osgeo4w/
# 或使用conda
conda install -c conda-forge gdal
```

#### Linux (ARM/x86-64)
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install gdal-bin libgdal-dev

# CentOS/RHEL
sudo yum install gdal gdal-devel

# 验证安装
ogr2ogr --version
```

## 编译指南

### 克隆或创建项目
```bash
mkdir shapefile-converter
cd shapefile-converter
# 将main.cpp, CMakeLists.txt放入此目录
```

### Windows编译 (x86-64)
```bash
# 使用Visual Studio 2017或更高版本
mkdir build
cd build
cmake .. -G "Visual Studio 15 2017 Win64"
cmake --build . --config Release

# 或使用MinGW
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### Linux编译 (ARM/x86-64)
```bash
mkdir build
cd build
cmake ..
make -j4
sudo make install  # 可选，安装到系统
```

### 交叉编译 (为ARM Linux编译)
```bash
# 需要ARM工具链
mkdir build-arm
cd build-arm
cmake .. -DCMAKE_TOOLCHAIN_FILE=../arm-toolchain.cmake
make -j4
```

## 使用说明

### 基本语法
```bash
shapefile_converter [选项] <输入路径> [输出路径]
```

### 命令行选项
- `-r, --recursive` : 递归处理子目录
- `-s, --source-enc <编码>` : 指定源文件编码（默认: GBK）
- `-t, --target-enc <编码>` : 指定目标编码（默认: UTF-8）
- `-v, --verbose` : 显示详细执行信息
- `-h, --help` : 显示帮助信息

### 使用示例

#### 1. 转换单个文件
```bash
# 在原地创建_utf8后缀的文件
shapefile_converter input.shp

# 指定输出文件
shapefile_converter input.shp output.shp
```

#### 2. 转换目录中的所有Shapefile（非递归）
```bash
# 在原目录创建_utf8后缀的文件
shapefile_converter /path/to/shapefiles/

# 输出到新目录
shapefile_converter /path/to/shapefiles/ /path/to/output/
```

#### 3. 递归转换所有子目录
```bash
# 递归处理，保持目录结构
shapefile_converter -r /data/gis/china/ /data/gis/china_utf8/

# 使用GB2312作为源编码
shapefile_converter -r -s GB2312 /data/maps/
```

#### 4. 显示详细信息
```bash
shapefile_converter -v -r input_folder/ output_folder/
```

## 工作原理

1. 程序扫描指定路径中的所有`.shp`文件
2. 对每个Shapefile，调用`ogr2ogr`进行编码转换
3. 自动复制相关文件（.shx, .prj等）到输出位置
4. 创建新的`.cpg`文件，标记为UTF-8编码

## 注意事项

1. **备份数据**：转换前请备份原始数据
2. **编码检测**：程序不会自动检测源文件编码，需要手动指定
3. **空间占用**：转换会创建新文件，请确保有足够的磁盘空间
4. **文件完整性**：确保Shapefile的所有相关文件都在同一目录
5. **权限问题**：在Linux上可能需要对输出目录有写入权限

## 常见问题

### Q: 提示找不到ogr2ogr命令
A: 确保GDAL已正确安装并添加到PATH环境变量：
```bash
# Windows
set PATH=%PATH%;C:\OSGeo4W64\bin

# Linux
export PATH=$PATH:/usr/local/bin
```

### Q: 转换后中文显示乱码
A: 可能是源编码指定错误，尝试使用不同的源编码：
```bash
shapefile_converter -s GB2312 input.shp output.shp
# 或
shapefile_converter -s GBK input.shp output.shp
```

### Q: 编译时提示filesystem未找到
A: 某些较旧的编译器需要链接stdc++fs库，CMakeLists.txt已经处理了这种情况。

## 许可证

本项目采用MIT许可证。

## 贡献

欢迎提交Issue和Pull Request！

## 更新日志

### v1.0.0 (2024-01-01)
- 初始版本发布
- 支持基本的编码转换功能
- 支持递归处理
- 跨平台支持