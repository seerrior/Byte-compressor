#ifndef BYTEC_LZ_H
#define BYTEC_LZ_H

#include <stddef.h>
#include <stdint.h>

size_t bytec_lz_bound(size_t input_size);

size_t bytec_lz_compress(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_cap);

int bytec_lz_decompress(const uint8_t *in, size_t in_len,
                        uint8_t *out, size_t out_len);

#endif
