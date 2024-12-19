//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * MTP相关定义
 *
 * @file
 *	 mtp_packet.h
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

#ifndef MTP_PACKET_H
#define MTP_PACKET_H

#define PTP_FILE_ID "PTP"     	// 栅格地图瓦片 PTP
#define DTP_FILE_ID "DTP"     	// DEM瓦片 DTP
#define VTP_FILE_ID "VTP"     	// 矢量瓦片 VTP

#define MTP_FILE_ID_SIZE 3		    // 文件头长度
#define ENCRYPT_VERSION 0x2000	    // 加密数据的文件版本
#define NOENCRYPT_VERSION 0x1000	// 未加密数据的文件版本

#endif /* MTP_PACKET_H */
