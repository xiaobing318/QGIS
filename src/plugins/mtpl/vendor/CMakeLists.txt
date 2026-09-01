set(QGIS_MTPL_VENDOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/mtpl")

set(QGIS_MTPL_CORE_SOURCES
  ${QGIS_MTPL_VENDOR_ROOT}/src/common/mtpl.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/common/package.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/internal/crypto.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/internal/file_io.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/tile/tile_core.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/tile/ptp.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/tile/dtp.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/tile/vtp.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/sfp/sfp.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/security/arc4.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/coding/base64_decode.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/csnappy/csnappy_compress.c
  ${QGIS_MTPL_VENDOR_ROOT}/src/csnappy/csnappy_decompress.c
)

add_library(qgis_mtpl_core STATIC ${QGIS_MTPL_CORE_SOURCES})

set_target_properties(qgis_mtpl_core PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS NO
  POSITION_INDEPENDENT_CODE YES
  C_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(qgis_mtpl_core
  PUBLIC
    ${QGIS_MTPL_VENDOR_ROOT}/include
  PRIVATE
    ${QGIS_MTPL_VENDOR_ROOT}/src/common
    ${QGIS_MTPL_VENDOR_ROOT}/src/internal
    ${QGIS_MTPL_VENDOR_ROOT}/src/tile
    ${QGIS_MTPL_VENDOR_ROOT}/src/sfp
    ${QGIS_MTPL_VENDOR_ROOT}/src/security
    ${QGIS_MTPL_VENDOR_ROOT}/src/coding
    ${QGIS_MTPL_VENDOR_ROOT}/src/csnappy
)

target_compile_definitions(qgis_mtpl_core
  PUBLIC MTPL_STATIC_DEFINE
)

if(MSVC)
  target_compile_definitions(qgis_mtpl_core PRIVATE
    _CRT_NONSTDC_NO_WARNINGS
    _CRT_SECURE_NO_WARNINGS
  )
  set_source_files_properties(
    ${QGIS_MTPL_VENDOR_ROOT}/src/csnappy/csnappy_compress.c
    ${QGIS_MTPL_VENDOR_ROOT}/src/csnappy/csnappy_decompress.c
    PROPERTIES COMPILE_OPTIONS "/wd4127;/wd4244;/wd4334;/wd5105"
  )
else()
  target_compile_definitions(qgis_mtpl_core PRIVATE
    _FILE_OFFSET_BITS=64
    _LARGEFILE_SOURCE
  )
endif()

target_link_libraries(qgis_mtpl_core PUBLIC ZLIB::ZLIB)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(qgis_mtpl_core PUBLIC m)
endif()
