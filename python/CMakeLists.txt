#[[
杨小兵-2024-04-01

一、解释
  这部分内容是使用CMake配置项目时用到的，主要用于设置项目中生成的可执行文件、库文件等的输出目录。CMake是一个跨平台的自动化构建系统，它使用CMakeLists.txt文件
来定义构建过程。下面我将分别解释这些命令的应用场景、数学原理背后的解决方案，以及如何从计算机实现的角度解决这一问题。


二、详细解释

1. **`file(MAKE_DIRECTORY ${QGIS_PYTHON_OUTPUT_DIRECTORY})`**
   - **功能**：创建一个目录，如果目录已经存在，则不会进行任何操作。
   - **应用场景**：在项目构建之前，确保所有的输出文件都有一个统一的存放位置。例如，如果你的项目需要将Python绑定或者插件输出到特定的目录，这个命令可以用来创建这
  个目录。

2. **`set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${QGIS_PYTHON_OUTPUT_DIRECTORY})`**
   - **功能**：设置可执行文件的输出目录。
   - **应用场景**：当你的项目中包含了可执行文件（例如，应用程序、工具等）时，这个命令用来指定这些可执行文件应该被输出到的目录。这有助于将生成的文件组织在项目的
  特定部分，便于管理和使用。

3. **`set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${QGIS_PYTHON_OUTPUT_DIRECTORY})`**
   - **功能**：设置库文件的输出目录。
   - **应用场景**：对于包含动态或静态库的项目，这个命令指定库文件的输出目录。这同样有助于文件的组织和管理，特别是在开发需要被多个项目或模块引用的共享库时。

  在这三个命令中，`${QGIS_PYTHON_OUTPUT_DIRECTORY}`是一个变量，它应该在之前被定义，指向了希望将文件输出到的目录路径。这种方式使得管理路径变得灵活，易于在不同的
环境或项目配置中进行调整。
二、总结
1、QGIS_PYTHON_OUTPUT_DIRECTORY这个变量是在顶层CMakeLists.txt中定义的（set (QGIS_PYTHON_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIRECTORY}/qgis)）
2、其中file命令是为了创建一个目录，如果存在那么则不进行任何的处理，反之则创建需要的文件
]]
file (MAKE_DIRECTORY ${QGIS_PYTHON_OUTPUT_DIRECTORY})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${QGIS_PYTHON_OUTPUT_DIRECTORY})
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${QGIS_PYTHON_OUTPUT_DIRECTORY})


#[[
杨小兵-2024-04-01

一、解释
  在使用CMake进行项目构建时，如何管理和部署Python插件及其支持包到一个指定的目录（`PYTHON_OUTPUT_DIRECTORY`），以确保当应用程序从构建目录运行时插件能够正常工
作。

二、应用场景
  在开发需要支持Python插件的应用程序，比如QGIS，时，通常需要一种机制来自动化地管理插件的部署。这个过程包括将插件文件复制到特定目录、编译Python代码到字节码等。
这样做的目的是确保开发者或者用户在构建目录中运行应用程序时，所有插件都是可用的，并且是最新的。

三、计算机实现的角度

1. **`PYTHON_OUTPUT_DIRECTORY`的设置**
   这个目录用于存放所有Python插件及其依赖，使得在应用程序运行时能够正常加载这些插件。这个目录应该在CMakeLists.txt中被指定，以便于所有相关的构建命令都能使用它。

2. **Make Targets的使用**
   - **`staged-plugins`**
     - **功能**：将插件复制到`PYTHON_OUTPUT_DIRECTORY`。这通常在从仓库拉取最新代码并构建项目后执行，以确保插件是最新的。
     - **应用场景**：开发过程中频繁更新插件代码时，使用这个目标可以快速部署插件到测试环境。
   
   - **`staged-plugins-pyc`**
     - **功能**：不仅复制插件到指定目录，还会将Python代码编译成字节码（.pyc文件）。这可以提高运行时的性能，并且减少源代码的暴露。
     - **应用场景**：为了提高应用程序的启动速度和运行效率，或者需要隐藏源代码时使用。
   
   - **`clean-staged-plugins`**
     - **功能**：移除之前部署的插件目录。这用于清理旧的插件，确保不会有任何冲突或遗留问题。
     - **应用场景**：在重新部署插件之前清理环境，或者当插件不再需要时使用。

3. **`add_custom_target`的使用**
   `add_custom_target`命令用于添加一个自定义目标，这里是`staged-plugins`，它不会自动执行，需要手动触发。这种方式提供了灵活性，让开发者可以在适当的时候选择执
  行哪些操作。

