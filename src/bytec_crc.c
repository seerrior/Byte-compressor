#include "bytec_crc.h"

static uint32_t crc_table[256];
static int      crc_ready = 0;

static void crc_build(void)
{
    uint32_t i;

    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        int      k;

        for (k = 0; k < 8; k++) {
            c = (c & 1u) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
    crc_ready = 1;
}

uint32_t bytec_crc32(uint32_t crc, const uint8_t *data, size_t len)
{
    size_t i;

    if (!crc_ready) {
        crc_build();
    }

    crc = ~crc;
    for (i = 0; i < len; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
    }
    return ~crc;
}
