set(_qgis_mtpl_supported_platform FALSE)
if(WIN32 AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_qgis_mtpl_supported_platform TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux"
       AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
       AND CMAKE_SIZEOF_VOID_P EQUAL 8
       AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
  set(_qgis_mtpl_supported_platform TRUE)
endif()

option(WITH_MTPL_PLUGIN "Build the self-contained MTPL plugin" ${_qgis_mtpl_supported_platform})
if(NOT WITH_MTPL_PLUGIN)
  return()
endif()

if(NOT _qgis_mtpl_supported_platform)
  message(WARNING
    "WITH_MTPL_PLUGIN is verified on Windows x64 with MSVC and Linux x86_64 with GNU GCC; this configuration is experimental")
endif()

add_subdirectory(vendor)

set(mtpl_support_SRCS
  qgsmtpldockwidget.cpp
  qgsmtplpackage.cpp
  qgsmtplpathwidget.cpp
  qgsmtplpluginlayer.cpp
  dialogs/qgsmtplpackagetoolswidget.cpp
  services/qgsmtplcredentialstore.cpp
  services/qgsmtplkeysidecar.cpp
  services/qgsmtplpackageoperation.cpp
  services/qgsmtplpackageservice.cpp
)

set(mtpl_support_HDRS
  qgsmtpldockwidget.h
  qgsmtplpackage.h
  qgsmtplpathwidget.h
  qgsmtplpluginlayer.h
  dialogs/qgsmtplpackagetoolswidget.h
  services/qgsmtplcredentialstore.h
  services/qgsmtplkeysidecar.h
  services/qgsmtplpackageoperation.h
  services/qgsmtplpackageservice.h
  services/qgsmtplstatus.h
)

add_library(qgis_mtpl_plugin_support STATIC ${mtpl_support_SRCS} ${mtpl_support_HDRS})
set_target_properties(qgis_mtpl_plugin_support PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(qgis_mtpl_plugin_support PUBLIC
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/dialogs
  ${CMAKE_CURRENT_SOURCE_DIR}/services
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mtpl/include
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mtpl/src/sfp
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(qgis_mtpl_plugin_support PUBLIC
  qgis_core
  qgis_gui
  qgis_mtpl_core
  Qt::Widgets
)

if(MSVC)
  target_compile_options(qgis_mtpl_plugin_support PRIVATE /utf-8)
endif()

if(WIN32)
  target_link_libraries(qgis_mtpl_plugin_support PUBLIC Advapi32)
endif()

add_library(plugin_mtpl MODULE qgsmtplplugin.cpp qgsmtplplugin.h)
target_link_libraries(plugin_mtpl PRIVATE qgis_mtpl_plugin_support)

if(MSVC)
  target_compile_options(plugin_mtpl PRIVATE /utf-8)
endif()

install(TARGETS plugin_mtpl
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR}
)

unset(_qgis_mtpl_supported_platform)
