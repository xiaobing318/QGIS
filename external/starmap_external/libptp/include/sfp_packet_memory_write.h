//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 虚拟文件打包处理,数据先保存在内存中
 *
 * @file
 *	 sfp_packet_memory_write.h
 * @see
 *
 * @author
 * <li>wuxqing<br>
 * <li>wu.qingman@gmail.com<br>
 *
 * @date
 * 	Jul 17, 2013
 *
 * @version
 * 	V1.00
 *
 */

#ifndef SFP_PACKET_MEMORY_WRITE_H_
#define SFP_PACKET_MEMORY_WRITE_H_

#include "typedef.h"

/**
 *	保存小文件数据到 内存中的数据包 中
 * @param handle	        虚拟文件包 句柄
 * @param name	            小文件名称
 * @param buff	            小文件数据
 * @param buff_size	        虚拟文件包 版本
 * @param private_key		私有key
 * @param private_key_len   私有key长度
 * @param dev_key			设备Key
 * @param dev_key_len		设备key长度
 * @param is_encrypt	    是否加密
 * @param version	        小文件ID 版本，取值：1、2
 * @param status	        操作的状态
 * @return
 * 	返回　－　虚拟文件包 句柄
 */
HANDLE64 sfp_write_memory(HANDLE64 handle, PCHAR name, PCHAR buff, uint32_t buff_size, const char *private_key, uint32 private_key_len, 
                         const char *dev_key, uint32 dev_key_len, BOOL is_encrypt, uint32 version, uint32 *status);

/**
 *	保存内存中的数据包到本地文件
 * @param handle	虚拟文件包 句柄
 * @param filename	本地文件名
 * @param version	虚拟文件包 版本
 * @param status	操作的状态
 * @return
 * 	返回　－　True、False
 */
BOOL sfp_write_memory_save_file(HANDLE64 handle, PCHAR filename, uint32 version, uint32 *status);

#endif /* SFP_PACKET_MEMORY_WRITE_H_ */
