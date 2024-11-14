//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * MTP写操作
 *
 * @file
 *	 mtp_packet_write.h
 * @see
 *
 * @author
 * <li>wuxqing<br>
 * <li>wu.qingman@gmail.com<br>
 *
 * @date
 * 	Apr 30, 2014
 *
 * @version
 * 	V1.00
 *
 */

#ifndef MTP_PACKET_WRITE_H_
#define MPT_PACKET_WRITE_H_
#include "mtp_packet.h"
#include "typedef.h"

/**
 * MTP创建
 * @param out_file			- 输出文件
 * @param tile_size			- 瓦片大小
 * @param mtp_id			- ID
 * @param metadata			- 属性数据
 * @param private_key   	- 私有KEY
 * @param private_key_len 	- 私有KEY长度，当长度等于0，表示不使用私有KEY
 * @param dev_key   		- 设备KEY
 * @param dev_key_len 		- 设备KEY长度
 * @param is_encrypt		- 数据包是否加密
 * @param status			- 状态（错误状态：0 - 无错误，其它参考status.h）
 * @return
 * 	成功 - 返回MTP句柄
 * 	失败 - 返回0，通过参数status可以了解到失败原因
 */
HANDLE64_PTP mtp_create(const char *out_file, int tile_size, const char *mtp_id, const char *metadata,
				  const char *private_key, uint32 private_key_len,
				  const char *dev_key, uint32 dev_key_len, BOOL is_encrypt, uint32 *status);

/**
 * MTP写打开
 * @param filename			- MTP文件名
 * @param readonly			- 是否只读模式
 * @param private_key   	- 私有KEY
 * @param private_key_len 	- 私有KEY长度，当长度等于0，表示不使用私有KEY
 * @param dev_key   		- 设备KEY
 * @param dev_key_len 		- 设备KEY长度
 * @param status			- 状态（错误状态：0 - 无错误，其它参考status.h）
 * @return
 * 	成功 - 返回MTP句柄
 * 	失败 - 返回0，通过参数status可以了解到失败原因
 */
HANDLE64_PTP mtp_wrtie_open(const char *filename, BOOL readonly, const char *private_key, uint32 private_key_len,
					  const char *dev_key, uint32 dev_key_len, uint32 *status);

/**
 *	往MTP文件中加入瓦片范围
 * @param handle		- MTP句柄
 * @param zoom			- Z
 * @param xmin			- 最小X
 * @param xmax			- 最大X
 * @param ymin			- 最小Y
 * @param ymax			- 最大Y
 * @param status		- 状态（错误状态：0 - 无错误，其它参考status.h）
 */
void add_tile_ranges(HANDLE64_PTP handle, uint32 zoom, uint32 xmin, uint32 xmax, uint32 ymin, uint32 ymax, uint32 *status);

/**
 * 往MTP中加入瓦片
 * @param handle		- MTP句柄
 * @param filename		- 瓦片文件
 * @param zoom			- z
 * @param x				- x
 * @param y				- y
 * @param is_encrypt    - 是否加密数据
 * @param status		- 状态（错误状态：0 - 无错误，其它参考status.h）
 */
void add_tile(HANDLE64_PTP handle, const char *filename, uint32 zoom, uint32 x, uint32 y, BOOL is_encrypt, uint32 *status);

/**
 * 往MTP中加入瓦片
 * @param handle		- MTP句柄
 * @param buff			- 瓦片数据
 * @param buff_size		- 瓦片数据长度
 * @param zoom			- z
 * @param x				- x
 * @param y				- y
 * @param is_encrypt    - 是否加密数据
 * @param status		- 状态（错误状态：0 - 无错误，其它参考status.h）
 */
void add_tile_with_data(HANDLE64_PTP handle, char *buff, uint32 buff_size, uint32 zoom, uint32 x, uint32 y, BOOL is_encrypt, uint32 *status);

/**
 * 是否存在瓦片
 * @param handle		- MTP句柄
 * @param zoom			- z
 * @param x				- x
 * @param y				- y
 * @param status		- 状态（错误状态：0 - 无错误，其它参考status.h）
 * @return
 * 	TRUE  - 指定区域已经存在瓦片
 * 	FALSE - 制定区域没有存在瓦片
 */
BOOL exist_tile(HANDLE64_PTP handle, uint32 zoom, uint32 x, uint32 y, uint32 *status);

/**
 * 关闭MTP
 * @param handle		- MTP句柄
 * @param status		- 状态（错误状态：0 - 无错误，其它参考status.h）
 */
void mtp_write_close(HANDLE64_PTP handle, uint32 *status);

/**
 * mtp合包
 * 		将 src_file 中的瓦片合并 到 dest_file中去
 * @param src_file			- 来源MTP文件
 * @param dest_file			- 目标MTP文件
 * @param private_key   	- 私有KEY
 * @param private_key_len 	- 私有KEY长度，当长度等于0，表示不使用私有KEY
 * @param dev_key   		- 设备KEY
 * @param dev_key_len 		- 设备KEY长度
 * @param status			- 状态（SUCCESS：表示合包成功 其他：参考status.h）
 */
void mtp_merge(const char *src_file, const char *dest_file, const char *private_key, uint32 private_key_len,
			   const char *dev_key, uint32 dev_key_len, uint32 *status);

#endif