二、总结
  这段注释和相关的CMake配置提供了一种灵活高效的方式来管理Python插件的部署过程。通过定义专门的make目标，它使得开发和测试过程中插件的更新和维护变得简单，确保了
应用程序在开发阶段能够顺利地运行和测试插件。这些机制虽然主要是围绕构建和部署过程，但背后反映了项目管理和自动化的重要性，以及如何通过工具化来提高开发效率和质量。
]]
# Python plugins and support packages can be staged to PYTHON_OUTPUT_DIRECTORY
# so plugins will function when app is run from build directory

# When staging all plugins, use the following make targets:
#   staged-plugins - stage plugins (usually after repo pull/build and project make)
#   staged-plugins-pyc - stage and byte-compile all
#   clean-staged-plugins - removes the staged plugins' directories
#
# NOTE: regular project 'make install' is unaffected

# Other target dependencies will be added, per staged resource
add_custom_target(staged-plugins)



# Plugins can also be staged with CMake option at build time
if(WITH_STAGED_PLUGINS)
  add_custom_target(staged-plugins-on-build ALL DEPENDS staged-plugins)
endif()

# Non-default/non-option-controlled target to stage and compile plugins and extras
add_custom_target(staged-plugins-pyc DEPENDS staged-plugins
  COMMAND ${Python_EXECUTABLE} -m compileall -q "${PYTHON_OUTPUT_DIRECTORY}"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Byte-compiling build output/python directory..."
)

# Other custom commands will be added, per staged resource
add_custom_target(clean-staged-plugins
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${PYTHON_OUTPUT_DIRECTORY}/plugins"
)

# Macro to byte-compile a target's staged Python resource(s)
macro(PY_COMPILE TARGET_NAME RESOURCE_PATHS)
  if(WITH_PY_COMPILE)
    add_custom_command(TARGET ${TARGET_NAME}
      POST_BUILD
      COMMAND ${Python_EXECUTABLE} -m compileall -q ${RESOURCE_PATHS}
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
      COMMENT "Byte-compiling staged resource..."
    )
  endif()
endmacro(PY_COMPILE)

# Macro to auto migrate resources
macro(PY_2TO3 TARGET_NAME RESOURCE_PATHS)
  add_custom_command(TARGET ${TARGET_NAME}
    POST_BUILD
    COMMAND "${Python_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/2to3" --no-diffs -w ${RESOURCE_PATHS}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Porting ${RESOURCE_PATHS} to Python 3 and Qt5"
  )
endmacro(PY_2TO3)

add_subdirectory(plugins)
if (WITH_GUI)
  add_subdirectory(qsci_apis)
  add_subdirectory(console)
  add_subdirectory(pyplugin_installer)
endif()
add_subdirectory(PyQt)
add_subdirectory(ext-libs)
add_subdirectory(testing)
add_subdirectory(processing)

include_directories(SYSTEM
  ${Python_INCLUDE_DIRS}
  ${SIP_INCLUDE_DIR}
  ${QT_QTCORE_INCLUDE_DIR}
  ${QT_QTGUI_INCLUDE_DIR}
  ${QT_QTNETWORK_INCLUDE_DIR}
  ${QT_QTSVG_INCLUDE_DIR}
  ${QT_QTXML_INCLUDE_DIR}
  ${GDAL_INCLUDE_DIR}
  ${GEOS_INCLUDE_DIR}
  ${QWT_INCLUDE_DIR}
  ${QCA_INCLUDE_DIR}
  ${QTKEYCHAIN_INCLUDE_DIR}
  ${SQLITE3_INCLUDE_DIR}
  ${SPATIALINDEX_INCLUDE_DIR}
)

if (${SIP_VERSION_STR} VERSION_LESS 5)
  include_directories(SYSTEM
    ${SIP_INCLUDE_DIR}
  )
endif()

if (WITH_GUI)
  include_directories(SYSTEM
    ${QSCINTILLA_INCLUDE_DIR}
  )
endif()

include_directories(
  ${CMAKE_SOURCE_DIR}/src/plugins

  ${CMAKE_BINARY_DIR}  # qgsconfig.h, qgsversion.h
  ${CMAKE_BINARY_DIR}/src/ui
  ${CMAKE_BINARY_DIR}/src/analysis
)

if (WITH_GUI)
  include_directories(

  )
endif()

if(NOT ENABLE_TESTS)
  set(SIP_DISABLE_FEATURES ${SIP_DISABLE_FEATURES} TESTS)
