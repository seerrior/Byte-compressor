#include "bytec_lz.h"

#include <stdlib.h>
#include <string.h>

#define LZ_WINDOW      65536u
#define LZ_HASH_BITS   16
#define LZ_HASH_SIZE   (1u << LZ_HASH_BITS)
#define LZ_MIN_MATCH   4
#define LZ_MAX_MATCH   258
#define LZ_MAX_CHAIN   64

static uint32_t lz_hash(const uint8_t *p)
{
    uint32_t v = (uint32_t)p[0]
               | ((uint32_t)p[1] << 8)
               | ((uint32_t)p[2] << 16)
               | ((uint32_t)p[3] << 24);
    return (v * 2654435761u) >> (32 - LZ_HASH_BITS);
}

size_t bytec_lz_bound(size_t input_size)
{
    return input_size + (input_size / 8) + 64;
}

size_t bytec_lz_compress(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_cap)
{
    int32_t *head;
    int32_t *prev;
    size_t   pos = 0;
    size_t   op = 0;
    size_t   flag_pos = 0;
    unsigned flag_bit = 0;
    uint8_t  flags = 0;

    if (in_len == 0 || in == NULL || out == NULL) {
        return 0;
    }

    head = (int32_t *)malloc(LZ_HASH_SIZE * sizeof *head);
    prev = (int32_t *)malloc(in_len * sizeof *prev);
    if (head == NULL || prev == NULL) {
        free(head);
        free(prev);
        return 0;
    }
    memset(head, 0xff, LZ_HASH_SIZE * sizeof *head);

    while (pos < in_len) {
        size_t best_len = 0;
        size_t best_dist = 0;

        if (pos + LZ_MIN_MATCH <= in_len) {
            uint32_t h = lz_hash(in + pos);
            int32_t  candidate = head[h];
            int      chain = LZ_MAX_CHAIN;

            prev[pos] = candidate;
            head[h] = (int32_t)pos;

            while (candidate >= 0 && chain-- > 0) {
                size_t dist = pos - (size_t)candidate;
                size_t limit = in_len - pos;
                size_t len = 0;

                if (dist > LZ_WINDOW) {
                    break;
                }
                if (limit > LZ_MAX_MATCH) {
                    limit = LZ_MAX_MATCH;
                }
                while (len < limit && in[(size_t)candidate + len] == in[pos + len]) {
                    len++;
                }
                if (len > best_len) {
                    best_len = len;
                    best_dist = dist;
                    if (len == LZ_MAX_MATCH) {
                        break;
                    }
                }
                candidate = prev[candidate];
            }
        }

        if (flag_bit == 0) {
            if (op + 1 > out_cap) {
                goto overflow;
            }
            flag_pos = op++;
            flags = 0;
        }

        if (best_len >= LZ_MIN_MATCH) {
            size_t i;

            if (op + 3 > out_cap) {
                goto overflow;
            }
            flags |= (uint8_t)(1u << flag_bit);
            out[op++] = (uint8_t)((best_dist - 1) & 0xff);
            out[op++] = (uint8_t)(((best_dist - 1) >> 8) & 0xff);
            out[op++] = (uint8_t)(best_len - LZ_MIN_MATCH);

            for (i = 1; i < best_len; i++) {
                size_t at = pos + i;
                if (at + LZ_MIN_MATCH <= in_len) {
                    uint32_t h = lz_hash(in + at);
                    prev[at] = head[h];
                    head[h] = (int32_t)at;
                }
            }
            pos += best_len;
        } else {
            if (op + 1 > out_cap) {
                goto overflow;
            }
            out[op++] = in[pos++];
        }

        out[flag_pos] = flags;
        flag_bit = (flag_bit + 1) & 7;
    }

    free(head);
    free(prev);
    return op;

overflow:
    free(head);
    free(prev);
    return 0;
}

int bytec_lz_decompress(const uint8_t *in, size_t in_len,
                        uint8_t *out, size_t out_len)
{
    size_t   ip = 0;
    size_t   op = 0;
    unsigned bit = 8;
    uint8_t  flags = 0;

    if (out_len == 0) {
        return 0;
    }
    if (in == NULL || out == NULL) {
        return -1;
    }

    while (op < out_len) {
        if (bit == 8) {
            if (ip >= in_len) {
                return -1;
            }
            flags = in[ip++];
            bit = 0;
        }

        if (flags & (1u << bit)) {
            size_t dist;
            size_t len;
            size_t i;

            if (ip + 3 > in_len) {
                return -1;
            }
            dist = (size_t)in[ip] | ((size_t)in[ip + 1] << 8);
            dist += 1;
            len = (size_t)in[ip + 2] + LZ_MIN_MATCH;
            ip += 3;

            if (dist > op || op + len > out_len) {
                return -1;
            }
            for (i = 0; i < len; i++) {
                out[op + i] = out[op - dist + i];
            }
            op += len;
        } else {
            if (ip >= in_len) {
                return -1;
            }
            out[op++] = in[ip++];
        }
        bit++;
    }

    return 0;
}
