//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 虚拟文件打包处理
 *
 * @file
 *	stp_packet_write.h
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

#ifndef SFP_PACKET_WRITE_H_
#define SFP_PACKET_WRITE_H_

#include "sfp_packet.h"



/**
 * sfp写处理
 * @param files 需要打包的文件
 * @param out_file 打包后的文件
 * @param is_encrypt 是否加密
 * @param private_key		私有key
 * @param private_key_len   私有key长度
 * @param dev_key			设备Key
 * @param dev_key_len		设备key长度
 * @param version 版本（１老版本　或　２新版本）
 * @return
 *		打包成功返回 TRUE
 *		打包失败返回 FALSE，错误原因通过get_last_error获取
 */
BOOL sfp_write(FILES *files, PCHAR out_file, const char * private_key, uint32 private_key_len, 
                      const char * dev_key, uint32 dev_key_len, BOOL is_encrypt, uint32 version);

/**
 * 列出指定目录下的所有文件
 * @param path		- 目录名
 * @param files		- 文件名
 * @param status    - 操作状态
 * @return
 *		返回 count
 */
int list_dir(char *path, FILES *files, uint32 *status);

#endif /* SFP_PACK_WRITE_H_ */
