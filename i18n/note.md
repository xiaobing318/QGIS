以下是 CMakeLists.txt 文件的处理步骤，将有条理地罗列出来，回答下列三个问题：
 - 说明每个步骤的作用是什么？
 - 为什么需要这样做？
 - 这么做的目的是什么？

---

### **1. 查找 Qt 的 LinguistTools 组件**
**代码：**
```cmake
find_package(${QT_VERSION_BASE} COMPONENTS LinguistTools REQUIRED)
set(QT_LRELEASE_EXECUTABLE ${QT_VERSION_BASE}::lrelease)
```

- **作用**：
  这段代码让 CMake 在系统中查找 Qt 提供的 LinguistTools 工具包，特别是其中的 `lrelease` 工具。`lrelease` 是用来把翻译源文件（.ts 文件）转换成软件可用的二进制文件（.qm 文件）的命令行工具。
- **为什么需要这样做**：
  Qt 框架支持多语言功能，但这些工具不是默认就可用的，需要通过 `find_package` 明确告诉 CMake 去加载它们。如果不这样做，CMake 不知道去哪里找 `lrelease`，翻译文件就无法处理。
- **目的**：
  确保构建系统能找到并使用 `lrelease` 工具，为后续生成翻译文件打下基础。没有这个工具，软件就无法支持多语言。

---

### **2. 定义宏 ADD_TRANSLATION_FILES**
**代码：**
```cmake
macro(ADD_TRANSLATION_FILES _sources )
    foreach (_current_FILE ${ARGN})
      get_filename_component(_in ${_current_FILE} ABSOLUTE)
      get_filename_component(_basename ${_current_FILE} NAME_WE)
      set(_out ${QGIS_OUTPUT_DIRECTORY}/i18n/${_basename}.qm)
      add_custom_command(
         OUTPUT ${_out}
         COMMAND ${QT_LRELEASE_EXECUTABLE}
         ARGS -silent ${_in} -qm ${_out}
         DEPENDS ${_in}
      )
      set(${_sources} ${${_sources}} ${_out} )
   endforeach (_current_FILE)
endmacro(ADD_TRANSLATION_FILES)
```

- **作用**：
  这段代码定义了一个宏（macro），用于批量处理翻译文件。它会遍历传入的每个 .ts 文件，提取文件名，指定输出路径（生成 .qm 文件），然后用 `lrelease` 把 .ts 文件转换成 .qm 文件，最后把生成的 .qm 文件添加到来源列表中。
- **为什么需要这样做**：
  QGIS 支持多种语言，手动为每种语言写转换命令太麻烦。这个宏就像一个“自动化工具”，能一次性处理所有翻译文件，减少重复工作，提高效率。
- **目的**：
  自动化翻译文件的生成过程，确保所有语言的翻译都能被正确编译并包含在软件中。

---

### **3. 确保输出目录存在**
**代码：**
```cmake
file(MAKE_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/i18n)
```

- **作用**：
  这行代码检查并创建存放 .qm 文件的目录（`${QGIS_OUTPUT_DIRECTORY}/i18n`）。如果目录不存在，CMake 会自动创建它。
- **为什么需要这样做**：
  如果没有这个目录，后续生成的 .qm 文件无处存放，构建过程会失败。提前创建目录是为了避免这种情况。
- **目的**：
  为翻译文件准备好存储位置，确保生成的文件有地方放。

---

### **4. 定义翻译文件列表并生成 .qm 文件**
**代码：**
```cmake
set(TS_FILES qgis_ar.ts qgis_az.ts ... qgis_zh-Hant.ts)
ADD_TRANSLATION_FILES (QM_FILES ${TS_FILES})
```

- **作用**：
  先定义一个包含所有 .ts 文件的列表（每种语言一个文件），然后调用 `ADD_TRANSLATION_FILES` 宏，把这些 .ts 文件转换成 .qm 文件，并将结果存到 `QM_FILES` 变量中。
- **为什么需要这样做**：
  .ts 文件是人类可读的翻译源文件，但软件运行时需要更高效的 .qm 文件。这个步骤把“原料”加工成“成品”，让软件能加载翻译。
- **目的**：
  为 QGIS 生成所有语言的翻译文件，确保软件运行时能根据用户语言加载相应的翻译。

---

