set(mtpl_test_common_sources
  mtpltestfixtures.cpp
  mtpltestfixtures.h
  mtpltestutils.h
)

function(add_mtpl_test test_name test_source)
  add_executable(${test_name}
    ${test_source}
    ${mtpl_test_common_sources}
    ${util_SRCS}
  )
  set_target_properties(${test_name} PROPERTIES AUTORCC TRUE)
  target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${test_name} PRIVATE
    qgis_mtpl_plugin_support
    qgis_test
  )
  if(MSVC)
    target_compile_options(${test_name} PRIVATE /utf-8)
  endif()

  add_test(
    NAME ${test_name}
    COMMAND $<TARGET_FILE:${test_name}> -maxwarnings 10000
  )
  set_tests_properties(${test_name} PROPERTIES
    FIXTURES_REQUIRED SOURCETREE
    LABELS mtpl
    RUN_SERIAL TRUE
    TIMEOUT 300
  )
endfunction()

add_mtpl_test(test_mtpl_package test_mtpl_package.cpp)
add_mtpl_test(test_mtpl_operation test_mtpl_operation.cpp)
add_mtpl_test(test_mtpl_widget test_mtpl_widget.cpp)
add_mtpl_test(test_mtpl_tileset test_mtpl_tileset.cpp)
add_mtpl_test(test_mtpl_layer test_mtpl_layer.cpp)
if(WITH_AUTH)
  add_mtpl_test(test_mtpl_credentials test_mtpl_credentials.cpp)
  if(TARGET authmethod_mtpl)
    add_dependencies(test_mtpl_credentials authmethod_mtpl)
  endif()
endif()

# Explicit manual corpus generation only. Never register this writer with CTest.
add_executable(mtpl_testdata_generator
  mtpl_testdata_generator.cpp
  mtpltestfixtures.cpp
  mtpltestfixtures.h
)
target_include_directories(mtpl_testdata_generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mtpl_testdata_generator PRIVATE qgis_mtpl_plugin_support)
if(MSVC)
  target_compile_options(mtpl_testdata_generator PRIVATE /utf-8)
endif()
