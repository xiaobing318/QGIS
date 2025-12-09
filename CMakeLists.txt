#############################################################
# CMake settings
#[[
1. 设置CMake的最小版本是3.22.0，假设使用构建的CMake版本低于指定版本那么CMakeLists.txt中使用到的feature将不会被正确识别和执行。
]]
cmake_minimum_required(VERSION 3.22.0)
#[[
1. 设置一个普通的布尔类型变量，将其赋值为ON，这里设置并不是一个缓存变量，如果是缓存变量的话是可以在cmake-gui中显示的并且可以直接通过命令行-D来进行使用。
2. 设置这个变量是为了能够启动CMake中内置的一个特性即在日志中采用有颜色的输出日志信息，这样能够给使用者带来更好的开发体验。
]]
set(CMAKE_COLOR_MAKEFILE ON)
#[[
1. 设置一个普通的布尔类型变量，将其赋值为ON，这里设置并不是一个缓存变量，如果是缓存变量的话是可以在cmake-gui中显示的并且可以直接通过命令行-D来进行使用。
2. 将这个变量设置为ON是为了让CMake能够在构建项目的时候自动对*.qrc文件进行处理。
]]
set(CMAKE_AUTORCC ON)
# set path to additional CMake modules
#[[
1. 对于CMake来说其默认有对应的模块搜索路径，例如C:\Program Files\CMake\share\cmake-3.28\Modules就是安装CMake的时候默认设置的模块搜索目录路径。
2. 这里添加了QGIS项目中自定义的CMake模块搜索目录路径从而使得CMake可以找到模块的具体实现，因为在顶层CMakeLists.txt以及其它的CMakeLists.txt文件中使用到了自定义CMake模块中的一些功能，这一点同Python、C、C++等等编程语言都是类似的，核心在于将功能进行模块化。
]]
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
# POLICIES
#[[
1. POLICY是为了 CMake 在构建项目的时候能够兼容旧项目而设置的一个开关从而用来规定某个行为到底按旧规则还是新规则执行，这样可以避免升级后出现问题。
2. 在构建、编译QGIS的时候如果遇到了设置POLICY的提示，可以通过类似这种方式消除警告，根本的方式要从源头解决问题。
]]
if("${CMAKE_VERSION}" VERSION_GREATER_EQUAL "3.27")
  # include(Dart) still used, as is the "Experimental" target
  cmake_policy(SET CMP0145 OLD)
endif()
if("${CMAKE_VERSION}" VERSION_GREATER_EQUAL "3.31")
  cmake_policy(SET CMP0177 NEW)
endif()

# don't relink it only the shared object changes
#[[
1. QGIS项目中依赖大量的第三方库，这些第三方库又依赖其它的第三方库，例如QT、GDAL、PROJ、GEOS等等，这些第三方库被使用的方式绝大多数是通过动态链接库的方式。QGIS在这些第三方库的基础上构建便于使用的动态链接库、静态库、可执行文件等等，在构建、编译QGIS的时候一般是不需要对第三方库进行单独编译的，也就是说直接使用第三方库的成果来编译、链接自己便于使用的动态链接库、静态库、可执行文件。在这个过程中CMake通过在项目文件中添加命令来控制是否需要对动态链接库进行重新链接，触发的条件就是动态链接库文件的时间戳是否发生了变化，默认情况是只要动态链接库文件的时间戳发生了变化那么在编译其它目标对象的时候重新链接时间戳发生变化的动态链接库。
2. 反例：如果被依赖的动态链接库文件的接口经常发生变化并且打开这个设置，那么在编译阶段一般是不会出现问题，但是在运行时则会出现找不到符号的提示，这是需要注意的，如果公开接口不发生改变，其内部实现发生变化，那么是不会出现这种问题的。
]]
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

#[[
1. 创建一个布尔类型的开关以供开发者控制是否需要在QGIS中构建Python支持，如果该开关关闭的话那么将会禁用processing。
2. 该变量是一个缓存变量，可以在CMakeCache.txt和CMake-GUI中显示。
]]
option (WITH_PYTHON "Determines whether Python support should be built (disabling it will in particular disable processing)" ON)
#[[
1. 创建一个布尔类型的缓存变量以供开发者控制是否在QGIS项目中提供Python绑定功能。
2. 该变量被设置为一个缓存变量，可以在CMakeCache.txt和CMake-GUI中显示。
]]
set(WITH_BINDINGS ${WITH_PYTHON} CACHE BOOL "Determines whether Python bindings should be built")

#[[
1. 创建布尔类型的缓存变量以供开发者控制是否在QGIS项目中提供对应的功能。
2. 这些变量被设置为一个缓存变量，可以在CMakeCache.txt和CMake-GUI中显示。
]]
set (WITH_3D TRUE CACHE BOOL "Determines whether QGIS 3D library should be built")
set (WITH_QGIS_PROCESS TRUE CACHE BOOL "Determines whether the standalone \"qgis_process\" tool should be built")
set (WITH_DESKTOP TRUE CACHE BOOL "Determines whether QGIS desktop should be built")
set (WITH_GUI TRUE CACHE BOOL "Determines whether QGIS GUI library should be built")
set(WITH_VCPKG FALSE CACHE BOOL "Use the vcpkg submodule for dependency management.")

#[[
1. 创建一个缓存变量用来保存VCPKG SDK的路径，如果选择使用vcpkg构建QGIS项目，那么需要提供VCPKG SDK的路径，默认是空路径，需要手动添加。
2. 目前还没有通过这种方式构建QGIS。
]]
set(SDK_PATH "" CACHE STRING "Build with VCPKG SDK")

#[[
1. 根据不同的情况分别给出提示信息以及对应的设置：
  1.1 方式一：直接使用VCPKG编译的QGIS所依赖的第三方库成果。
  1.2 方式二：本地构建编译QGIS所依赖的第三方库。
  1.3 方式三：不通过VCPKG的方式而是使用传统的方式构建QGIS，这是目前我走通的构建方式。
]]
if(NOT SDK_PATH STREQUAL "")
  #[[
  1. 输出使用VCPKG SDK构建QGIS项目的提示信息。
  2. 在CMake中可以使用${VARIABLE}使用之前创建的变量。
  ]]
  message(STATUS "Building with SDK -- ${SDK_PATH}")
  #[[
  1. 创建两个普通变量（非缓存变量）用来保存CMake工具链文件路径和SDK安装路径。
  ]]
  set(CMAKE_TOOLCHAIN_FILE "${SDK_PATH}/scripts/buildsystems/vcpkg.cmake")
  set(VCPKG_INSTALL_PREFIX "${SDK_PATH}/installed")
  #[[
  1. 如果构建的目标平台是macos并且对应的变量没有被设置，则给出错误信息提示。
  ]]
  if(APPLE AND NOT VCPKG_TARGET_TRIPLET)
    message(FATAL_ERROR "VCPKG_TARGET_TRIPLET not set (set it to arm64-osx-dynamic-release or x64-osx-dynamic-release")
  endif()
  #[[
  1. 将之前设置的缓存变量从FALSE修改为ON，说明使用VCPKG的方式构建编译QGIS。
  ]]
  set(WITH_VCPKG ON)
elseif(WITH_VCPKG)
  #[[
  1. 输出“使用VCPKG构建本地依赖”信息用来强调使用VCPKG来构建第三方库依赖而不是直接使用第三方库编译成果。
  ]]
  message(STATUS "Building local dependencies with VCPKG --")
  set(VCPKG_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/vcpkg_installed")
  include(VcpkgToolchain)
else()
  message(STATUS "Building with system libraries --")
endif()


#[[
1. 如果选择使用 VCPKG 的方式即本地构建 QGIS 依赖以及 QGIS 项目则根据不同平台设置配置。
2. CMAKE_PROGRAM_PATH 是 CMake 的一个路径搜索列表变量，会被 find_program() 使用，CMake 在查找可执行文件的时候会在这个变量罗列的目录路径中查找。
]]
if(WITH_VCPKG)
  if(WIN32)
    list(APPEND CMAKE_PROGRAM_PATH "${VCPKG_INSTALL_PREFIX}/${VCPKG_TARGET_TRIPLET}/tools/python3/Scripts/")
  else()
    list(APPEND CMAKE_PROGRAM_PATH "${VCPKG_INSTALL_PREFIX}/${VCPKG_TARGET_TRIPLET}/bin")
  endif()
  set(PREFER_INTERNAL_LIBS FALSE)
else()
  set(PREFER_INTERNAL_LIBS TRUE)
endif()

#############################################################
# Project and version
#[[
1. 创建五个普通变量用来保存字符串版本信息。
]]
set(CPACK_PACKAGE_VERSION_MAJOR "3")
set(CPACK_PACKAGE_VERSION_MINOR "44")
set(CPACK_PACKAGE_VERSION_PATCH "3")
set(COMPLETE_VERSION ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})
set(RELEASE_NAME "Solothurn")
#[[
1. 设置项目名称以及版本信息。
]]
project(qgis VERSION ${COMPLETE_VERSION})

#[[
1. 根据不同的构建、编译平台设置 QGIS 项目的名称信息：
  1.1 如果构建、编译的平台是 MacOS 则将 QGIS 启动程序的名称设置为 QGIS 。
  1.2 如果构建、编译的平台是 MacOS 则将 QGIS 启动程序的名称设置为 qgis 。
]]
if (APPLE)
  set(QGIS_APP_NAME "QGIS")
else()
  set(QGIS_APP_NAME "qgis")
endif()

