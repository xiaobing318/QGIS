#pragma once
#ifndef STAREARTH_CONFIG_H_
#define STAREARTH_CONFIG_H_

#if defined __GNUC__
#   define COMPILER_GCC  1
#endif

#if defined _MSC_VER
#   define COMPILER_MSVC  1
#endif

#if _MSC_VER >= 1600	
#   define _HAVE_STDINT_H  1
#endif

#if defined _WIN32 || defined WIN32 || defined __NT__ || defined __WIN32__
#   define OS_FAMILY_WINDOWS  1
#endif

#if defined linux || defined __linux__
#   define OS_FAMILY_LINUX  1
#   define OS_FAMILY_UNIX   1
#endif

#if defined(OS_FAMILY_WINDOWS)
#   define EXPORT_DECL  __declspec(dllexport)
#   define IMPORT_DECL  __declspec(dllimport)
#   define EXPORT_FUNC
#elif defined(OS_FAMILY_UNIX) && defined(COMPILER_GCC)
#   define EXPORT_DECL  __attribute__ ((visibility("default")))
#   define IMPORT_DECL  __attribute__ ((visibility("default")))
#   define EXPORT_FUNC  __attribute__ ((visibility("default")))
#else
#   define EXPORT_DECL
#   define IMPORT_DECL
#   define EXPORT_FUNC
#endif


#ifdef DLLPROJECT_EXPORT
	#define SE_API EXPORT_DECL
#else
	#define SE_API IMPORT_DECL
#endif // DLLPROJECT_EXPORT


#endif // STAREARTH_CONFIG_H_