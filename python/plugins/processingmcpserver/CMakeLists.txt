file(GLOB PY_FILES *.py)
file(GLOB ROOT_FILES metadata.txt README.md requirements.txt)
file(GLOB EXAMPLE_FILES examples/*)
file(GLOB TEST_FILES tests/*.py)
file(GLOB TEST_DATA_FILES tests/data/*)

PLUGIN_INSTALL(processingmcpserver . ${PY_FILES} ${ROOT_FILES})
PLUGIN_INSTALL(processingmcpserver examples ${EXAMPLE_FILES})
PLUGIN_INSTALL(processingmcpserver tests ${TEST_FILES})
PLUGIN_INSTALL(processingmcpserver tests/data ${TEST_DATA_FILES})

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
