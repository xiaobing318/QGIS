//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 虚拟文件打包处理
 *
 * @file
 *	sfp_packet.h
 * @see
 *
 * @author
 * <li>wuxqing<br>
 * <li>wu.qingman@gmail.com<br>
 *
 * @date
 * 	Apr 22, 2013
 *
 * @version
 * 	V1.00
 *
 */

#ifndef SFP_PACKET_H_
#define SFP_PACKET_H_

#include <stdio.h>
#include <stdlib.h>
#include "typedef.h"
#include "uthash.h"

#define MAX_FILES_COUNT 100000	// 一个包内最大支持10万
#define SFP_FILE_ID "SFP"     	// 文件ID
// todo: 暂时开始保留１的版本
//#define SFP_FILE_VERSION 2      // 文件版本
#define SFP_FILE_VERSION 1      // 文件版本
#define SFP_FILE_ID_SIZE 4		// 文件头长度
#define BUFF_SIZE (1024 * 1024) // 预分配缓存

// 需要打包的文件
typedef struct FILES {
	uint32 count;									// 文件数
	char root[MAX_PATH];							// 根目录
	uint64 size[MAX_FILES_COUNT];					// 每个文件长度
	char  name[MAX_FILES_COUNT][MAX_PATH];			// 每个文件的路径
} FILES;

typedef struct PATH_FILES {
	uint32 path_count;								// 目录数
	char   *paths;									// 所有路径
	uint32 file_count;								// 文件个数
	uint64 *file_size;								// 每个文件长度
	char   *file_name;								// 所有文件
} PATH_FILES;

typedef struct SFP_MEMORY_DATA {
	char name[MAX_PATH];							// 文件名
	uint64 id;
	uint32 size;				// 数据长度
	uint32 crypt_size; 			// 每个文件加密后的长度（0：表示没有加密 >0：表示加密）
	PCHAR data;					// 数据
	struct SFP_MEMORY_DATA *next;
}SFP_MEMORY_DATA_NODE;

typedef struct FILE_INFO {
	uint64 id;					// id
	uint32 size;				// 数据长度
	uint32 crypt_size; 			// 每个文件加密后的长度（0：表示没有加密 >0：表示加密）
	uint64 offset;				// 数据偏移量
	char name[MAX_PATH];		// 文件路径
	FILE *fp;
	void *pack_info;
	UT_hash_handle hh;
} FILE_INFO;

typedef struct PACK_INFO {
	uint64 id;					// id
	uint32 reserve;				// 保留
	uint32 file_version;		// 文件版本
	FILE_INFO *files;			// 所有文件哈希
	FILE *fp;					// 文件句柄
	char *decrypt_key;
	uint32 decrypt_key_len;
	uint32 file_count;			// 包内有多少个文件
} PACK_INFO;



#endif /* SFP_PACK_H_ */
