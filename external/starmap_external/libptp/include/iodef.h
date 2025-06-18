#ifndef _IODEF_H_
#define _IODEF_H_

#ifdef LINUX
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define tell(fd) lseek(fd, 0, SEEK_CUR)
#define FILE_OPEN open
#define FILE_LSEEK lseek64
#define FILE_READ read
#define FILE_WRITE write
#define FILE_CLOSE close
#define FILE_TELL tell
#define FILE_OPEN_O_RDONLY O_RDONLY		// 只读模式
#define FILE_OPEN_O_WONLY O_WRONLY			// 只写模式
#define FILE_OPEN_O_APPEND O_APPEND		// 每次写操作都写入文件的尾部
#define FILE_OPEN_O_RDWR	O_RDWR			// 读写模式
#define FILE_OPEN_O_CREAT O_CREAT			// 文件不存在就创建
#define FILE_OPEN_O_TRUNC	O_TRUNC			// 在文件存在就将文件长度截断为0
#define FILE_OPEN_O_BINARY	0	// 二进制文件
#define FILE_OPEN_S_IREAD	S_IREAD			// 允许读
#define FILE_OPEN_S_IWRITE S_IWRITE		// 允许写
#define FILE_OPEN_S_IEXEC S_IEXEC			// 允许执行
#define FILE_OPEN_S_RWE_PERMISSION S_IREAD|S_IWRITE|S_IEXEC
#endif

#ifdef WINDOWS
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define tell(fd) lseek(fd, 0, SEEK_CUR)
#define FILE_OPEN open
#define FILE_LSEEK lseek64
#define FILE_READ read
#define FILE_WRITE write
#define FILE_CLOSE close
#define FILE_TELL tell
#define FILE_OPEN_O_RDONLY O_RDONLY		// 只读模式
#define FILE_OPEN_O_WONLY O_WRONLY			// 只写模式
#define FILE_OPEN_O_APPEND O_APPEND		// 每次写操作都写入文件的尾部
#define FILE_OPEN_O_RDWR	O_RDWR			// 读写模式
#define FILE_OPEN_O_CREAT O_CREAT			// 文件不存在就创建
#define FILE_OPEN_O_TRUNC	O_TRUNC			// 在文件存在就将文件长度截断为0
#define FILE_OPEN_O_BINARY	0	// 二进制文件
#define FILE_OPEN_S_IREAD	S_IREAD			// 允许读
#define FILE_OPEN_S_IWRITE S_IWRITE		// 允许写
#define FILE_OPEN_S_IEXEC S_IEXEC			// 允许执行
#define FILE_OPEN_S_RWE_PERMISSION S_IREAD|S_IWRITE|S_IEXEC
#endif

#ifdef ANROID
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#define FILE_OPEN _open
#define FILE_LSEEK _lseeki64
#define FILE_READ _read
#define FILE_WRITE _write
#define FILE_CLOSE _close
#define FILE_TELL _tell
#define FILE_OPEN_O_RDONLY _O_RDONLY		// 只读模式
#define FILE_OPEN_O_WONLY _O_WRONLY		// 只写模式
#define FILE_OPEN_O_APPEND _O_APPEND		// 每次写操作都写入文件的尾部
#define FILE_OPEN_O_RDWR	_O_RDWR			// 读写模式
#define FILE_OPEN_O_CREAT _O_CREAT			// 文件不存在就创建
#define FILE_OPEN_O_TRUNC	_O_TRUNC			// 在文件存在就将文件长度截断为0
#define FILE_OPEN_O_BINARY		_O_BINARY // 二进制文件
#define FILE_OPEN_S_IREAD	_S_IREAD			// 允许读
#define FILE_OPEN_S_IWRITE _S_IWRITE		// 允许写
#define FILE_OPEN_S_IEXEC _S_IEXEC			// 允许执行
#define FILE_OPEN_S_RWE_PERMISSION _S_IREAD|_S_IWRITE|_S_IEXEC 
#endif

#endif