# Note the version no is Mmmpp for Major/minor/patch, 0-padded, thus '10100' for 1.1.0
#[[
1. 使用 CMake 中数学工具计算 QGIS 版本信息。
2. QGIS 版本采用 Mmmpp 的格式，这个我命名文件夹的方式是一致的。
]]
math(EXPR QGIS_VERSION_INT "${CPACK_PACKAGE_VERSION_MAJOR}*10000+${CPACK_PACKAGE_VERSION_MINOR}*100+${CPACK_PACKAGE_VERSION_PATCH}")
message(STATUS "QGIS version: ${COMPLETE_VERSION} ${RELEASE_NAME} (${QGIS_VERSION_INT})")

#[[
1. 创建缓存变量以供开发者控制是否禁用某些和发布相关的步骤从而来加快在开发中的编译时间。
]]
set (ENABLE_LOCAL_BUILD_SHORTCUTS FALSE CACHE BOOL "Disables some build steps which are only relevant for releases to speed up compilation time for development")

#############################################################
#[[
1. 如果构建、编译 QGIS 的平台是 MacOS ，那么进行一些平台相关的设置。
]]
if (APPLE)
  # QGIS custom dependencies package from qgis/QGIS-Mac-Packager
  # they can be downloaded from https://download.qgis.org/downloads/macos/deps/
  # and extracted to /opt/QGIS/qgis-deps-<deps-version>/stage
  set (QGIS_MAC_DEPS_DIR "" CACHE PATH "Path to QGIS Mac custom dependencies directory")

  # Setup LIB_DIR and CMAKE_PREFIX_PATH to help CMake's
  # find_packages to look for these libraries instead of system libraries
  if ( QGIS_MAC_DEPS_DIR )
    set(ENV{LIB_DIR} ${QGIS_MAC_DEPS_DIR})
    list(APPEND CMAKE_PREFIX_PATH ${QGIS_MAC_DEPS_DIR})
  endif()
endif()

#############################################################
# Configure OpenCL if available
#[[
1. 创建一个普通变量用来保存 OpenCL 是否可用的状态。
2. 如果构建、编译 QGIS 的平台是 BSD 则禁用 OpenCL 支持，反之则启用 OpenCL 支持。
]]
set(HAVE_OPENCL FALSE)
if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD$")
  option(USE_OPENCL "Use OpenCL" OFF)
else()
  option(USE_OPENCL "Use OpenCL" ON)
endif()
if (USE_OPENCL)
    #[[
    1. 使用 CMake 官方自带的模块配置 OpenCL 。
    2. CMake 可以自动找到自带的模块文件而不需要手动配置，如果是自定义的或者不是官方支持的模块文件则需要手动配置。
    ]]
    find_package(OpenCL)
    #[[
    1. 经过 CMake 官方自带的 OpenCL 模块配置，如果在目标平台上找到了 OpenCL 则会进行进一步的配置。
    ]]
    if(${OpenCL_FOUND})
        set(HAVE_OPENCL TRUE)
        # Fixup for standard FindOpenCL module not assigning proper framework headers directory
        #[[
        1. 官方的 OpenCL 在 MacOS 平台上有些配置是不正确的，需要手动修正。
        ]]
        if (APPLE AND "${OpenCL_INCLUDE_DIR}" MATCHES "OpenCL\\.framework/?$")
            set(OpenCL_INCLUDE_DIR "${OpenCL_INCLUDE_DIR}/Headers" CACHE PATH "" FORCE)
            set(OpenCL_INCLUDE_DIRS ${OpenCL_INCLUDE_DIR})
        endif()
        #[[
        1. 使用 CMake 官方的 OpenCLhpp 模块文件对 OpenCL 这个第三方库头文件进行配置。
        2、如果 CMake 官方的 OpenCLhpp 模块文件对 OpenCL 这个第三方库头文件没有配置成功则选择使用 QGIS 项目中自带的头文件配置，整体的逻辑是优先选择官方配置，如果官方配置失败则选择使用自带的头文件。
        ]]
        find_package(OpenCLhpp)
        if(NOT OPENCL_HPP_FOUND)
            # Use internal headers copied from OpenCL-CLHPP project
            set(OPENCL_HPP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/opencl-clhpp/include CACHE PATH "" FORCE)
            message(STATUS "Couldn't find OpenCL C++ headers, using internal: ${OPENCL_HPP_INCLUDE_DIR}")
        endif()
        set(OpenCL_INCLUDE_DIRS ${OpenCL_INCLUDE_DIRS} ${OPENCL_HPP_INCLUDE_DIR})
    else()
        message(STATUS "Couldn't find OpenCL: support DISABLED")
    endif()
endif()

# Configure CCache if available
#[[
1. 创建一个缓存变量用来保存 CCache 是否可以使用的状态，默认 CCache 是可以使用的。
2. CCache 是一种缓存技术，整体的效果就是可以加快编译。
]]
option(USE_CCACHE "Use ccache" ON)
if (USE_CCACHE)
  #[[
  1. 使用 find_program 命令来确认 ccache 可执行文件是否存在，如果存在的话则获取其路径。
  2. 如果 ccache 可执行文件是存在的则进行相应的配置，这部分不是重点。
  ]]
  find_program(CCACHE_EXE ccache)
  if(CCACHE_EXE)
    message(STATUS "ccache found")
    execute_process(COMMAND ${CCACHE_EXE} --help OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CCACHE_HELP)
    execute_process(COMMAND ${CCACHE_EXE} --get-config sloppiness OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CCACHE_SLOPPINESS)
    execute_process(COMMAND ${CCACHE_EXE} --get-config compiler_type OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CCACHE_COMPILER_TYPE)
    string(FIND "${CCACHE_SLOPPINESS}" "pch_defines" fpch_defines_found_index)
    string(FIND "${CCACHE_SLOPPINESS}" "time_macros" time_macros_found_index)
    string(FIND "${CCACHE_SLOPPINESS}" "include_file_mtime" include_file_mtime_found_index)
    string(FIND "${CCACHE_SLOPPINESS}" "include_file_ctime" include_file_ctime_found_index)
    # Detect if we have ccache >= 4.8 which accepts passing configuration settings when invoking the compiler
    string(FIND "${CCACHE_HELP}" "ccache [KEY=VALUE ...] compiler" ccache_key_value_found_index)
    if (fpch_defines_found_index EQUAL -1 OR time_macros_found_index EQUAL -1 OR
        (BUILD_WITH_QT6 AND (include_file_mtime_found_index EQUAL -1 OR include_file_ctime_found_index EQUAL -1)))
        set(CCACHE_SLOPPINESS_REQUIRED "pch_defines,time_macros")
        if (BUILD_WITH_QT6 AND (include_file_mtime_found_index EQUAL -1 OR include_file_ctime_found_index EQUAL -1))
          string(APPEND CCACHE_SLOPPINESS_REQUIRED ",include_file_mtime,include_file_ctime")
        endif()
    else()
        set(CCACHE_SLOPPINESS_REQUIRED "")
    endif()
    if (BUILD_WITH_QT6 AND CMAKE_CXX_COMPILER STREQUAL "/usr/bin/c++" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT "${CCACHE_COMPILER_TYPE}" STREQUAL "gcc")
        # Cf https://github.com/ccache/ccache/discussions/959
        set(CCACHE_COMPILER_TYPE_GCC_REQUIRED ON)
    else()
        set(CCACHE_COMPILER_TYPE_GCC_REQUIRED OFF)
    endif()
    set(CCACHE_INVOCATION_COMMAND "ccache")
    if (MSVC)
       # CCache doesn't work yet with precompiled headers (cf https://github.com/ccache/ccache/issues/1383)
       # so no need to set specific ccache configuration items
    elseif (ccache_key_value_found_index EQUAL -1 )
      if (CCACHE_SLOPPINESS_REQUIRED)
          message(FATAL_ERROR "The use of precompiled headers only works if the ccache 'sloppiness' settings contains 'pch_defines' and 'time_macros'. Consider running 'ccache --set-config sloppiness=${CCACHE_SLOPPINESS_REQUIRED}' to define them")
      endif()
      if (CCACHE_COMPILER_TYPE_GCC_REQUIRED)
          message(FATAL_ERROR "The use of precompiled headers only works properly with QT6 if the ccache 'compiler_type' settings is set to 'gcc'. Consider running 'ccache --set-config compiler_type=gcc'")
      endif()
    else()
      if (CCACHE_SLOPPINESS_REQUIRED)
        string(APPEND CCACHE_INVOCATION_COMMAND " sloppiness=${CCACHE_SLOPPINESS_REQUIRED}")
      endif()
      if (CCACHE_COMPILER_TYPE_GCC_REQUIRED)
        string(APPEND CCACHE_INVOCATION_COMMAND " compiler_type=gcc")
      endif()
    endif()
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_INVOCATION_COMMAND}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_INVOCATION_COMMAND}")
  endif(CCACHE_EXE)
endif(USE_CCACHE)

#[[
1. 如果想要在 MSVC 即 Windows 平台上使用 ccache 加快编译系统的速度则需要关闭“预编译头”这个特性，因为这个特性在 MSVC 环境中还没有完全支持。
]]
if (USE_CCACHE AND MSVC)
  # CCache doesn't work yet with precompiled headers (cf https://github.com/ccache/ccache/issues/1383)
  set(USE_PRECOMPILED_HEADERS OFF)
else()
  set(USE_PRECOMPILED_HEADERS ON)
endif()

#[[
1. 如果构建、编译 QGIS 的平台是 apple ios 则强制将动态链接库编译成静态库，这是因为在 apple ios 平台上采用的是静态库运行的方式而不是动态链接库的方式。
]]
if (IOS)
    set (DEFAULT_FORCE_STATIC_LIBS TRUE)
else()
    set (DEFAULT_FORCE_STATIC_LIBS FALSE)
endif()

