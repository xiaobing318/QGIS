file(GLOB TEST_PY_FILES *.py)
file(GLOB TEST_DATA_FILES data/*)
file(GLOB TEST_METADATA_FILES requirements.txt metadata.txt)
file(GLOB TEST_DOC_FILES README.md)

PLUGIN_INSTALL(processingmcpserver tests ${TEST_PY_FILES})
PLUGIN_INSTALL(processingmcpserver tests/data ${TEST_DATA_FILES})
PLUGIN_INSTALL(processingmcpserver tests/metadata ${TEST_METADATA_FILES})
PLUGIN_INSTALL(processingmcpserver tests/docs ${TEST_DOC_FILES})

if(ENABLE_TESTS)
  include(UsePythonTest)
  ADD_PYTHON_TEST(ProcessingMCPServer processing_mcp_server_tests.py)
endif()
