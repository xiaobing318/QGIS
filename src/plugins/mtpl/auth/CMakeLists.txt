set(AUTH_MTPL_SRCS
  core/qgsauthmtplmethod.cpp
)

set(AUTH_MTPL_HDRS
  core/qgsauthmtplmethod.h
)

if(WITH_GUI)
  list(APPEND AUTH_MTPL_SRCS
    gui/qgsauthmtpledit.cpp
  )
  list(APPEND AUTH_MTPL_HDRS
    gui/qgsauthmtpledit.h
  )
endif()

# Desktop builds discover authentication methods as separate modules.
add_library(authmethod_mtpl MODULE ${AUTH_MTPL_SRCS} ${AUTH_MTPL_HDRS})
target_include_directories(authmethod_mtpl PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/core
)
target_link_libraries(authmethod_mtpl PRIVATE qgis_core)

if(WITH_GUI)
  target_include_directories(authmethod_mtpl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gui
  )
  target_link_libraries(authmethod_mtpl PRIVATE qgis_gui Qt::Widgets)
  add_dependencies(authmethod_mtpl ui)
endif()

target_compile_definitions(authmethod_mtpl PRIVATE "-DQT_NO_FOREACH")
if(MSVC)
  target_compile_options(authmethod_mtpl PRIVATE /utf-8)
endif()

install(TARGETS authmethod_mtpl
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR}
)