### **5. 配置并生成 Linux 平台的 .desktop 和 .appdata.xml 文件**
**代码：**
```cmake
configure_file(${CMAKE_SOURCE_DIR}/scripts/ts2appinfo.py.in ${CMAKE_BINARY_DIR}/ts2appinfo.py)
if (UNIX AND NOT APPLE AND PYQT_FOUND)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/org.qgis.qgis.desktop ${CMAKE_BINARY_DIR}/org.qgis.qgis.appdata.xml
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMAND ${Python_EXECUTABLE}
    ARGS ${CMAKE_BINARY_DIR}/ts2appinfo.py "${CMAKE_BINARY_DIR}"
    COMMENT "Updating appinfo files..."
    DEPENDS ${QM_FILES} ${CMAKE_SOURCE_DIR}/linux/org.qgis.qgis.desktop.in ${CMAKE_SOURCE_DIR}/linux/org.qgis.qgis.appdata.xml.in
  )
  set(MD_FILES ${CMAKE_BINARY_DIR}/org.qgis.qgis.desktop ${CMAKE_BINARY_DIR}/org.qgis.qgis.appdata.xml)
  install(FILES ${CMAKE_BINARY_DIR}/org.qgis.qgis.desktop DESTINATION share/applications)
  install(FILES ${CMAKE_BINARY_DIR}/org.qgis.qgis.appdata.xml DESTINATION share/metainfo)
endif()
```

- **作用**：
  这段代码在 Linux 平台（非苹果系统，`对于 GNU/Linux 平台这部分内容是需要关注的，因为需要国产化，在后续涉及到这部分内容的时候再详细查看这部分内容。`）生成两个文件：
  - `.desktop` 文件：类似快捷方式，让 QGIS 出现在 Linux 的应用程序菜单中。
  - `.appdata.xml` 文件：提供软件元数据（如描述、截图），供应用商店使用。
  它通过一个 Python 脚本（ts2appinfo.py）生成这些文件，并安装到指定目录。
- **为什么需要这样做**：
  在 Linux 上，.desktop 文件让用户能从菜单方便地启动 QGIS，否则只能用命令行启动；.appdata.xml 文件则提供软件信息，方便用户了解 QGIS。这些文件需要动态生成，因为它们可能依赖翻译内容。
- **目的**：
  提升 Linux 用户体验，让 QGIS 更容易被发现和使用。

---

### **6. 创建自定义目标 translations**
**代码：**
```cmake
add_custom_target (translations ALL DEPENDS ${QM_FILES} ${MD_FILES})
```

- **作用**：
  定义一个名为 `translations` 的自定义目标，依赖于所有 .qm 文件和 Linux 的 .desktop、.appdata.xml 文件。`ALL` 表示这个目标会在默认构建时执行。
- **为什么需要这样做**：
  CMake 默认只构建代码相关的目标，但翻译文件和元数据文件也需要生成。这个自定义目标确保它们在构建过程中被包含。
- **目的**：
  保证每次构建 QGIS 时，翻译和元数据文件都能更新，用户得到完整的软件。

---

### **7. 设置依赖关系**
**代码：**
```cmake
if (WITH_DESKTOP)
  add_dependencies (translations ${QGIS_APP_NAME})
endif()
```

- **作用**：
  如果启用了 `WITH_DESKTOP` 选项（通常表示构建桌面版本），让 `translations` 目标依赖于 QGIS 主程序（`${QGIS_APP_NAME}`）。这意味着先构建主程序，再生成翻译。
- **为什么需要这样做**：
  翻译文件可能需要主程序中的资源或信息，先构建主程序可以确保翻译生成时所需的一切都已就绪。
- **目的**：
  保持构建的正确顺序，避免因依赖缺失导致的失败。

---

### **8. 安装翻译文件**
**代码：**
```cmake
install (FILES ${QM_FILES} DESTINATION ${QGIS_DATA_DIR}/i18n)
```

- **作用**：
  在安装 QGIS 时，把所有 .qm 文件复制到 `${QGIS_DATA_DIR}/i18n` 目录下。
- **为什么需要这样做**：
  软件运行时会从这个目录加载翻译文件。如果不安装到正确位置，用户无法使用多语言功能。
- **目的**：
  确保安装后的 QGIS 能在正确的位置找到翻译文件，支持多语言切换。

---

### **总结**
这段 CMakeLists.txt 文件通过以上 8 个步骤，完成了 QGIS 多语言支持和 Linux 元数据的自动化处理：
1. 查找工具，确保翻译处理能力。
2. 定义宏，简化翻译文件处理。
3. 创建目录，为输出文件准备位置。
4. 生成翻译，把 .ts 文件转为 .qm 文件。
5. 生成元数据，提升 Linux 用户体验。
6. 自定义目标，确保翻译和元数据被构建。
7. 设置依赖，保证构建顺序。
8. 安装文件，让翻译在软件中生效。

这些步骤共同协作，让 QGIS 支持多语言、在 Linux 上易于使用，同时减少手动操作的繁琐，确保构建和安装过程高效可靠。
