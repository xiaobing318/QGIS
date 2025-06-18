//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 一些定义
 *
 * @file
 *	 typedef.h
 * @see
 *
 * @author
 * <li>wuxqing<br>
 * <li>wu.qingman@gmail.com<br>
 *
 * @date
 * 	2013/4/23
 *
 * @version
 * 	V1.00
 *
 */


#ifndef LIB_TYPEDEF_H_
#define LIB_TYPEDEF_H_

#include <stdlib.h>
#include <stdint.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int BOOL;		    // 原先是 int16，windows下编译冲突
typedef uint64 HANDLE64;    // 原先是 HANDLE，windows下编译冲突

#define TRUE 1
#define FALSE 0

#define U32SIZE 4
#define U64SIZE 8

#define MAX_PATH 512

#define MAX_FILE_NAME 100

#ifndef byte
typedef unsigned char byte;
#endif

#ifndef PCHAR
typedef char * PCHAR;
#endif

#ifndef MIN
	#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
	#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif


#endif /* LIB_TYPEDEF_H_ */