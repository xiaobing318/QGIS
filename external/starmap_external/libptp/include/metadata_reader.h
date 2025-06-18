//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 产品元数据读取控制
 *
 * @file
 *	 metadata_reader.h
 * @see
 *
 * @author
 * <li>wuxqing<br>
 * <li>wu.qingman@gmail.com<br>
 *
 * @date
 * 	Oct 28, 2014
 *
 * @version
 * 	V1.00
 *
 */

#ifndef METADATA_READER_H_
#define METADATA_READER_H_
#include <typedef.h>

HANDLE64 metadata_load(const char *map_path, const char *map_id,
		const char* private_key, uint32 private_key_len,
		const char* dev_key, uint32 dev_key_len,
		uint32 *status);

char *get_first_single_map_guid(HANDLE64 handle);

uint32 metadata_unload(HANDLE64 handle);

#endif /* METADATA_READER_H_ */
