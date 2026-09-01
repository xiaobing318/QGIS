#ifndef MTPL_MTPL_H
#define MTPL_MTPL_H

#include <mtpl/export.h>
#include <mtpl/status.h>
#include <mtpl/types.h>

#define MTPL_VERSION_MAJOR 2
#define MTPL_VERSION_MINOR 1
#define MTPL_VERSION_PATCH 0
#define MTPL_VERSION_STRING "2.1.0"

#ifdef __cplusplus
extern "C" {
#endif

MTPL_API const char *mtpl_version_string(void);
MTPL_API const char *mtpl_status_string(mtpl_status_t status);
MTPL_API void mtpl_buffer_release(mtpl_buffer_t *buffer);

#ifdef __cplusplus
}
#endif

#include <mtpl/ptp.h>
#include <mtpl/dtp.h>
#include <mtpl/vtp.h>
#include <mtpl/sfp.h>
#include <mtpl/package.h>

#endif
