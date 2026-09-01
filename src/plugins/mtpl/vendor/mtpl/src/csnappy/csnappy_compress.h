//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 *
 *
 * @file
 *	 csnappy_compress.h
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

#ifndef CSNAPPY_COMPRESS_H_
#define CSNAPPY_COMPRESS_H_

void
csnappy_compress(
	const char *input,
	uint32_t input_length,
	char *compressed,
	uint32_t *compressed_length,
	void *working_memory,
	const int workmem_bytes_power_of_two);

#endif /* CSNAPPY_COMPRESS_H_ */
