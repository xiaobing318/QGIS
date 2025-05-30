#ifndef CRC_H
#define CRC_H

#include "typedef.h"

/**
 * 计算 CRC32 
 * @param buff 数据
 * @param buff_len 数据长度
 * @return CRC32
 */
extern unsigned int calc_crc(const unsigned char *buff, uint32 buff_len);

/**
 * 计算 CRC32。这个比 calc_crc 更少的碰撞
 * @param buff 数据
 * @param buff_len 数据长度
 * @return CRC32
 */
extern unsigned int calc_crc32(const void *buff, uint32 buff_len);


#endif
