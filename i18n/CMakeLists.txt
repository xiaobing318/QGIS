#[[
Notes:杨小兵-2025-03-21

1、这段 CMakeLists.txt 文件就像一个“厨房里的食谱”，告诉计算机如何为 QGIS 这个软件准备好多语言支持和一些重要的“身份文件”。想象 QGIS 是一个餐厅的菜单，你希望它能让来自世界各地的人看懂（比如中文、法文、日文等），那就需要把菜单翻译成不同的语言。这段代码的工作就是：
  1.1 **把翻译文件“烹饪”成软件能用的格式**：它把一大堆翻译文件（比如 qgis_fr.ts 是法语翻译）变成软件能直接加载的小文件（叫 .qm 文件）。
  1.2 **生成 Linux 系统的“名片”**：在 Linux 上，它还会生成两个文件，一个是“桌面图标文件”（.desktop），让 QGIS 出现在应用程序菜单里；另一个是“描述文件”（.appdata.xml），告诉用户这个软件是干嘛的。
  1.3 **把这些文件放对地方**：它确保翻译文件和“名片”文件在软件安装时被放到正确的位置，让用户打开 QGIS 时一切都准备就绪。
2、这段 CMakeLists.txt 解决了什么问题？
  2.1 **翻译太复杂，手动处理容易出错**：如果没有这段代码，程序员得手动把几十个翻译文件（比如 qgis_en_US.ts、qgis_zh-Hans.ts）转成软件能用的格式，一个一个操作会累死人，还容易漏掉。这段代码自动化了这个过程，就像一台“翻译机器”，一键搞定。
  2.2 **Linux 上不好找软件**：在 Linux 系统上，如果没有 .desktop 文件，用户就得自己去找 QGIS 的启动程序，很麻烦。这段代码自动生成这个文件，让 QGIS 直接出现在菜单里，就像给软件安了个“快捷按钮”。
  2.3 **安装乱七八糟**：翻译文件和“名片”文件如果随便放，用户可能找不到，或者软件用不了。这段代码规定了它们要放进 QGIS 的“家”（安装目录），保持一切井井有条。
3、当前文件相同目录下有一个名为modifications_qt_xx.txt的文件，这个文件没有作用（之前是用来测试commit的），是不需要关心的。
4、对于这段cmake命令的具体解释在当前文件所在目录的名为note.md文件中。
]]

find_package(${QT_VERSION_BASE} COMPONENTS LinguistTools REQUIRED)
set(QT_LRELEASE_EXECUTABLE ${QT_VERSION_BASE}::lrelease)

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

# make sure the output directory exists
file(MAKE_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/i18n)


set(TS_FILES qgis_ar.ts qgis_az.ts qgis_bg.ts qgis_bs.ts qgis_ca.ts qgis_cs.ts qgis_da.ts qgis_de.ts qgis_en_US.ts qgis_es.ts qgis_et.ts qgis_eu.ts qgis_fi.ts qgis_fr.ts qgis_gl.ts qgis_hu.ts qgis_is.ts qgis_it.ts qgis_ja.ts qgis_ko.ts qgis_lt.ts qgis_lv.ts qgis_nb.ts qgis_nl.ts qgis_pl.ts qgis_pt_BR.ts qgis_pt_PT.ts qgis_ro.ts qgis_ru.ts qgis_sc.ts qgis_sk.ts qgis_sv.ts qgis_tr.ts qgis_uk.ts qgis_vi.ts qgis_zh-Hans.ts qgis_zh-Hant.ts)

ADD_TRANSLATION_FILES (QM_FILES ${TS_FILES})

# TODO QGIS 4.0: remove configure and call directly script
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

# creating a custom target is needed to make the files build
# "ALL" means that it will be run by default
add_custom_target (translations ALL DEPENDS ${QM_FILES} ${MD_FILES})

# first compile sources, then compile translations
if (WITH_DESKTOP)
  add_dependencies (translations ${QGIS_APP_NAME})
endif()

install (FILES ${QM_FILES}
         DESTINATION ${QGIS_DATA_DIR}/i18n)
