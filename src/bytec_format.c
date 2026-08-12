#include "bytec_format.h"

int bytec_put_u8(FILE *file, uint8_t value)
{
    return fwrite(&value, 1, 1, file) == 1 ? 0 : -1;
}

int bytec_put_u16(FILE *file, uint16_t value)
{
    uint8_t buffer[2];

    buffer[0] = (uint8_t)(value & 0xff);
    buffer[1] = (uint8_t)((value >> 8) & 0xff);
    return fwrite(buffer, 1, 2, file) == 2 ? 0 : -1;
}

int bytec_put_u32(FILE *file, uint32_t value)
{
    uint8_t buffer[4];
    int     i;

    for (i = 0; i < 4; i++) {
        buffer[i] = (uint8_t)((value >> (8 * i)) & 0xff);
    }
    return fwrite(buffer, 1, 4, file) == 4 ? 0 : -1;
}

int bytec_put_u64(FILE *file, uint64_t value)
{
    uint8_t buffer[8];
    int     i;

    for (i = 0; i < 8; i++) {
        buffer[i] = (uint8_t)((value >> (8 * i)) & 0xff);
    }
    return fwrite(buffer, 1, 8, file) == 8 ? 0 : -1;
}

int64_t bytec_tell(FILE *file)
{
#ifdef _WIN32
    return _ftelli64(file);
#else
    return (int64_t)ftello(file);
#endif
}
