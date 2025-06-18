//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 虚拟文件读操作
 *
 * @file
 *	 sfp_packet_reader.h
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

#ifndef SFP_PACKET_READER_H_
#define SFP_PACKET_READER_H_

#include "sfp_packet.h"
#include "typedef.h"

/**
 *	文件打开
 * @param filename 			文件名
 * @param private_key		私有key
 * @param private_key_len   私有key长度
 * @param dev_key			设备Key
 * @param dev_key_len		设备key长度
 * @param status			打开结果
 * @return
 * 	如果打开成功就返回文件操作句柄
 * 	打开失败返回0
 */
HANDLE64 sfp_open(const char *filename, const char* private_key, uint32 private_key_len,
		const char* dev_key, uint32 dev_key_len, uint32 *status);

/**
 *	从虚拟文件包中获取一个文件元数据
 * @param pack_handle  		虚拟文件句柄
 * @param file_name 		需要获取文件名
 * @param status			打开状态
 * @return
 * 	打开成功就返回文件元数据句柄
 * 	打开失败返回NULL
 */
HANDLE64 sfp_open_file(HANDLE64 pack_handle, const char *file_name, uint32 *status);

/**
 *	 从虚拟文件包中获取文件列表
 * @param pack_handle  	虚拟文件id
 * @param files 		获取文件名后保存在这个参数中
 */
void sfp_get_file_list(HANDLE64 pack_handle, FILES *files);

/**
 *	关闭包文件
 * @param pack_handle	虚拟文件包id
 * @return
 * 	关闭成功返回 TRUE
 * 	关闭失败返回 FALSE,可以使用get_last_error函数获取错误原因
 */
BOOL sfp_close(HANDLE64 pack_handle);

/**
 *	从虚拟文件包中读取一个文件,在数据不用的时候要通过internal_free函数释放
 * @param file_handle  		文件元数据句柄
 * @param read_size			读取数据的长度
 * @param status			状态(通过这个可以知道是不是读取成功)
 * @return
 * 		成功 - 返回数据
 * 		失败 - 返回NULL
 */
byte * sfp_read(HANDLE64 file_handle, uint32 *read_size, uint32 *status);

/**
 *	从虚拟文件包中读取一个文件, 在数据不用的时候要通过internal_free函数释放
 *	完成sfp_open_file、sfp_get_file_size、sfp_read全部的工作
 * @param pack_handle  		虚拟文件句柄
 * @param file_name 		需要获取文件名
 * @param read_size			读取数据的长度
 * @param status		    读取的状态
 * @return
 * 		成功 - 返回数据
 * 		失败 - 返回NULL
 */
byte * sfp_get_file(HANDLE64 pack_handle, const char *file_name, uint32 *read_size, uint32 *status);

/**
 *	从虚拟文件包中读取一个文件,因为字符串最后一个字符必须是0,所以会多分配一个字节,在数据不用的时候要通过internal_free函数释放
 *	完成sfp_open_file、sfp_get_file_size、sfp_read全部的工作
 * @param pack_handle  		虚拟文件句柄
 * @param file_name 		需要获取文件名
 * @param read_size			读取数据的长度
 * @param status		    读取的状态
 * @return
 * 	读取成功返回 读取长度
 * 	读取失败返回 -1
 */
byte *sfp_get_file_to_str(HANDLE64 pack_handle, const char *file_name,  uint32 *read_size, uint32 *status);

/**
 *	获取文件大小
 * @param file_handle	- 文件元数据句柄
 * @return
 * 	文件长度
 */
uint32 sfp_get_file_size(HANDLE64 file_handle);


/**
 *	通过文件名获取文件大小
 * @param 	pack_handle	- 包句柄
 * @param	file_name	- 文件名
 * @return
 * 	文件长度
 */
uint32 sfp_get_file_size_by_name(HANDLE64 pack_handle, const char *file_name);


/**
 *	获取文件和路径
 * @param 	path		- 路径(如果是空串("")表示取根目录下的目录和文件)
 * @param	pakc_handle	- 包句柄
 * @return
 * 	文件和路径结果句柄
 */
HANDLE64 sfp_get_file_path_list(const char *path, HANDLE64 pakc_handle);


/**
 *	释放"获取文件和路径"分配的内存
 * @param 	list_handle	- 列表句柄
 */
void free_file_path_list_handle(HANDLE64 list_handle);


/**
 * 获取sfp文件列表信息,已json的格式返回,调用这个函数返回的结果要用"internal_free"释放
 * @param sfp_handle	- sfp文件句柄
 * @param result_size	- 结果长度
 * @param status		- 状态
 * @return
 * 		成功  -  返回JSON串，[{"文件名":[offset,size]},{"文件名":[offset,size]}]
 * 		失败  -  返回NULL,原因可以通过status参数了解
 */
byte *sfp_file_list_to_json(HANDLE64 sfp_handle, uint32 *result_size, uint32 *status);
/**
 *	获取列表中的文件个数
 * @param 	list_handle	- 列表句柄
 * @return
 * 	文件个数
 */
uint32 sfp_get_file_count(HANDLE64 list_handle);


/**
 *	获取列表中指定索引的文件名
 * @param 	list_handle	- 列表句柄
 * @param	index		- 索引
 * @return
 * 	文件名
 */
const char *sfp_get_file_name(HANDLE64 list_handle, uint32 index);

/**
 *	获取列表中指定索引的文件长度
 * @param list_handle	- 列表句柄
 * @param index			- 索引
 * @return
 * 	文件长度
 */
uint64 sfp_get_file_size_by_index(HANDLE64 list_handle, uint32 index);

/**
 *	获取列表中的目录个数
 * @param list_handle	- 列表句柄
 * @return
 * 	目录个数
 */
uint32 sfp_get_path_count(HANDLE64 list_handle);

/**
 *	获取列表中指定索引的目录名
 * @param list_handle	- 列表句柄
 * @param index			- 索引
 * @return
 * 	目录名
 */
const char * sfp_get_path_name(HANDLE64 list_handle, uint32 index);


/**
 *	获取版本号
 * @param pack_handle	虚拟文件包id
 * @return
 * 	返回　－　版本号
 */
uint32 sfp_get_version(HANDLE64 pack_handle);

#endif
