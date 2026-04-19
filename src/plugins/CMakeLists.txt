if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS_OLD "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
endif()
# override default path where built files are put to allow running qgis without installing
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/${QGIS_PLUGIN_SUBDIR})
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/${QGIS_PLUGIN_SUBDIR})

add_subdirectory(topology)
add_subdirectory(geometry_checker)

if (WITH_QTWEBENGINE)
  if (QT_VERSION_MAJOR EQUAL 6)
    add_subdirectory(qcopilots)
    add_subdirectory(qcopilots_server)
  else()
    message(NOTICE "Skip the qcopilots plugin: qcopilots requires Qt6 (QT_VERSION_MAJOR=6).")
    message(NOTICE "Skip the qcopilots_server plugin: qcopilots_server requires Qt6 (QT_VERSION_MAJOR=6).")
  endif()
else()
  message(NOTICE "Skip the qcopilots plugin: If you want to enable it, please use a Qt6 build and enable the WITH_QTWEBENGINE option while disabling the WITH_QTWEBKIT option, as the WebEngineWidgets component in Qt6 supports a newer Chrome kernel.")
  message(NOTICE "Skip the qcopilots_server plugin: qcopilots_server relies on the same Qt6 + WebEngine build profile as qcopilots.")
endif()

if (WITH_SPATIALITE)
  add_subdirectory(offline_editing)
endif()

if (GRASS_FOUND)
  set (WITH_GRASS_PLUGIN TRUE CACHE BOOL "Determines whether the GRASS c++ plugin should be built")
  if (WITH_GRASS_PLUGIN)
    add_subdirectory(grass)
  endif()
endif()

if (CUSTOM_PLUGINS)
  add_subdirectory(${CUSTOM_PLUGINS})
endif()


# headers installed in qgis_core target
