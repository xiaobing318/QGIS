/*
 * arc4.h
 *
 *  Created on: Jul 26, 2013
 *      Author: wuxqing
 */

#ifndef ARC4_H_
#define ARC4_H_

#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint8_t state[256];
	uint8_t x;
	uint8_t y;
} stream_state;

/* Encryption and decryption are symmetric */
#define stream_decrypt stream_encrypt

void stream_init(stream_state *self, const uint8_t *key, size_t key_size);
void stream_encrypt(stream_state *self, uint8_t *block, size_t size);


#endif /* ARC4_H_ */