endif()

if(NOT ANDROID)
  set(SIP_DISABLE_FEATURES ${SIP_DISABLE_FEATURES} ANDROID)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
else()
  set(SIP_DISABLE_FEATURES ${SIP_DISABLE_FEATURES} ARM)
endif()

# SIP 4.19.7+ can:
# * prepend auto-generated Python signature to existing Docstrings
# * document template based classes
set(DOCSTRINGSTEMPLATE "//")
if(${SIP_VERSION_STR} VERSION_GREATER 4.19.6)
  set(DEFAULTDOCSTRINGSIGNATURE "%DefaultDocstringSignature \"prepended\"")
  set(DOCSTRINGSTEMPLATE "")
endif()

if(${SIP_VERSION_STR} VERSION_GREATER 4.19.0)
  set(SIP_FINAL "final")
else()
  set(SIP_FINAL "")
endif()

set(QGIS_PYTHON_DIR ${Python_SITEARCH}/qgis)

# core module
file(GLOB_RECURSE sip_files_core core/*.sip core/*.sip.in)
set(SIP_EXTRA_FILES_DEPEND ${sip_files_core})
set(SIP_EXTRA_OPTIONS ${PYQT_SIP_FLAGS} -g -o -a ${CMAKE_BINARY_DIR}/python/qgis.core.api)
if((${SIP_VERSION_STR} VERSION_EQUAL 4.19.11) OR (${SIP_VERSION_STR} VERSION_GREATER 4.19.11))
  set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -n ${PYQT5_SIP_IMPORT})
endif()
if(SIP_BUILD_EXECUTABLE)
  install(FILES ${CMAKE_BINARY_DIR}/python/core/build/_core/_core.pyi DESTINATION ${QGIS_PYTHON_DIR})
elseif((${SIP_VERSION_STR} VERSION_EQUAL 4.18) OR (${SIP_VERSION_STR} VERSION_GREATER 4.18))
  set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -y ${QGIS_PYTHON_OUTPUT_DIRECTORY}/_core.pyi)
  install(FILES ${QGIS_PYTHON_OUTPUT_DIRECTORY}/_core.pyi DESTINATION ${QGIS_PYTHON_DIR})
endif()

if((${PYQT5_VERSION_STR} VERSION_EQUAL 5.15) OR (${PYQT5_VERSION_STR} VERSION_GREATER 5.15))
  set(SIP_DISABLE_FEATURES ${SIP_DISABLE_FEATURES} VECTOR_MAPPED_TYPE)
endif()

if(NOT WITH_GUI)
  set(SIP_DISABLE_FEATURES ${SIP_DISABLE_FEATURES} HAVE_GUI)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/core/project.py.in ${CMAKE_CURRENT_BINARY_DIR}/core/project.py @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/core/pyproject.toml.in ${CMAKE_CURRENT_BINARY_DIR}/core/pyproject.toml @ONLY)
GENERATE_SIP_PYTHON_MODULE_CODE(qgis._core core/core.sip "${sip_files_core}" cpp_files)
BUILD_SIP_PYTHON_MODULE(qgis._core core/core.sip ${cpp_files} "" qgis_core)
set(SIP_CORE_CPP_FILES ${cpp_files})

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # Bad hack to fix compilation with gcc 11 - for some reason it's ignoring
  # the c++ standard version set for the target in BUILD_SIP_PYTHON_MODULE!
  add_definitions(-std=c++14)
endif()

if((UNIX OR MINGW) AND SIP_VERSION_STR VERSION_GREATER_EQUAL 4.10 AND SIP_VERSION_STR VERSION_LESS 5.0)
  set(SIP_EXTRA_OPTIONS -P ${SIP_EXTRA_OPTIONS})
  add_definitions(-Dprotected=public)
elseif((UNIX OR MINGW) AND SIP_VERSION_STR VERSION_GREATER_EQUAL 5.0)
  add_definitions(-Dprotected=public)
endif()

set (PY_MODULES core)

if(WITH_ANALYSIS)
  set(PY_MODULES ${PY_MODULES} analysis)
endif()

# gui module
if (WITH_GUI)
  set(PY_MODULES ${PY_MODULES} gui)

  file(GLOB_RECURSE sip_files_gui gui/*.sip gui/*.sip.in)
  set(SIP_EXTRA_FILES_DEPEND ${sip_files_core} ${sip_files_gui})
  set(SIP_EXTRA_OPTIONS ${PYQT_SIP_FLAGS} -g -o -a ${CMAKE_BINARY_DIR}/python/qgis.gui.api)
  if((${SIP_VERSION_STR} VERSION_EQUAL 4.19.11) OR (${SIP_VERSION_STR} VERSION_GREATER 4.19.11))
    set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -n ${PYQT5_SIP_IMPORT})
  endif()
  if(SIP_BUILD_EXECUTABLE)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gui/build/_gui/_gui.pyi DESTINATION ${QGIS_PYTHON_DIR})
  elseif((${SIP_VERSION_STR} VERSION_EQUAL 4.18) OR (${SIP_VERSION_STR} VERSION_GREATER 4.18))
    set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -y ${QGIS_PYTHON_OUTPUT_DIRECTORY}/_gui.pyi)
    install(FILES ${QGIS_PYTHON_OUTPUT_DIRECTORY}/_gui.pyi DESTINATION ${QGIS_PYTHON_DIR})
  endif()
  if(QSCI_SIP_DIR)
    set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -I ${QSCI_SIP_DIR})
  else()
    message(STATUS "Qsci sip file not found - disabling bindings for derived classes")
    set(SIP_DISABLE_FEATURES ${SIP_DISABLE_FEATURES} HAVE_QSCI_SIP)
  endif()

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gui/project.py.in ${CMAKE_CURRENT_BINARY_DIR}/gui/project.py @ONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gui/pyproject.toml.in ${CMAKE_CURRENT_BINARY_DIR}/gui/pyproject.toml @ONLY)
  GENERATE_SIP_PYTHON_MODULE_CODE(qgis._gui gui/gui.sip "${sip_files_gui}" cpp_files)
  BUILD_SIP_PYTHON_MODULE(qgis._gui gui/gui.sip ${cpp_files} "" qgis_core qgis_gui)
endif()

# 3D module
if (WITH_3D)
  include_directories(
    ${QT5_3DEXTRA_INCLUDE_DIR}
  )
  set(PY_MODULES ${PY_MODULES} 3d)

  file(GLOB_RECURSE sip_files_3d 3d/*.sip 3d/*.sip.in)
  set(SIP_EXTRA_FILES_DEPEND ${sip_files_core} ${sip_files_3d})
  set(SIP_EXTRA_OPTIONS ${PYQT_SIP_FLAGS} -g -o -a ${CMAKE_BINARY_DIR}/python/qgis.qgis3d.api)
  if((${SIP_VERSION_STR} VERSION_EQUAL 4.19.11) OR (${SIP_VERSION_STR} VERSION_GREATER 4.19.11))
    set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -n ${PYQT5_SIP_IMPORT})
  endif()
  if(SIP_BUILD_EXECUTABLE)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/3d/build/_3d/_3d.pyi DESTINATION ${QGIS_PYTHON_DIR})
  elseif((${SIP_VERSION_STR} VERSION_EQUAL 4.18) OR (${SIP_VERSION_STR} VERSION_GREATER 4.18))
    set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -y ${QGIS_PYTHON_OUTPUT_DIRECTORY}/_qgis3d.pyi)
    install(FILES ${QGIS_PYTHON_OUTPUT_DIRECTORY}/_qgis3d.pyi DESTINATION ${QGIS_PYTHON_DIR})
  endif()

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/3d/project.py.in ${CMAKE_CURRENT_BINARY_DIR}/3d/project.py @ONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/3d/pyproject.toml.in ${CMAKE_CURRENT_BINARY_DIR}/3d/pyproject.toml @ONLY)
  GENERATE_SIP_PYTHON_MODULE_CODE(qgis._3d 3d/3d.sip "${sip_files_3d}" cpp_files)
  BUILD_SIP_PYTHON_MODULE(qgis._3d 3d/3d.sip ${cpp_files} "" qgis_core qgis_3d)
endif()

# server module
if (WITH_SERVER AND WITH_SERVER_PLUGINS)
  include_directories(
    ../src/server
   ${CMAKE_BINARY_DIR}/src/server
  )

  set(PY_MODULES ${PY_MODULES} server)

  file(GLOB_RECURSE sip_files_server server/*.sip server/*.sip.in)
  set(SIP_EXTRA_FILES_DEPEND ${sip_files_core} ${sip_files_server})
  set(SIP_EXTRA_OPTIONS ${PYQT_SIP_FLAGS} -g -o -a ${CMAKE_BINARY_DIR}/python/qgis.server.api)
  if((${SIP_VERSION_STR} VERSION_EQUAL 4.19.11) OR (${SIP_VERSION_STR} VERSION_GREATER 4.19.11))
    set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -n ${PYQT5_SIP_IMPORT})
  endif()
  if(SIP_BUILD_EXECUTABLE)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/server/build/_server/_server.pyi DESTINATION ${QGIS_PYTHON_DIR})
  elseif((${SIP_VERSION_STR} VERSION_EQUAL 4.18) OR (${SIP_VERSION_STR} VERSION_GREATER 4.18))
    set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -y ${QGIS_PYTHON_OUTPUT_DIRECTORY}/_server.pyi)
    install(FILES ${QGIS_PYTHON_OUTPUT_DIRECTORY}/_server.pyi DESTINATION ${QGIS_PYTHON_DIR})
  endif()

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/server/project.py.in ${CMAKE_CURRENT_BINARY_DIR}/server/project.py @ONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/server/pyproject.toml.in ${CMAKE_CURRENT_BINARY_DIR}/server/pyproject.toml @ONLY)
  GENERATE_SIP_PYTHON_MODULE_CODE(qgis._server server/server.sip "${sip_files_server}" cpp_files)
  BUILD_SIP_PYTHON_MODULE(qgis._server server/server.sip ${cpp_files} "" qgis_core qgis_server)
endif()

# additional analysis includes
if(WITH_ANALYSIS)
  include_directories(BEFORE
    ${CMAKE_BINARY_DIR}/src/analysis/processing
    ${CMAKE_BINARY_DIR}/src/analysis/georeferencing
    ${CMAKE_BINARY_DIR}/src/analysis/vector
    ${CMAKE_BINARY_DIR}/src/analysis/mesh
    ${CMAKE_BINARY_DIR}/src/analysis/raster
    ${CMAKE_BINARY_DIR}/src/analysis/network
    ${CMAKE_BINARY_DIR}/src/analysis/interpolation
  )

  # analysis module
  file(GLOB_RECURSE sip_files_analysis analysis/*.sip analysis/*.sip.in)

  set(SIP_EXTRA_FILES_DEPEND ${sip_files_core} ${sip_files_analysis})
  set(SIP_EXTRA_OPTIONS ${PYQT_SIP_FLAGS} -g -o -a ${CMAKE_BINARY_DIR}/python/qgis.analysis.api)
  if((${SIP_VERSION_STR} VERSION_EQUAL 4.19.11) OR (${SIP_VERSION_STR} VERSION_GREATER 4.19.11))
    set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -n ${PYQT5_SIP_IMPORT})
  endif()
  if(SIP_BUILD_EXECUTABLE)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/analysis/build/_analysis/_analysis.pyi DESTINATION ${QGIS_PYTHON_DIR})
  elseif((${SIP_VERSION_STR} VERSION_EQUAL 4.18) OR (${SIP_VERSION_STR} VERSION_GREATER 4.18))
    set(SIP_EXTRA_OPTIONS ${SIP_EXTRA_OPTIONS} -y ${QGIS_PYTHON_OUTPUT_DIRECTORY}/_analysis.pyi)
    install(FILES ${QGIS_PYTHON_OUTPUT_DIRECTORY}/_analysis.pyi DESTINATION ${QGIS_PYTHON_DIR})
  endif()

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/analysis/project.py.in ${CMAKE_CURRENT_BINARY_DIR}/analysis/project.py @ONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/analysis/pyproject.toml.in ${CMAKE_CURRENT_BINARY_DIR}/analysis/pyproject.toml @ONLY)
  GENERATE_SIP_PYTHON_MODULE_CODE(qgis._analysis analysis/analysis.sip "${sip_files_analysis}" cpp_files)
  BUILD_SIP_PYTHON_MODULE(qgis._analysis analysis/analysis.sip ${cpp_files} "" qgis_core qgis_analysis)
endif()

if(WITH_QSCIAPI)
  # wait until after python module builds for api files to be available
  set(QGIS_PYTHON_API_FILE "${CMAKE_BINARY_DIR}/python/qsci_apis/PyQGIS.api")

  add_custom_target(qsci-api ALL
    DEPENDS python_module_qgis__gui python_module_qgis__core python_module_qgis__analysis)

  # run update/concatenate command
  add_custom_command(TARGET qsci-api
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_SOURCE_DIR}/cmake/QsciAPI.cmake"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Generating pyqgis api file" VERBATIM)

  install(FILES ${QGIS_PYTHON_API_FILE} DESTINATION "${QGIS_DATA_DIR}/python/qsci_apis")

  # create target for generating console auto-completion *.pap binary file
  # takes too long to build (> 1 minute) for targets to have ALL property
  set(APIS_SRC_DIR "${CMAKE_SOURCE_DIR}/python/qsci_apis")
  set(APIS_BIN_DIR "${CMAKE_BINARY_DIR}/python/qsci_apis")

  # generate a .pap file to be immediately installed in QGIS source tree (the default .pap)
  add_custom_target(qsci-pap-src
    DEPENDS qsci-api ${QGIS_PYTHON_API_FILE})

  set(PAP_NAME "pyqgis.pap")
  add_custom_command(TARGET qsci-pap-src
    POST_BUILD
    COMMAND ${Python_EXECUTABLE} "${APIS_SRC_DIR}/generate_console_pap.py" "${APIS_SRC_DIR}/${PAP_NAME}" "${APIS_SRC_DIR}" "${APIS_BIN_DIR}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Generating ${PAP_NAME} for console auto-completion (MAY TAKE > 1 MINUTE!)" VERBATIM)
endif()

if(WITH_CUSTOM_WIDGETS)
  install(FILES custom_widgets/qgis_customwidgets.py DESTINATION "${PYUIC_WIDGET_PLUGIN_DIRECTORY}")
endif()

# Plugin utilities files to copy to staging or install
set(PY_FILES
  __init__.py
  utils.py
  user.py
)

add_custom_target(pyutils ALL)
install(FILES ${PY_FILES} DESTINATION "${QGIS_PYTHON_DIR}")

# stage to output to make available when QGIS is run from build directory
foreach(pyfile ${PY_FILES})
  add_custom_command(TARGET pyutils
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${QGIS_PYTHON_OUTPUT_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E copy ${pyfile} "${QGIS_PYTHON_OUTPUT_DIRECTORY}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${pyfile}
  )
  PY_COMPILE(pyutils "${QGIS_PYTHON_OUTPUT_DIRECTORY}/${pyfile}")
endforeach(pyfile)

foreach(module ${PY_MODULES})
  add_custom_target(py${module} ALL)
  add_dependencies(py${module} python_module_qgis__${module})

  # concat auto_additions/*.py in _module_/__init__.py
  file(GLOB PY_FILES_AUTO_ADDITIONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${module}/auto_additions/*.py)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${module}/__init__.py.in ${QGIS_PYTHON_OUTPUT_DIRECTORY}/${module}/__init__.py COPYONLY)
  foreach(pyfile ${PY_FILES_AUTO_ADDITIONS})
    file(READ ${pyfile} CONTENTS)
    file(APPEND ${QGIS_PYTHON_OUTPUT_DIRECTORY}/${module}/__init__.py "${CONTENTS}")
  endforeach(pyfile)
  install(FILES ${QGIS_PYTHON_OUTPUT_DIRECTORY}/${module}/__init__.py DESTINATION "${QGIS_PYTHON_DIR}/${module}")

  file(GLOB PY_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${module}/*.py)
  install(FILES ${PY_FILES} DESTINATION "${QGIS_PYTHON_DIR}/${module}")
  file(GLOB PY_FILES_ADDITIONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${module}/additions/*.py)
  install(FILES ${PY_FILES_ADDITIONS} DESTINATION "${QGIS_PYTHON_DIR}/${module}/additions")
  set(PY_FILES ${PY_FILES} ${PY_FILES_ADDITIONS})
  foreach(pyfile ${PY_FILES})
    get_filename_component(subdir ${pyfile} DIRECTORY)
    add_custom_command(TARGET py${module}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${QGIS_PYTHON_OUTPUT_DIRECTORY}/${subdir}"
      COMMAND ${CMAKE_COMMAND} -E copy ${pyfile} "${QGIS_PYTHON_OUTPUT_DIRECTORY}/${subdir}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS ${PY_FILES}
    )
  endforeach(pyfile)
  PY_COMPILE(py${module} "${QGIS_PYTHON_OUTPUT_DIRECTORY}/${module}")

  # install source sip files
  if(SIP_GLOBAL_INSTALL)
    file(GLOB sip_files ${CMAKE_CURRENT_BINARY_DIR}/${module}/*.sip)
    install(FILES ${sip_files} DESTINATION ${SIP_DEFAULT_SIP_DIR}/qgis/${module})
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${module}/auto_generated DESTINATION ${SIP_DEFAULT_SIP_DIR}/qgis/${module})
  endif()
endforeach(module)
