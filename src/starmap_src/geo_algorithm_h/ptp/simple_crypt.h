//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 数据加密解密
 *
 * @file
 *	 qi_data_decrypt.h
 * @see
 *	 qi_data_decrypt.c
 * @author
 * <li>wuxqing<br>
 * <li>wu.qingman@gmail.com<br>
 *
 * @date
 * 	May 30, 2013
 *
 * @version
 * 	V1.00
 *
 */


#ifndef SIMPLE_CRYPT_H_
#define SIMPLE_CRYPT_H_

#include "typedef.h"

#define ENCRYPT_COMPRESS_METHOD_NONE 		    1  // 不压缩数据，直接拷贝
#define ENCRYPT_COMPRESS_METHOD_SNAPPY 			2
#define ENCRYPT_COMPRESS_METHOD_ZLIB 			3


/**
 * 根据private_key、dev_key产生解密的decrypt_key
 * 内部会调用 build_decrypt_key
 *
 * @param private_key 		用于产生decrypt_key的私有key（base64编码过的）
 * @param private_key_len 	private_key长度
 * @param dev_key 			用于产生decrypt_key的设备key
 * @param dev_key_len 		dev_key长度
 * @param decrypt_key_len 	decrypt_key长度
 * @return decrypt_key
 */
char * make_decrypt_key(const char * private_key, uint32 private_key_len, const char * dev_key, uint32 dev_key_len, uint32 *decrypt_key_len);

/**
 * 对数据加密，支持压缩数据
 *
 * @param decrypt_key 		用于加密数据的 decrypt_key
 * @param decrypt_key_len 	decrypt_key 长度
 * @param buff 				待加密数据
 * @param buff_len  		待加密数据长度
 * @param compress_method  	压缩类型
 * @param encrypt_len  		数据加密后的长度
 * @param encrypt_status  	执行状态
 * @return 加密后的数据
 */
byte *encrypt_fast(const char * decrypt_key, uint32 decrypt_key_len, const char* buff,
		uint32 buff_len, uint16 compress_method, uint32 *encrypt_len, uint32 *encrypt_status);

/**
 * 对数据进行解密
 *
 * @param decrypt_key 		用于解密数据的 decrypt_key
 * @param decrypt_key_len 	decrypt_key 长度
 * @param buff 				待加密数据
 * @param buff_len  		待加密数据长度
 * @param decrypt_len  		数据解密后的长度
 * @param decrypt_status  	执行状态
 * @return 解密后的数据
 */
byte *decrypt_fast(const char * decrypt_key, uint32 decrypt_key_len, byte* buff,
		uint32 buff_len, uint32 *decrypt_len, uint32 *decrypt_status);

#endif