#[[
1. 创建一个缓存变量用来保存是否应该将库仅编译为静态库，默认的设置则通过之前变量的值来确定。
]]
set (FORCE_STATIC_LIBS ${DEFAULT_FORCE_STATIC_LIBS} CACHE BOOL "Determines whether libraries should be static only")
#[[
1. 将该缓存变量设置成高级模式即不直接出现在 CMake-GUI 中，如果开发者想要修改该变量也是可以通过勾选高级模式来查看、修改该缓存变量的。
]]
mark_as_advanced(FORCE_STATIC_LIBS)
#[[
1. 如果想要将库仅编译成静态库，那么则将库的类型设置成静态库，否则设置成动态库的方式。
]]
if (FORCE_STATIC_LIBS)
  set (LIBRARY_TYPE STATIC)
  # following variable is used in qgsconfig.h
  set (HAVE_STATIC_PROVIDERS TRUE)
else()
  set (LIBRARY_TYPE SHARED)
  unset(HAVE_STATIC_PROVIDERS)
endif()

# in generated makefiles use relative paths so the project dir is moveable
# Note commented out since it cause problems but it would be nice to resolve these and enable
#
# issue is caused by include_directories(${CMAKE_BINARY_DIR}) near the end of this file generating incorrect path
#set (CMAKE_USE_RELATIVE_PATHS ON)

#[[
1. 创建一个缓存变量以供开发者控制是否应该构建 QGIS core ，默认情况是需要构建的，并且将这个变量设置为高级选项。
]]
set (WITH_CORE TRUE CACHE BOOL "Determines whether QGIS core should be built.")
mark_as_advanced(WITH_CORE)

