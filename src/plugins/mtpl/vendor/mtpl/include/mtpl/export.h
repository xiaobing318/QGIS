#ifndef MTPL_EXPORT_H
#define MTPL_EXPORT_H

/*
 * QGIS vendors MTPL as a private static library.  Upstream normally creates
 * this header with CMake's GenerateExportHeader module.  Keeping the static
 * form in the snapshot avoids a generated include directory and, more
 * importantly, prevents MTPL symbols from becoming part of the QGIS plugin
 * ABI.
 */
#define MTPL_API
#define MTPL_NO_EXPORT

#endif
