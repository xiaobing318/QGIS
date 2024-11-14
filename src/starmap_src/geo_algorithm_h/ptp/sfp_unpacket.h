//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 解压SFP文件
 *
 * @file
 *	 sfp_unpacket.h
 * @see
 *
 * @author
 * <li>wuxqing<br>
 * <li>wu.qingman@gmail.com<br>
 *
 * @date
 * 	Jul 23, 2013
 *
 * @version
 * 	V1.00
 *
 */

#ifndef SFP_UNPACKET_H_
#define SFP_UNPACKET_H_

#include "typedef.h"

/**
 *	解包 sfp 到指定文件夹 中
 * @param sfp_filename	    虚拟文件包 文件名
 * @param out_path	        小文件输出路径
 * @param private_key		私有key
 * @param private_key_len   私有key长度
 * @param dev_key			设备Key
 * @param dev_key_len		设备key长度
 * @param status	        操作的状态
 * @return
 * 	返回　－　小文件个数
 */
int32 sfp_unpack(const char *sfp_filename, const char *out_path, 
                        const char * private_key, uint32 private_key_len, 
                        const char * dev_key, uint32 dev_key_len, uint32 *status);

#endif /* SFP_UNPACKET_H_ */