#[[
1.
]]
if(WITH_CORE)
  # Try to configure and build GRASS plugin by default
  #[[
  1. 默认情况下尝试配置和构建 GRASS 插件，循环处理 GRASS 的不同版本。
  ]]
  foreach (GRASS_SEARCH_VERSION 7 8)
    # Legacy note:
    # For GRASS 6 there were used cached variables without version suffix so that existing caches didn't have to be reconfigured.
    # Cached variables were: WITH_GRASS, WITH_GRASS7, GRASS_PREFIX, GRASS_PREFIX7, GRASS_INCLUDE_DIR, GRASS_INCLUDE_DIR7
    # Everywhere else each variable has version major appended.
    # Normal variables were: GRASS_FOUND6, GRASS_FOUND7, GRASS_MAJOR_VERSION6, GRASS_MAJOR_VERSION7, etc.
    # In addition there is also GRASS_FOUND, which is TRUE if at least one version of GRASS was found
    #[[
    1. 创建普通变量用来记录 GRASS 插件版本。
    2、创建缓存变量用来记录指定 GRASS 插件版本是否应该被构建，默认都是构建的。
    ]]
    set (GRASS_CACHE_VERSION ${GRASS_SEARCH_VERSION})
    set (WITH_GRASS${GRASS_CACHE_VERSION} TRUE CACHE BOOL "Determines whether GRASS ${GRASS_SEARCH_VERSION} plugin should be built")
    #[[
    1. 如果 GRASS 执行版本插件需要被构建，那么使用 cmake 目录中的 FindGRASS.cmake 文件对 GRASS 进行配置。
    ]]
    if (WITH_GRASS${GRASS_CACHE_VERSION})
      find_package(GRASS ${GRASS_SEARCH_VERSION})
      #[[
      1. 设置缓存变量用来保存指定版本 GRASS 插件的根目录路径。
      ]]
      set (GRASS_PREFIX${GRASS_CACHE_VERSION} ${GRASS_PREFIX${GRASS_SEARCH_VERSION}} CACHE PATH "Path to GRASS ${GRASS_SEARCH_VERSION} base directory")
    endif()
  endforeach (GRASS_SEARCH_VERSION)


  #[[
  1. 创建缓存变量以供开发者控制是否应该构建 OAuth2 authentication method 插件的状态，默认是需要构建的。
  ]]
  set (WITH_OAUTH2_PLUGIN TRUE CACHE BOOL "Determines whether OAuth2 authentication method plugin should be built")
  if(WITH_OAUTH2_PLUGIN)
    set(HAVE_OAUTH2_PLUGIN TRUE)
  endif()

  set (WITH_AUTH TRUE CACHE BOOL "Determines whether QGIS authentication methods should be built")

  set (WITH_ANALYSIS TRUE CACHE BOOL "Determines whether QGIS analysis library should be built")

  #[[
  1. 如果需要构建 QGIS desktop 那么根据不同的平台设置 crash handler 的构建状态，并且进行一些配置。
  ]]
  if(WITH_DESKTOP)
      if((WIN32 AND NOT MINGW) OR (UNIX AND NOT APPLE AND NOT ANDROID AND NOT IOS))
	set (CRASH_HANDLER_AVAILABLE TRUE)
      else()
	set (CRASH_HANDLER_AVAILABLE FALSE)
      endif()

      set (WITH_CRASH_HANDLER ${CRASH_HANDLER_AVAILABLE} CACHE BOOL "Determines whether the QGIS crash handler application should be built")
      if(WITH_CRASH_HANDLER AND NOT CRASH_HANDLER_AVAILABLE)
	message(FATAL_ERROR "Crash handler cannot be built on this environment. Set WITH_CRASH_HANDLER to false.")
      endif()
      if(WITH_CRASH_HANDLER)
	set (HAVE_CRASH_HANDLER TRUE)    # used in qgsconfig.h
      else()
	set (HAVE_CRASH_HANDLER FALSE)    # used in qgsconfig.h
      endif()
  endif()

  set (WITH_QUICK FALSE CACHE BOOL "Determines whether QGIS Quick library should be built")

  set (NATIVE_CRSSYNC_BIN "" CACHE PATH "Path to a natively compiled synccrsdb binary. If set, crssync will not build but use provided bin instead.")
  mark_as_advanced (NATIVE_CRSSYNC_BIN)

  # try to configure and build python bindings by default
  #[[
  1. 默认将会配置和构建 Python 绑定。
  ]]
  if (WITH_BINDINGS)
    # By default bindings will be installed only to QGIS directory
    # Someone might want to install it to python site-packages directory
    # as otherwise user has to use PYTHONPATH environment variable to add
    # QGIS bindings to package search path
    #[[
    1. 默认情况下，绑定程序只会安装到 QGIS 目录。但有些人可能希望将其安装到 Python 的 site-packages 目录，否则用户必须使用 PYTHONPATH 环境变量将 QGIS 绑定程序添加到包搜索路径中。
    ]]
    set (BINDINGS_GLOBAL_INSTALL FALSE CACHE BOOL "Install bindings to global Python directory? (might need root)")
    #[[
    1. 创建一个缓存变量以供开发者控制是否应该将 sip source file 安装到系统的 sip 目录中，默认不会安装到系统目录中。
    ]]
    set (SIP_GLOBAL_INSTALL FALSE CACHE BOOL "Install sip source files to system sip directory? (might need root)")
    #[[
    1. 创建一个缓存变量以供开发者控制是否预先安装核心 Python 插件以便从构建目录运行？（实用程序和控制台始终预先安装）。
    ]]
    set (WITH_STAGED_PLUGINS TRUE CACHE BOOL "Stage-install core Python plugins to run from build directory? (utilities and console are always staged)")
    #[[
    1. 创建一个缓存变量以供开发者确定暂存位置或已安装位置中的 Python 模块是否应进行字节编译。
    ]]
    set (WITH_PY_COMPILE FALSE CACHE BOOL "Determines whether Python modules in staged or installed locations should be byte-compiled")
    # concatenate QScintilla2 API files
    if (WITH_GUI)
      #[[
      1. 创建一个缓存变量以供开发者确认是否生成 PyQGIS QScintilla2 API 文件。（对于开发人员）在 QGIS 构建和安装之间运行 make qsci-pap-src 以在源代码树中重新生成 .pap 文件，以便实现控制台自动补全，默认是需要生成 PyQGIS QScintilla2 API 文件的。
      ]]
      set (WITH_QSCIAPI TRUE CACHE BOOL "Whether to generate PyQGIS QScintilla2 API file. (For devs) run 'make qsci-pap-src' in between QGIS build and install to regenerate .pap file in source tree for console auto-completion.")
      # keep casual users from updating their source tree via WITH_QSCIAPI
      mark_as_advanced (WITH_QSCIAPI)
    endif()
  endif()

  # server disabled default because it needs FastCGI (which is optional dependency)
  #[[
  1. 创建一个缓存变量以供开发者确认 QGIS server 是否应该被构建，默认是不会进行构建的，因为这个功能依赖 FastCGI 。
  ]]
  set (WITH_SERVER FALSE CACHE BOOL "Determines whether QGIS server should be built")
  #[[
  1. 如果 QGIS server 需要被构建，那么进行相应的配置。
  ]]
  if(WITH_SERVER)
    set (WITH_SERVER_LANDINGPAGE_WEBAPP FALSE CACHE BOOL "Determines whether QGIS server landingpage webapp should be built (requires nodejs and yarn)")

    set (SERVER_SKIP_ECW FALSE CACHE BOOL "Determines whether QGIS server should disable ECW (ECW in server apps requires a special license)")

    set (WITH_SERVER_PLUGINS ${WITH_BINDINGS} CACHE BOOL "Determines whether QGIS server support for Python plugins should be built")
    if(WITH_SERVER_PLUGINS AND NOT WITH_BINDINGS)
      message(FATAL_ERROR "Server plugins are not supported without Python bindings. Enable WITH_BINDINGS or disable WITH_SERVER_PLUGINS")
    endif()
    if(WITH_SERVER_PLUGINS)
      set(HAVE_SERVER_PYTHON_PLUGINS TRUE)
    endif()
  endif()

  # Custom widgets
  set (WITH_CUSTOM_WIDGETS FALSE CACHE BOOL "Determines whether QGIS custom widgets for Qt Designer should be built")

  if (NOT WITH_GUI)
    set (HAVE_GUI FALSE)    # used in qgsconfig.h
    # force value of some options
    if(WITH_DESKTOP)
      message(FATAL_ERROR "Desktop cannot be built without gui. Enable WITH_GUI or disable WITH_DESKTOP.")
    endif()
    if(WITH_CUSTOM_WIDGETS)
      message(FATAL_ERROR "Custom widgets cannot be built without gui. Enable WITH_GUI or disable WITH_CUSTOM_WIDGETS.")
    endif()
  else()
    set (HAVE_GUI TRUE)     # used in qgsconfig.h
  endif()

  if ( WITH_DESKTOP AND NOT WITH_ANALYSIS )
    message(FATAL_ERROR "Desktop cannot be built without analysis")
  endif()

  if ( WITH_QGIS_PROCESS AND NOT WITH_ANALYSIS )
    message(FATAL_ERROR "Process tool cannot be built without analysis")
  endif()

  if ( WITH_DESKTOP )
    # The qgis_desktop target is meant to build a minimal but complete running QGIS during development
    # This should help to reduce compile time while still having a "complete enough" QGIS for most of the development
    #[[
    1. 定义一个没有自己源文件的自定义目标，相当于给一组操作/依赖起了一个名字，方便用 `cmake --build` 或 IDE 单独构建这个“虚拟目标”。
    2. 指定目标之间的构建依赖关系，让某个目标在编译前必须先完成其他目标，从而控制构建顺序。
    ]]
    add_custom_target(qgis_desktop
      DEPENDS qgis provider_postgres staged-plugins resources svg doc icons
    )
    if ( WITH_PYTHON )
      add_dependencies(qgis_desktop qgispython pycore pygui pyanalysis staged-plugins pyplugin-installer )
    endif()
  endif()

  # try to configure and build MDAL support
  set (WITH_INTERNAL_MDAL TRUE CACHE BOOL "Determines whether MDAL should be built from internal copy (recommended)")
  if (NOT WITH_INTERNAL_MDAL)
    set (MDAL_PREFIX "" CACHE PATH "Path to MDAL base directory")
  endif()

  # try to configure and build POLY2TRI support
  set (WITH_INTERNAL_POLY2TRI ${PREFER_INTERNAL_LIBS} CACHE BOOL "Determines whether POLY2TRI should be built from internal copy")
  set (WITH_INTERNAL_MESHOPTIMIZER ${PREFER_INTERNAL_LIBS} CACHE BOOL "Determines whether MESHOPTIMIZER should be built from internal copy")

  # try to configure and build POSTGRESQL support
  set (WITH_POSTGRESQL TRUE CACHE BOOL "Determines whether POSTGRESQL support should be built")
  if (WITH_POSTGRESQL)
    set (POSTGRESQL_PREFIX "" CACHE PATH "Path to POSTGRESQL base directory")
  endif()

  # try to configure and build POSTGRESQL support
  set (WITH_SPATIALITE TRUE CACHE BOOL "Determines whether Spatialite support should be built (required for spatialite, virtual, wfs providers)")
  if (WITH_SPATIALITE)
      set (WITH_QSPATIALITE FALSE CACHE BOOL "Determines whether QSPATIALITE sql driver should be built")
  endif()

  set (WITH_ORACLE FALSE CACHE BOOL "Determines whether Oracle support should be built")
  if(WITH_ORACLE)
    set(HAVE_ORACLE TRUE)
    set(ORACLE_INCLUDEDIR "" CACHE STRING "Path to OCI headers")
    set(ORACLE_LIBDIR "" CACHE STRING "Path to OCI libraries")
  endif()

  set (WITH_HANA FALSE CACHE BOOL "Determines whether SAP HANA Spatial support should be built")
  if(WITH_HANA)
    find_package(ODBC)
    if(ODBC_FOUND)
      set(HAVE_HANA TRUE)
      add_subdirectory(external/odbccpp)
      set_target_properties(odbccpp_static PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    else()
      message(STATUS "Couldn't find ODBC library")
    endif()
  endif(WITH_HANA)

  set (WITH_PDAL TRUE CACHE BOOL "Determines whether PDAL support should be built")

  set (WITH_EPT TRUE CACHE BOOL "Determines whether Entwine Point Cloud (EPT) support should be built")

  set (WITH_COPC TRUE CACHE BOOL "Determines whether Cloud Optimized Point Cloud (COPC) support should be built")

  set (WITH_DRACO TRUE CACHE BOOL "Determines whether Draco support should be built")

  set (WITH_THREAD_LOCAL TRUE CACHE BOOL "Determines whether std::thread_local should be used")
  mark_as_advanced(WITH_THREAD_LOCAL)

  if (MINGW OR CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    # MingW has broken support for thread_local, so force disabling it
    # see
    # https://sourceforge.net/p/mingw-w64/bugs/445/
    # https://sourceforge.net/p/mingw-w64/bugs/527/
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80816

    # also OpenBSD has no thread_local support, see https://github.com/qgis/QGIS/issues/25248

  else()
    if (WITH_THREAD_LOCAL)
      set (USE_THREAD_LOCAL TRUE)  # used in qgsconfig.h
    endif()
  endif()

  # Compile flag. Make it possible to turn it off.
  set (PEDANTIC TRUE CACHE BOOL "Determines if we should compile in pedantic mode.")

  # whether coverage tests should be performed
  set (ENABLE_COVERAGE FALSE CACHE BOOL "Perform coverage tests?")

  # whether coverage documentation should be generated
  set (GENERATE_COVERAGE_DOCS FALSE CACHE BOOL "Generate coverage docs (requires lcov)?")

  # hide this variable because building of python bindings might fail
  # if set to other directory than expected
  mark_as_advanced(LIBRARY_OUTPUT_PATH)

  if (MSVC AND CMAKE_GENERATOR MATCHES "NMake")
    # following variable is also used in qgsconfig.h
    set (USING_NMAKE TRUE)
  endif()

  if (CMAKE_GENERATOR MATCHES "Ninja")
    # following variable is also used in qgsconfig.h
    set (USING_NINJA TRUE)
  endif()

  #[[
  1. 使用 CMake 官方的模块文件配置 flex 和 bison 。
  ]]
  find_package(FLEX 2.6 REQUIRED)
  find_package(BISON 2.4 REQUIRED)

  #############################################################
  # search for dependencies

  if(NOT WIN32 AND NOT ANDROID)
    include(CheckFunctionExists)
    CHECK_FUNCTION_EXISTS(openpty OPENPTY_IN_LIBC)
    if(NOT OPENPTY_IN_LIBC)
      set(CMAKE_REQUIRED_INCLUDES util.h)
      set(CMAKE_REQUIRED_LIBRARIES util)
      CHECK_FUNCTION_EXISTS(openpty NEED_LIBUTIL)
      if(NEED_LIBUTIL)
        set(OPENPTY_LIBRARY util)
      else()
        message (SEND_ERROR "openpty not found!")
      endif()
    endif()
  endif()

  # required
  find_package(Proj REQUIRED)
  message(STATUS "Found Proj: ${PROJ_VERSION} ${PROJ_DIR}")
  if(PROJ_VERSION VERSION_LESS "8.1")
    message(FATAL_ERROR "Cannot build QGIS using Proj older than 8.1")
  endif()

  find_package(GEOS REQUIRED)
  message(STATUS "Found Geos: ${GEOS_VERSION} ${GEOS_DIR}")
  find_package(GDAL REQUIRED)
  message(STATUS "Found GDAL: ${GDAL_VERSION} ${GDAL_DIR}")
  find_package(EXPAT REQUIRED)
  find_package(Spatialindex REQUIRED)
  find_package(LibZip REQUIRED)
  set (WITH_INTERNAL_NLOHMANN_JSON ${PREFER_INTERNAL_LIBS} CACHE BOOL "Determines whether the vendored copy of nlohmann-json should be used")
  find_package(nlohmann_json REQUIRED)

  # The following bypasses the FindSQLite3 module introduced in CMake 3.14
  # On case insensitive platforms (e.g. Windows) this is because
  # ./cmake/FindSqlite3.cmake comes first on the CMAKE_MODULE_PATH
  # (otherwise it is because of the case: *Sqlite3* vs. *SQLite3*)
  find_package(Sqlite3 REQUIRED)

  find_package(Protobuf CONFIG)
  find_package(Protobuf REQUIRED)

  message(STATUS "Found Protobuf: ${Protobuf_LIBRARIES}")
  if (NOT Protobuf_PROTOC_EXECUTABLE)
    message (SEND_ERROR "Protobuf library's 'protoc' tool was not found!")
  endif()
  find_package(ZLIB REQUIRED)       # for decompression of vector tiles in MBTiles file
  message(STATUS "Found zlib: ${ZLIB_LIBRARIES}")

  find_package(EXIV2 REQUIRED)

  # optional
  if (WITH_POSTGRESQL)
    find_package(Postgres) # PostgreSQL provider
  endif()

  if (NOT WITH_INTERNAL_MDAL)
    find_package(MDAL REQUIRED) # MDAL provider
  endif()

  if (NOT WITH_INTERNAL_POLY2TRI)
    find_package(poly2tri REQUIRED)
  endif()

  if (WITH_SPATIALITE)
    find_package(SpatiaLite REQUIRED)
    set (HAVE_SPATIALITE TRUE)
  endif()

  if (POSTGRES_FOUND)
    # following variable is used in qgsconfig.h
    set (HAVE_POSTGRESQL TRUE)
  endif()

  set (WITH_QTWEBKIT TRUE CACHE BOOL "Enable QtWebkit support")
  if (WITH_QTWEBKIT)
    add_definitions(-DWITH_QTWEBKIT)
    message(STATUS "Qt WebKit support enabled")
  else()
    message(STATUS "Qt WebKit support DISABLED.")
  endif()

  set (WITH_INTERNAL_LAZPERF TRUE CACHE BOOL "Determines whether LazPerf should be built from internal copy (recommended)")
  if (WITH_EPT OR WITH_COPC)
    if (NOT WITH_INTERNAL_LAZPERF)
      find_package(LazPerf) # for decompression of point clouds
    endif()
    if (NOT LazPerf_FOUND)
      message(STATUS "Using embedded laz-perf")
    endif()
  endif()

  if (WITH_EPT)  # EPT provider
    find_package(ZSTD REQUIRED) # for decompression of point clouds
    set(HAVE_EPT TRUE)  # used in qgsconfig.h
  endif()

  if (WITH_COPC)  # COPC provider
    set(HAVE_COPC TRUE)  # used in qgsconfig.h
  endif()

  if (WITH_PDAL)
    if (NOT WITH_EPT)
      message(FATAL_ERROR "PDAL provider cannot be built with EPT disabled")
    endif()
    find_package(PDAL REQUIRED) # PDAL provider
    set(HAVE_PDAL_QGIS TRUE)  # used in qgisconfig.h. note -- we can't use HAVE_PDAL here as the grass public headers redefine this!
    if (PDAL_VERSION_MAJOR GREATER 3 OR (PDAL_VERSION_MAJOR EQUAL 2 AND PDAL_VERSION_MINOR GREATER_EQUAL 5))
      set(PDAL_2_5_OR_HIGHER TRUE)
    endif()
  endif()

  if (WITH_DRACO)
    find_package(Draco REQUIRED)
    message(STATUS "Found Draco: ${DRACO_LIBRARY} (${DRACO_VERSION})")
    set(HAVE_DRACO TRUE)  # used in qgisconfig.h
  endif()

  #############################################################
  # search for Qt

  set (BUILD_WITH_QT6 FALSE CACHE BOOL "Enable (experimental) Qt6 support")
  if (BUILD_WITH_QT6)
    set(QT_VERSION_MAJOR 6)
    set(QT_MIN_VERSION 6.4.0)
    set(QT_VERSION_BASE "Qt6")
    set(QT_VERSION_BASE_LOWER "qt6")
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
  else()
    set(QT_VERSION_MAJOR 5)
    set(QT_MIN_VERSION 5.15.2)
    set(QT_VERSION_BASE "Qt5")
    set(QT_VERSION_BASE_LOWER "qt5")
    set(HAS_KDE_QT5_PDF_TRANSFORM_FIX FALSE CACHE BOOL "Using KDE's Qt 5.15 fork with the PDF brush transform fix")
    set(HAS_KDE_QT5_SMALL_CAPS_FIX FALSE CACHE BOOL "Using KDE's Qt 5.15 fork with the QFont::SmallCaps fix")
    set(HAS_KDE_QT5_FONT_STRETCH_FIX FALSE CACHE BOOL "Using KDE's Qt 5.15 fork with the QFont stretch fix")
  endif()

  # Use QtSerialPort optionally for GPS
  set (WITH_QTSERIALPORT TRUE CACHE BOOL "Determines whether QtSerialPort should be tried for GPS positioning")
  if (WITH_QTSERIALPORT)
      find_package(${QT_VERSION_BASE} COMPONENTS SerialPort REQUIRED)
      # following variable is used in qgsconfig.h
      set (HAVE_QTSERIALPORT TRUE)
  endif()

  # Use QtGamepad optionally for input control
  set (WITH_QTGAMEPAD FALSE CACHE BOOL "Determines whether QtGamepad should be tried for GPS positioning")
  if (WITH_QTGAMEPAD)
      find_package(${QT_VERSION_BASE} COMPONENTS Gamepad REQUIRED)
      # following variable is used in qgsconfig.h
      set (HAVE_QTGAMEPAD TRUE)
  endif()

  set (WITH_PDF4QT FALSE CACHE BOOL "Determines whether the embedded PDF4Qt library should be build for PDF and HTML to QPainter conversion")
  if (WITH_PDF4QT)
    set(HAVE_PDF4QT TRUE)  # used in qgisconfig.h
    message(STATUS "PDF4Qt enabled")
  else()
    message(STATUS "PDF4Qt disabled")
  endif()

  find_package(${QT_VERSION_BASE} COMPONENTS Core Gui Widgets Network Xml Svg Concurrent Test Sql Positioning REQUIRED)
  if (BUILD_WITH_QT6)
    find_package(${QT_VERSION_BASE} COMPONENTS Core5Compat REQUIRED)
  endif()

  if (NOT IOS)
    set (DEFAULT_WITH_QTPRINTER TRUE)
  else ()
    set (DEFAULT_WITH_QTPRINTER FALSE)
  endif()
  set (WITH_QTPRINTER ${DEFAULT_WITH_QTPRINTER} CACHE BOOL "Enable QtPrinter support")
  if (WITH_QTPRINTER)
    find_package(${QT_VERSION_BASE} COMPONENTS PrintSupport REQUIRED)
    set (HAVE_QTPRINTER TRUE)
  endif()

  if (WITH_QTWEBKIT)
    if(BUILD_WITH_QT6)
      message(FATAL_ERROR "Qt WebKit support cannot be enabled on Qt 6 builds")
    endif()
    find_package(Qt5WebKit REQUIRED)
    find_package(Qt5WebKitWidgets REQUIRED)
  endif()
  if (WITH_3D)
    find_package(${QT_VERSION_BASE} COMPONENTS 3DCore 3DRender 3DInput 3DLogic 3DExtras REQUIRED)
    set(HAVE_3D TRUE)  # used in qgsconfig.h
  endif()

  set (WITH_QTWEBENGINE FALSE CACHE BOOL "Enable QtWebEngine support")
  if (WITH_QTWEBENGINE)
    if(BUILD_WITH_QT6)
      find_package(${QT_VERSION_BASE} COMPONENTS WebEngineCore REQUIRED)
    else()
      # QWebEnginePage is only available in widgets module in Qt5
      find_package(${QT_VERSION_BASE} COMPONENTS WebEngineCore WebEngineWidgets REQUIRED)
    endif()

    message(STATUS "QtWebEngine support enabled")
    set(HAVE_WEBENGINE TRUE)  # used in qgsconfig.h
  else()
    message(STATUS "QtWebEngine support DISABLED.")
    set(HAVE_WEBENGINE FALSE)  # used in qgsconfig.h
  endif()

  # get the Qt plugins directory
  get_target_property(QMAKE_EXECUTABLE ${QT_VERSION_BASE}::qmake LOCATION)

  execute_process(COMMAND ${QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS RESULT_VARIABLE return_code OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE DEFAULT_QT_PLUGINS_DIR )
  set (QT_PLUGINS_DIR ${DEFAULT_QT_PLUGINS_DIR} CACHE STRING "Path to installation directory for Qt Plugins. Defaults to Qt native plugin directory")

  if (BUILD_WITH_QT6)
    message(STATUS "Found Qt version: ${Qt6Core_VERSION}")
  else()
    message(STATUS "Found Qt version: ${Qt5Core_VERSION_STRING}")
  endif()
  if (WITH_QUICK)
    find_package(${QT_VERSION_BASE} COMPONENTS Qml Quick REQUIRED)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Android" AND NOT BUILD_WITH_QT6)
      find_package(${QT_VERSION_BASE} COMPONENTS AndroidExtras)
    endif()

    # following variable is used in qgsconfig.h
    set (HAVE_QUICK TRUE)
  endif()

  if(WITH_QTWEBKIT)
    set(OPTIONAL_QTWEBKIT ${Qt5WebKitWidgets_LIBRARIES})
  endif()

  # search for QScintilla2 (C++ lib)
  if (WITH_GUI)
    find_package(QScintilla REQUIRED)
    find_package(Qwt REQUIRED)
  endif()

  # Password helper
  if(BUILD_WITH_QT6)
    find_package(Qt6Keychain CONFIG REQUIRED)
  else()
    find_package(Qt5Keychain CONFIG REQUIRED)
  endif()
  # Master password hash and authentication encryption
  find_package(QCA REQUIRED)
  # Check for runtime dependency of qca-ossl plugin
  # REQUIRED if unit tests are to be run from build directory
  if(NOT MSVC)
    include(QCAMacros)
    FIND_QCAOSSL_PLUGIN_CPP(ENABLE_TESTS)
  endif()

  if (APPLE)
    # Libtasn1 is for DER-encoded PKI ASN.1 parsing/extracting workarounds
    find_package(Libtasn1 REQUIRED)
  endif()

  # Disable automatic conversion from QString to ASCII 8-bit strings (char *)
  # (Keeps code compatible with Qt/Mac/64bit)
  add_definitions(-DQT_NO_CAST_TO_ASCII)
endif()

set(CMAKE_AUTOMOC ON)

# build our version of astyle
set (WITH_ASTYLE FALSE CACHE BOOL "Deprecated. Should be OFF. If you plan to contribute you should reindent with scripts/prepare_commit.sh (using 'our' astyle)")

# QML
set(QML_IMPORT_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" CACHE PATH "QML directory for QML autocomplete")

#############################################################
# testing
# whether unit tests should be build
set (ENABLE_TESTS TRUE CACHE BOOL "Build unit tests?")
if (ENABLE_TESTS)
  set (PUSH_TO_CDASH FALSE CACHE BOOL "Determines whether test results should be pushed to CDASH site")
  set(QT_USE_QTTEST TRUE)
  enable_testing()
  if (PUSH_TO_CDASH)
    # Adds some testing specific build targets e.g. make Experimental
    include(Dart)
  endif()
  # Additional test configuration options e.g. max upload size of test report
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake_templates/CTestCustom.cmake.in"
    "${CMAKE_BINARY_DIR}/CTestCustom.cmake"
    IMMEDIATE @ONLY)
  if (PUSH_TO_CDASH)
    configure_file(
      "${CMAKE_SOURCE_DIR}/cmake_templates/CTestConfig.cmake.in"
      "${CMAKE_BINARY_DIR}/CTestConfig.cmake"
      IMMEDIATE @ONLY)
  endif()
  # For server side testing we have no X, we can use xvfb as a fake x
  # sudo apt-get install xvfb
  add_custom_target(check COMMAND xvfb-run --server-args=-screen\ 0\ 1024x768x24 ctest --output-on-failure)

  # Define SOURCETREE fixture
  add_test(NAME   logGitStatus COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts/chkgitstatus.sh log)
  add_test(NAME checkGitStatus COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts/chkgitstatus.sh check)
  set_tests_properties(logGitStatus   PROPERTIES FIXTURES_SETUP   SOURCETREE)
  set_property(TEST logGitStatus PROPERTY SKIP_RETURN_CODE 2)
  set_tests_properties(checkGitStatus PROPERTIES FIXTURES_CLEANUP SOURCETREE)
  set_property(TEST checkGitStatus PROPERTY SKIP_RETURN_CODE 2)

endif()

add_custom_target(tags COMMAND
  cd ${CMAKE_CURRENT_SOURCE_DIR} && ctags
    --c++-kinds=+p
    --exclude=build
    --exclude=.ci
    --exclude=cmake
    --exclude=cmake_templates
    --exclude=debian
    --exclude=doc
    --exclude=.docker
    --exclude=editors
    --exclude=.github
    --exclude=i18n
    --exclude=images
    --exclude=resources
    --exclude=rpm
    --exclude=scripts
    --exclude=Testing
    --exclude=tests
    --exclude=.tx
    --extras=+q
    --fields=+iaS
    --language-force=C++
    --recurse=yes
)

if (WITH_CORE)
# ModelTest
  set(ENABLE_MODELTEST FALSE CACHE BOOL "Enable QT ModelTest (not for production)")
endif()

#############################################################
# enable warnings

if (PEDANTIC)
  message (STATUS "Pedantic compiler settings enabled")
  if(MSVC)
    set(_warnings "")
    if (NOT USING_NMAKE AND NOT USING_NINJA)
      set(_warnings "${_warnings} /W4" )
    endif()

    # disable warnings
    set(_warnings "${_warnings} /wd4091 ")  # 'typedef': ignored on left of '' when no variable is declared (occurs in MS DbgHelp.h header)
    set(_warnings "${_warnings} /wd4100 ")  # unused formal parameters
    set(_warnings "${_warnings} /wd4127 ")  # constant conditional expressions (used in Qt template classes)
    set(_warnings "${_warnings} /wd4190 ")  # 'identifier' has C-linkage specified, but returns UDT 'identifier2' which is incompatible with C
    set(_warnings "${_warnings} /wd4231 ")  # nonstandard extension used : 'identifier' before template explicit instantiation (used in Qt template classes)
    set(_warnings "${_warnings} /wd4244 ")  # conversion from '...' to '...' possible loss of data
    set(_warnings "${_warnings} /wd4251 ")  # needs to have dll-interface to be used by clients of class (occurs in Qt template classes)
    set(_warnings "${_warnings} /wd4267 ")  # 'argument': conversion from 'size_t' to 'int', possible loss of data
    set(_warnings "${_warnings} /wd4275 ")  # non dll-interface class '...' used as base for dll-interface class '...'
    set(_warnings "${_warnings} /wd4290 ")  # c++ exception specification ignored except to indicate a function is not __declspec(nothrow) (occurs in sip generated bindings)
    set(_warnings "${_warnings} /wd4456 ")  # declaration of '...' hides previous local declaration
    set(_warnings "${_warnings} /wd4457 ")  # declaration of '...' hides a function parameter
    set(_warnings "${_warnings} /wd4458 ")  # declaration of '...' hides class member
    set(_warnings "${_warnings} /wd4505 ")  # unreferenced local function has been removed (QgsRasterDataProvider::extent)
    set(_warnings "${_warnings} /wd4510 ")  # default constructor could not be generated (sqlite3_index_info, QMap)
    set(_warnings "${_warnings} /wd4512 ")  # assignment operator could not be generated (sqlite3_index_info)
    set(_warnings "${_warnings} /wd4610 ")  # user defined constructor required (sqlite3_index_info)
    set(_warnings "${_warnings} /wd4706 ")  # assignment within conditional expression (pal)
    set(_warnings "${_warnings} /wd4714 ")  # function '...' marked as __forceinline not inlined (QString::toLower/toUpper/trimmed)
    set(_warnings "${_warnings} /wd4800 ")  # 'int' : forcing value to bool 'true' or 'false' (performance warning)
    set(_warnings "${_warnings} /wd4996 ")  # '...': was declared deprecated (unfortunately triggered when implementing deprecated interfaces even when it is deprecated too)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_warnings}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_warnings}")
  else()
    # add warnings via flags (not as definitions as on Mac -Wall can not be overridden per language )
    set(_warnings "-Wall -Wextra -Wno-long-long -Wformat-security -Wno-strict-aliasing")

    set(WERROR FALSE CACHE BOOL "Treat build warnings as errors.")
    if (WERROR)
      set(_warnings "${_warnings} -Werror")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_warnings}")

    # c++ only warnings

    set(_warnings "${_warnings} -Wnon-virtual-dtor")

    # unavoidable - we can't avoid these, as older, supported compilers do not support removing the redundant move
    set(_warnings "${_warnings} -Wno-redundant-move")

    # disable misleading-indentation warning -- it's slow to parse the sip files and not needed since we have the automated code styling rules
    set(_warnings "${_warnings} -Wno-misleading-indentation")

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.9.999)
      # heaps of these thrown by Qt headers at the moment (sep 2019)
      set(_warnings "${_warnings} -Wno-deprecated-copy")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_warnings}")

    # Qt produces lots of warnings with strict aliasing (as of Qt 4.4.0 & GCC 4.3)
    # There are redundant declarations in Qt and GDAL
    # add_definitions( -fstrict-aliasing -Wstrict-aliasing=1 -Wredundant-decls )

    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wreturn-type-c-linkage -Woverloaded-virtual -Wimplicit-fallthrough")
    endif()

    # add any extra CXXFLAGS flags set by user. can be -D CXX_EXTRA_FLAGS or environment variable
    # command line -D option overrides environment variable
    # e.g. useful for suppressing transient upstream warnings in dependencies, like Qt
    set(CXX_EXTRA_FLAGS "" CACHE STRING "Additional appended CXXFLAGS")
    if ("${CXX_EXTRA_FLAGS}" STREQUAL "" AND DEFINED $ENV{CXX_EXTRA_FLAGS})
      set(CXX_EXTRA_FLAGS "$ENV{CXX_EXTRA_FLAGS}")
    endif()
    if (NOT "${CXX_EXTRA_FLAGS}" STREQUAL "")
      message (STATUS "Appending CXX_EXTRA_FLAGS")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_EXTRA_FLAGS}")
    endif()
  endif()

endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Qunused-arguments")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Qunused-arguments")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Qunused-arguments")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Qunused-arguments")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)")
  # spatialite crashes on ppc - see bugs.debian.org/603986
  add_definitions( -fno-strict-aliasing )
endif()

if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
  message (STATUS "Debug output enabled")
  set(QGISDEBUG TRUE)
else()
  set(QGISDEBUG FALSE)
endif()

set (AGGRESSIVE_SAFE_MODE FALSE CACHE BOOL "Forces a aggressive safe mode where issues like unsafe thread access will resort in fatal exceptions")

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /std:c++17 /permissive-")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8 /std:c++17 /permissive-")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  # Increase our stack size to 8MB to give us a bit more room and avoid stackoverflow crashes
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8388608")
  # disable macros that offend std::numeric_limits<T>::min()/max()
  add_definitions(-DNOMINMAX)
endif()

if(BUILD_WITH_QT6)
  # Qt deprecated warnings are too noisy for now -- we have a LOT of QVariant::Type usage
  # in order to maintain Qt5 compatibility
  add_definitions(-DQT_NO_DEPRECATED_WARNINGS)
else()
  # Unfortunately Qwt uses deprecated QString::null in headers, preventing this being raised above 5.8
  add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050800)
endif()

# For fast string concatenation
add_definitions(-DQT_USE_QSTRINGBUILDER)

