#ifndef BYTEC_CRC_H
#define BYTEC_CRC_H

#include <stddef.h>
#include <stdint.h>

uint32_t bytec_crc32(uint32_t crc, const uint8_t *data, size_t len);

#endif
