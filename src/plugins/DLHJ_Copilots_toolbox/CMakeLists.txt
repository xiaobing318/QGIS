########################################################
# Files

set (CopilotsToolbox_SRCS
  PluginCopilotsToolbox.cpp
)

set (CopilotsToolbox_RCCS
  PluginCopilotsToolbox.qrc
)

########################################################
# Build

add_library (plugin_Copilots_toolbox MODULE ${CopilotsToolbox_SRCS} ${CopilotsToolbox_RCCS})

# require c++17
target_compile_features(plugin_Copilots_toolbox PRIVATE cxx_std_17)

# 包含头文件配置
target_include_directories(plugin_Copilots_toolbox SYSTEM PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)


# 链接库文件配置
if (BUILD_WITH_QT6)
  target_link_libraries(plugin_Copilots_toolbox PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    qgis_core
    qgis_gui
  )
else()
  target_link_libraries(plugin_Copilots_toolbox PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    qgis_core
    qgis_gui
  )
endif()

########################################################
# Install

install(TARGETS plugin_Copilots_toolbox
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})