set(WITH_GSL TRUE CACHE BOOL "Determines whether GSL library should be used")

if (WITH_ANALYSIS AND WITH_GSL)
  find_package(GSL REQUIRED)
  set(HAVE_GSL TRUE)
  set(HAVE_GEOREFERENCER TRUE)
endif()

if(ENABLE_COVERAGE)
  include("cmake/modules/coverage/CodeCoverage.cmake")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage --coverage")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage --coverage")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage --coverage")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage --coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage --coverage")
  SETUP_TARGET_FOR_COVERAGE(qgis_coverage ctest coverage)
endif()

if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
  include("cmake/modules/linker.cmake")
endif()

#############################################################
# platform specific stuff
if (WITH_CORE)
  if (WIN32)
    set (DEFAULT_LIB_SUBDIR     lib)
    set (DEFAULT_LIBEXEC_SUBDIR .)
    set (DEFAULT_DATA_SUBDIR    .)
    set (DEFAULT_PLUGIN_SUBDIR  plugins)
    set (DEFAULT_INCLUDE_SUBDIR include)
    set (DEFAULT_QML_SUBDIR     qml)

    set (DEFAULT_SERVER_MODULE_SUBDIR server)

    if (MSVC)
      set (DEFAULT_BIN_SUBDIR bin)
      set (DEFAULT_CGIBIN_SUBDIR bin)
      # put all the build products into a single directory
      # under build (doesn't affect install target) to make for
      # easier debugging.

      # Turn on defines for non standard maths stuff
      add_definitions(-D_USE_MATH_DEFINES)

      # Turn off deprecation warnings
      add_definitions(-D_CRT_SECURE_NO_WARNINGS)
      add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)

      if (INSTALL_DEPS)
        install(DIRECTORY ${INSTALL_DEPS} DESTINATION .)
      endif()
    else()
      set (DEFAULT_BIN_SUBDIR .)
      set (DEFAULT_CGIBIN_SUBDIR .)
    endif()
  else()

    if(APPLE)
        set(QGIS_MACAPP_FRAMEWORK TRUE CACHE BOOL "Build as a framework on OSX")
    endif()

    if (APPLE AND QGIS_MACAPP_FRAMEWORK)
      if (POLICY CMP0042) # in CMake 3.0.0+
        set (CMAKE_MACOSX_RPATH OFF) # otherwise ON by default
      endif()
      if (POLICY CMP0068) # in CMake 3.9.0+
        cmake_policy(SET CMP0068 NEW)
      endif()
      # for Mac OS X, everything is put inside an application bundle
      # save the root install prefix for the app later
      set (QGIS_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
      set (QGIS_MACAPP_PREFIX ${CMAKE_INSTALL_PREFIX}/${QGIS_APP_NAME}.app/Contents)
      # common prefix for components, let cmake handle it
      set (CMAKE_INSTALL_PREFIX ${QGIS_MACAPP_PREFIX}/MacOS)
      # 5 bundling levels, each includes previous
      #  -1  nothing
      #   0  fixup the library paths for all QGIS libraries with @loader_path
      #   1  Qt frameworks
      #   2  non-system libraries, "standard"
      #   3  non-system frameworks, "standalone"
      set (QGIS_MACAPP_BUNDLE 1 CACHE STRING "What to bundle into app package")
      set (QGIS_MACAPP_BUNDLE_USER "" CACHE STRING "Path to user bundling script")
      set (QGIS_MACAPP_INSTALL_DEV FALSE CACHE BOOL "Install developer frameworks")
      set (QGIS_MACAPP_DEV_PREFIX "/Library/Frameworks" CACHE STRING "Path to install developer frameworks")

      set (DEFAULT_BIN_SUBDIR      bin)
      set (QGIS_BIN_SUBDIR_REV     ..)
      set (DEFAULT_CGIBIN_SUBDIR   fcgi-bin)
      set (QGIS_CGIBIN_SUBDIR_REV  ..)
      set (DEFAULT_LIB_SUBDIR      lib)
      set (QGIS_LIB_SUBDIR_REV     ..)
      set (QGIS_FW_SUBDIR          ../Frameworks)
      set (QGIS_FW_SUBDIR_REV      ../MacOS)
      set (DEFAULT_DATA_SUBDIR     ../Resources)
      set (QGIS_DATA_SUBDIR_REV    ../MacOS)
      set (DEFAULT_LIBEXEC_SUBDIR  lib/qgis)
      set (QGIS_LIBEXEC_SUBDIR_REV ../..)
      set (DEFAULT_PLUGIN_SUBDIR   ../PlugIns/qgis)
      set (QGIS_PLUGIN_SUBDIR_REV  ../../MacOS)
      set (DEFAULT_INCLUDE_SUBDIR  include/qgis)
      set (DEFAULT_QML_SUBDIR     qml)

      # Set server moodules path to DEFAULT_LIBEXEC_SUBDIR+'/server'
      set (DEFAULT_SERVER_MODULE_SUBDIR ${DEFAULT_LIBEXEC_SUBDIR}/server)

      # path for framework references when running from build directory
      # changed later to reference in-app resources upon install
      set (CMAKE_INSTALL_NAME_DIR ${CMAKE_BINARY_DIR}/output/lib)
      # recent cmakes force SDKs, recent SDKs don't have user symlinks
      # need to find non-system frameworks
      # cmake bug #0007250 - CMAKE_SHARED_LINKER_FLAGS ignored when creating
      # a framework, so these need to be manually handled with LINK_FLAGS options
      set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -F/Library/Frameworks")
      set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -F/Library/Frameworks")
      set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -F/Library/Frameworks")

    else()
      # UNIX
      set (DEFAULT_BIN_SUBDIR     bin)
      set (DEFAULT_CGIBIN_SUBDIR  bin)
      set (DEFAULT_LIB_SUBDIR     lib${LIB_SUFFIX})
      set (DEFAULT_DATA_SUBDIR    share/qgis)
      set (DEFAULT_LIBEXEC_SUBDIR lib${LIB_SUFFIX}/qgis)
      set (DEFAULT_PLUGIN_SUBDIR  lib${LIB_SUFFIX}/qgis/plugins)
      set (DEFAULT_INCLUDE_SUBDIR include/qgis)
      set (DEFAULT_QML_SUBDIR     qml)

      set (DEFAULT_SERVER_MODULE_SUBDIR ${DEFAULT_LIBEXEC_SUBDIR}/server)

      # QGIS_MACAPP_FRAMEWORK=FALSE
      if(APPLE)
        set (QGIS_MACAPP_BUNDLE -1)
        set (CMAKE_FRAMEWORK FALSE)
        set (QGIS_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
      endif()
    endif()

  endif()

  if (ANDROID)
    set (DEFAULT_PLUGIN_SUBDIR  lib)
    string(REPLACE "<CMAKE_SHARED_LIBRARY_SONAME_CXX_FLAG><TARGET_SONAME>" "" CMAKE_CXX_CREATE_SHARED_MODULE "${CMAKE_CXX_CREATE_SHARED_MODULE}")
  endif()

  if("${CMAKE_SYSTEM_NAME}"  MATCHES "Linux")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined")
  endif() #"${CMAKE_SYSTEM_NAME}"  MATCHES "Linux")

  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  include(GenerateExportHeader)

  set (WITH_CLAZY FALSE CACHE BOOL "Determines whether Clazy checks should be enabled")
  mark_as_advanced (WITH_CLAZY)
  if (WITH_CLAZY)
    set(CMAKE_CXX_BASE_FLAGS "${CMAKE_CXX_FLAGS}")
    #  qcolor-from-literal crashes clang with clazy 1.11
    set(CLAZY_BASE_CHECKS "connect-3arg-lambda,lambda-unique-connection,empty-qstringliteral,fully-qualified-moc-types,lowercase-qml-type-name,qfileinfo-exists,qmap-with-pointer-key,unused-non-trivial-variable,overridden-signal,qdeleteall,qstring-left,skipped-base-method,isempty-vs-count,missing-qobject-macro,container-anti-pattern")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_BASE_FLAGS} -Xclang -plugin-arg-clazy -Xclang ${CLAZY_BASE_CHECKS}")

    if (WERROR AND NOT ${QGISDEBUG})
      message(FATAL_ERROR "Don't activate both WERROR and ADD_CLAZY_CHECKS in Release mode, you will get many false positive errors")
    endif()

    if (WERROR AND ("$ENV{CLAZY_IGNORE_DIRS}" STREQUAL ""))
      message(WARNING "WERROR and ADD_CLAZY_CHECKS are activated. To avoid unwanted Clazy error, please set the CLAZY_IGNORE_DIRS environment variable like this : \nexport CLAZY_IGNORE_DIRS=\"(.*/external/.*)|(.*sip_.*part.*)\"")
    endif()
  endif()
endif()

#############################################################
# user-changeable settings which can be used to customize
# layout of QGIS installation
# (default values are platform-specific)

set (QGIS_BIN_SUBDIR     ${DEFAULT_BIN_SUBDIR}     CACHE STRING "Subdirectory where executables will be installed")
set (QGIS_CGIBIN_SUBDIR  ${DEFAULT_CGIBIN_SUBDIR}  CACHE STRING "Subdirectory where CGI executables will be installed")
set (QGIS_LIB_SUBDIR     ${DEFAULT_LIB_SUBDIR}     CACHE STRING "Subdirectory where libraries will be installed")
set (QGIS_LIBEXEC_SUBDIR ${DEFAULT_LIBEXEC_SUBDIR} CACHE STRING "Subdirectory where private executables will be installed")
set (QGIS_DATA_SUBDIR    ${DEFAULT_DATA_SUBDIR}    CACHE STRING "Subdirectory where QGIS data will be installed")
set (QGIS_PLUGIN_SUBDIR  ${DEFAULT_PLUGIN_SUBDIR}  CACHE STRING "Subdirectory where plugins will be installed")
set (QGIS_INCLUDE_SUBDIR ${DEFAULT_INCLUDE_SUBDIR} CACHE STRING "Subdirectory where header files will be installed")
set (QGIS_QML_SUBDIR     ${DEFAULT_QML_SUBDIR}     CACHE STRING "Subdirectory where qml files/libraries will be installed")

set (QGIS_SERVER_MODULE_SUBDIR  ${DEFAULT_SERVER_MODULE_SUBDIR}  CACHE STRING "Subdirectory where server modules will be installed")

# mark *_SUBDIR variables as advanced as this is not something
# that an average user would use
mark_as_advanced (QGIS_BIN_SUBDIR QGIS_CGIBIN_SUBDIR QGIS_LIB_SUBDIR QGIS_LIBEXEC_SUBDIR QGIS_DATA_SUBDIR QGIS_PLUGIN_SUBDIR QGIS_INCLUDE_SUBDIR)

# full paths for the installation
set (QGIS_BIN_DIR     ${QGIS_BIN_SUBDIR})
set (QGIS_CGIBIN_DIR  ${QGIS_CGIBIN_SUBDIR})
set (QGIS_LIB_DIR     ${QGIS_LIB_SUBDIR})
set (QGIS_LIBEXEC_DIR ${QGIS_LIBEXEC_SUBDIR})
set (QGIS_DATA_DIR    ${QGIS_DATA_SUBDIR})
set (QGIS_PLUGIN_DIR  ${QGIS_PLUGIN_SUBDIR})
set (QGIS_INCLUDE_DIR ${QGIS_INCLUDE_SUBDIR})
set (QGIS_QML_DIR     ${QGIS_QML_SUBDIR})

set (QGIS_SERVER_MODULE_DIR ${QGIS_SERVER_MODULE_SUBDIR})

# set the default locations where the targets (executables, libraries) will land when compiled
# this is to allow running qgis from the source tree without having to actually do a "make install"
set (QGIS_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/${QGIS_BIN_SUBDIR})
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/${QGIS_LIB_SUBDIR})

