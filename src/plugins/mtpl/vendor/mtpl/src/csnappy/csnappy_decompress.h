//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 *
 *
 * @file
 *	 csnappy_decompress.h
 * @see
 *
 * @author
 * <li>wuxqing<br>
 * <li>wu.qingman@gmail.com<br>
 *
 * @date
 * 	Aug 8, 2013
 *
 * @version
 * 	V1.00
 *
 */

#ifndef CSNAPPY_DECOMPRESS_H_
#define CSNAPPY_DECOMPRESS_H_

int csnappy_decompress(
	const char *src,
	uint32_t src_len,
	char *dst,
	uint32_t dst_len);

#endif /* CSNAPPY_DECOMPRESS_H_ */
