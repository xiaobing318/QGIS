//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 瓦片数据读取
 *
 * @file
 *	 mtp_packet_reader.h
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

#ifndef MTP_PACKET_READER_H
#define MTP_PACKET_READER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "typedef.h"
#include "mtp_packet.h"

/**
 * 打开一个map数据包（MTP：ptp、dtp）
 * @param filename 		    数据包文件名
 * @param private_key   	私有KEY
 * @param private_key_len 	私有KEY长度，当长度等于0，表示不使用私有KEY
 * @param dev_key   		设备KEY
 * @param dev_key_len 		设备KEY长度
 * @param status            操作的状态
 * @return
 * 		如果打开成功就返回文件操作句柄
 * 		打开失败返回0，查看 status 值
 */
HANDLE64_PTP mtp_open(const char *filename, const char *private_key, uint32 private_key_len,
				const char *dev_key, uint32 dev_key_len, uint32 *status);

/**
 *	关闭文件
 * @param fd 文件句柄
 */
void mtp_close(HANDLE64_PTP fd);

/**
 × 获取一个瓦片
 * @param fd 				文件句柄
 * @param zoom    			zoom level
 * @param x_index 			瓦片x序号
 * @param y_index 			瓦片y序号
 * @param tile_data_len    	返回瓦片数据大小
 * @param is_decrypt    	是否解密数据
 * @param status            操作的状态
 * @return
 * 		获取成功，返回数据
 * 		获取失败，返回NULL，查看 status
 */
byte *mtp_get_tile(HANDLE64_PTP fd, uint32 zoom, uint32 x_index, uint32 y_index, uint32 *tile_data_len, BOOL is_decrypt, uint32 *status);


/**
 * 获取 dtp 33X33 瓦片
 * @param fd 				文件句柄
 * @param zoom    			zoom level
 * @param x_index 			瓦片x序号
 * @param y_index 			瓦片y序号
 * @param tile_data_len    	返回瓦片数据大小
 * @param is_decrypt    	是否解密数据
 * @param status            操作的状态
 * @return
 * 		获取成功，返回数据
 * 		获取失败，返回NULL，查看 status
 */
byte *dtp_get_tile33(HANDLE64_PTP fd, uint32 zoom, uint32 x_index, uint32 y_index, uint32 *tile_data_len, BOOL is_decrypt, uint32 *status);


/**
 * 获取 dtp 32X32 瓦片
 * @param fd 				文件句柄
 * @param zoom    			zoom level
 * @param x_index 			瓦片x序号
 * @param y_index 			瓦片y序号
 * @param tile_data_len    	返回瓦片数据大小
 * @param is_decrypt    	是否解密数据
 * @param status            操作的状态
 * @return
 * 		获取成功，返回数据
 * 		获取失败，返回NULL，查看 status
 */
byte *dtp_get_tile32(HANDLE64_PTP fd, uint32 zoom, uint32 x_index, uint32 y_index, uint32 *tile_data_len, BOOL is_decrypt, uint32 *status);


/**
 * 获取 dtp 129X129 瓦片
 * @param fd 				文件句柄
 * @param zoom    			zoom level
 * @param x_index 			瓦片x序号
 * @param y_index 			瓦片y序号
 * @param tile_data_len    	返回瓦片数据大小
 * @param is_decrypt    	是否解密数据
 * @param status            操作的状态
 * @return
 * 		获取成功，返回数据
 * 		获取失败，返回NULL，查看 status
 */
byte *dtp_get_tile129(HANDLE64_PTP fd, uint32 zoom, uint32 x_index, uint32 y_index, uint32 *tile_data_len, BOOL is_decrypt, uint32 *status);


/**
 * 获取 dtp128X128瓦片
 * @param fd 				文件句柄
 * @param zoom    			zoom level
 * @param x_index 			瓦片x序号
 * @param y_index 			瓦片y序号
 * @param tile_data_len    	返回瓦片数据大小
 * @param is_decrypt    	是否解密数据
 * @param status            操作的状态
 * @return
 * 		获取成功，返回数据
 * 		获取失败，返回NULL，查看 status
 */
byte *dtp_get_tile128(HANDLE64_PTP fd, uint32 zoom, uint32 x_index, uint32 y_index, uint32 *tile_data_len, BOOL is_decrypt, uint32 *status);


/**
 * 获取MTP数据包的元数据
 * @param fd				文件句柄
 * @param data_len			返回元数据长度
 * @param status			操作的状态
 * @return
 * 		获取成功返回的数据大小
 * 		获取失败返回0，可以使用get_last_error函数获取错误原因
 */
byte *mtp_get_metadata(HANDLE64_PTP fd, uint32 *data_len, uint32 *status);

/**
 * 获取一个tile的边界
 *
 * @param fd		操作的句柄
 * @param zoom		z
 * @param x_index   x
 * @param y_index	y
 * @param status	错误状态：0 - 无错误，其它参考status.h
 * @return
 * 	byte 字符，定义：
 *	        top
 *	 left cur_tile  right
 *	       bottom
 *   每个边2个bit表示（高位是1表示是数据包中最靠边的瓦片，低位是1表示是瓦片区域的边）
 *	 00   00  00    00
 *	 left top right bottom
 */
byte mtp_check_tile_edge(HANDLE64_PTP fd, uint32 zoom, uint32 x_index, uint32 y_index, uint32 *status);

/**
 * 获取MTP版本
 * @param fd        文件句柄
 * @return
 *  　版本号 (0x1000: 表示不加密　0x2000: 表示加密)
 */
uint32 mtp_get_version(HANDLE64_PTP fd);

/**
 * 获取MTP瓦片尺寸
 * @param fd        文件句柄
 * @return
 *  　瓦片尺寸
 */
uint32 mtp_get_tile_size(HANDLE64_PTP fd);

/**
 * 获取MTP文件ID
 * @param fd        文件句柄
 * @return
 *  　文件ID
 */
char * mtp_get_file_id(HANDLE64_PTP fd);

/**
 * 裁剪数据
 * @param data        	原始数据
 * @param data_size     原始数据大小，处理后是新数据大小
 * @param data_width    原始数据宽度
 * @param data_height   原始数据高度
 * @param data_byte_num 原始数据每个数据点的大小（byte）
 * @param new_width     新宽度
 * @param new_height    新高度
 * @return
 *  　裁剪后的数据
 */
byte *crop_specify_width_height(const byte *data, uint32 *data_size, uint32 data_width, uint32 data_height, uint32 data_byte_num,
		uint32 new_width, uint32 new_height);


#endif
