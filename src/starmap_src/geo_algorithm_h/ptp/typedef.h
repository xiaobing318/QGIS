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

typedef int BOOL;		    
typedef uint64 HANDLE64_PTP;

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