# write a marker with source directory path into the output's bin directory
# if run from the build directory QGIS will detect it and alter the paths
file(WRITE ${QGIS_OUTPUT_DIRECTORY}/${QGIS_BIN_SUBDIR}/qgisbuildpath.txt "${CMAKE_SOURCE_DIR}\n${QGIS_OUTPUT_DIRECTORY}")

# Write the qgis.env file to the bin folder to allow running qgis.exe correctly from build folder
# and find the SDK installed folders. Only runs when VCPKG_APPLOCAL_DEPS=OFF because ON means it is packaged together
if(WITH_VCPKG AND NOT VCPKG_APPLOCAL_DEPS AND WIN32)
  # Used in the env.in file to setup the correct paths to the SDK folder
    set(VCPKG_TARGET_PATH "${VCPKG_INSTALL_PREFIX}/${VCPKG_TARGET_TRIPLET}")
    configure_file("${CMAKE_SOURCE_DIR}/ms-windows/dev/qgis.env.in" "${QGIS_OUTPUT_DIRECTORY}/${QGIS_BIN_SUBDIR}/qgis.env")
endif()

# manual page - makes sense only on unix systems
if (UNIX AND NOT APPLE)
  set (DEFAULT_MANUAL_SUBDIR  man)
  set (QGIS_MANUAL_SUBDIR  ${DEFAULT_MANUAL_SUBDIR}  CACHE STRING "Subdirectory where manual files will be installed")
  mark_as_advanced (QGIS_MANUAL_SUBDIR)
  set (QGIS_MANUAL_DIR  ${CMAKE_INSTALL_PREFIX}/${QGIS_MANUAL_SUBDIR})
