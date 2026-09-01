/*
 *  arc4.c : Implementation for the Alleged-RC4 stream cipher
 *
 * Part of the Python Cryptography Toolkit
 *
 * Originally written by: A.M. Kuchling
 *
 * ===================================================================
 * The contents of this file are dedicated to the public domain.  To
 * the extent that dedication to the public domain is not available,
 * everyone is granted a worldwide, perpetual, royalty-free,
 * non-exclusive license to exercise all rights associated with the
 * contents of this file for any purpose whatsoever.
 * No rights are reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ===================================================================
 *
 */

#include "arc4.h"


void stream_encrypt(stream_state *self, uint8_t *block, size_t size) {
	size_t i;
	uint32_t x;
	uint32_t y;

	if (self == NULL || (block == NULL && size != 0u)) {
		return;
	}
	x = self->x;
	y = self->y;
	for (i = 0; i < size; ++i) {
		uint8_t temporary;
		uint32_t xor_index;
		x = (x + 1u) & 0xffu;
		y = (y + self->state[x]) & 0xffu;
		temporary = self->state[x];
		self->state[x] = self->state[y];
		self->state[y] = temporary;
		xor_index = (self->state[x] + self->state[y]) & 0xffu;
		block[i] = (uint8_t)(block[i] ^ self->state[xor_index]);
	}

	self->x = (uint8_t)x;
	self->y = (uint8_t)y;
}

void stream_init(stream_state *self, const uint8_t *key, size_t key_size) {
	uint32_t i;
	size_t key_index = 0u;
	uint32_t state_index = 0u;

	if (self == NULL || key == NULL || key_size == 0u) {
		return;
	}
	for (i = 0u; i < 256u; ++i) {
		self->state[i] = (uint8_t)i;
	}
	self->x = 0u;
	self->y = 0u;

	for (i = 0u; i < 256u; ++i) {
		uint8_t temporary;
		state_index =
			(key[key_index] + self->state[i] + state_index) & 0xffu;
		temporary = self->state[i];
		self->state[i] = self->state[state_index];
		self->state[state_index] = temporary;
		key_index = (key_index + 1u) % key_size;
	}
}