endif()

set (DISABLE_DEPRECATED FALSE CACHE BOOL "If set to true, it will disable deprecated functionality to prepare for the next generation of QGIS")
if (DISABLE_DEPRECATED)
  add_definitions(-DQGIS_DISABLE_DEPRECATED)
endif()

# whether to install required system libs in the output package
set(QGIS_INSTALL_SYS_LIBS TRUE CACHE BOOL "If set to TRUE install all required system libs in the output package")

option(ENABLE_UNITY_BUILDS "Enable Unity builds, that is compiling several .cpp files in the same compilation unit (EXPERIMENTAL. Not recommended for production builds)" OFF)
if (ENABLE_UNITY_BUILDS)
  if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
  endif()
endif()


#############################################################
# Python

set(MIN_PYTHON_VERSION "3.9")
set(Python_FIND_FRAMEWORK "LAST")


if (WITH_BINDINGS)
  find_package(Python ${MIN_PYTHON_VERSION} REQUIRED COMPONENTS Interpreter Development)
else()
  find_package(Python ${MIN_PYTHON_VERSION} REQUIRED COMPONENTS Interpreter)
endif()

# Fix python site-packages for Fedora
# See https://github.com/qgis/QGIS/issues/54348#issuecomment-1694216152
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(EXISTS "/etc/fedora-release")
    EXECUTE_PROCESS(COMMAND ${Python_EXECUTABLE} -c "import sysconfig;print(sysconfig.get_path(\"platlib\", \"rpm_prefix\"), end=\"\")" OUTPUT_VARIABLE Python_SITEARCH)
  endif()
endif()

message("-- Found Python executable: ${Python_EXECUTABLE} (version ${Python_VERSION})")
message("-- Python library: ${Python_LIBRARIES}")
message("-- Python site-packages: ${Python_SITEARCH}")

if (WITH_CORE AND WITH_BINDINGS)
  set(PYTHON_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/python)
  set (QGIS_PYTHON_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIRECTORY}/qgis)

  # python support: check for interpreter, sip, pyqt5
  find_package(SIP REQUIRED)
  if(BUILD_WITH_QT6)
    find_package(PyQt6 REQUIRED)
    set(PYQT_FOUND ${PYQT6_FOUND})
    set(PYQT_SIP_FLAGS ${PYQT6_SIP_FLAGS})
    set(PYQT_SIP_DIR ${PYQT6_SIP_DIR})
    set(PYQT_SIP_IMPORT ${PYQT6_SIP_IMPORT})
    set(PYQT_VERSION_STR ${PYQT6_VERSION_STR})
    set(PYQT_MOD_DIR ${PYQT6_MOD_DIR})
  else()
    find_package(PyQt5 REQUIRED)
    set(PYQT_FOUND ${PYQT5_FOUND})
    set(PYQT_SIP_FLAGS ${PYQT5_SIP_FLAGS})
    set(PYQT_SIP_DIR ${PYQT5_SIP_DIR})
    set(PYQT_SIP_IMPORT ${PYQT5_SIP_IMPORT})
    set(PYQT_VERSION_STR ${PYQT5_VERSION_STR})
    set(PYQT_MOD_DIR ${PYQT5_MOD_DIR})
  endif()
  separate_arguments(PYQT_SIP_FLAGS) # convert space separated values to a list

  find_package(Qsci REQUIRED)
  include(PythonMacros)
  include(SIPMacros)

  set(SIP_INCLUDES ${PYQT_SIP_DIR} ${CMAKE_SOURCE_DIR}/python)
  set(SIP_CONCAT_PARTS 25)

  if (NOT BINDINGS_GLOBAL_INSTALL)
    set(Python_SITEARCH ${QGIS_DATA_DIR}/python)
  endif()

  if (WITH_CUSTOM_WIDGETS)
    set(PYUIC_WIDGET_PLUGIN_DIRECTORY ${PYQT_MOD_DIR}/uic/widget-plugins/)
  endif()
endif()

#############################################################
# create qgsconfig.h
# installed with app target
configure_file(${CMAKE_SOURCE_DIR}/cmake_templates/qgsconfig.h.in ${CMAKE_BINARY_DIR}/qgsconfig.h)
include_directories(${CMAKE_BINARY_DIR})

#############################################################
# create qgsversion.h
include(CreateQgsVersion)
CREATE_QGSVERSION()

####################################################
# Added by Jef to prevent python core and gui libs linking to other qgisCore and qgisGui libs
# that may be in the same install prefix
if (WITH_CORE)
  link_directories(${CMAKE_BINARY_DIR}/src/core ${CMAKE_BINARY_DIR}/src/gui)
endif()

####################################################
# asan
set (WITH_ASAN FALSE CACHE BOOL "Use AddressSanitizer")
mark_as_advanced(WITH_ASAN)
if(WITH_ASAN)
  message(STATUS "Enabling AddressSanitizer")
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

#############################################################
# process subdirs

#create a variable to specify where our test data is
#so that unit tests can use TEST_DATA_DIR to locate
#the test data. See CMakeLists in test dirs for more info
#TEST_DATA_DIR is also used by QgsRenderChecker currently in core
set (TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/testdata")

add_subdirectory(doc)

if (WITH_CORE)
  add_subdirectory(src)
  add_subdirectory(images)
  add_subdirectory(resources)
  add_subdirectory(i18n)

  if (WITH_BINDINGS)
    add_subdirectory(python)
  endif()

  if (APPLE)
    # must be last for install, so install_name_tool can do its work
    add_subdirectory(mac)

    # allow QGIS to be run directly from build directory and to run unit tests
    execute_process(COMMAND /bin/mkdir -p "${QGIS_OUTPUT_DIRECTORY}/lib")
    execute_process(
      COMMAND /bin/ln -fs ../../Plugins/qgis/qgisgrass6.framework lib/
      WORKING_DIRECTORY "${QGIS_OUTPUT_DIRECTORY}"
    )
    execute_process(
      COMMAND /bin/ln -fs ../../Plugins/qgis/qgisgrass7.framework lib/
      WORKING_DIRECTORY "${QGIS_OUTPUT_DIRECTORY}"
    )
  endif()

  # manual page - makes sense only on unix systems
  if (UNIX AND NOT APPLE)
    install (FILES qgis.1 DESTINATION ${QGIS_MANUAL_DIR}/man1)
  endif()

  install(FILES cmake/FindQGIS.cmake DESTINATION ${QGIS_DATA_DIR})
endif()

if (WITH_ASTYLE)
  add_subdirectory(external/astyle)
endif()

if (ENABLE_TESTS)
  add_subdirectory(tests)
  set (CTEST_BINARY_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output/bin" )
  message (STATUS "Ctest Binary Directory set to: ${CTEST_BINARY_DIRECTORY}")
endif()

#############################################################
# Post-install commands
if (WITH_CORE)
  add_subdirectory(postinstall)
endif()

#############################################################
# Uninstall stuff see: http://www.vtk.org/Wiki/CMake_FAQ
if (WITH_CORE)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_templates/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
endif()

#############################################################
# Enable packaging
if (WITH_CORE)
  include(Bundle)
  include(VcpkgInstallDeps)
endif()

if (UNIX AND NOT APPLE)
  add_subdirectory(linux)
endif()
