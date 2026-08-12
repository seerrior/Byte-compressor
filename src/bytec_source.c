#include "bytec_source.h"

#include <string.h>

static int source_seek_file(FILE *file, int64_t offset, int origin)
{
#ifdef _WIN32
    return _fseeki64(file, offset, origin);
#else
    return fseeko(file, (off_t)offset, origin);
#endif
}

static int64_t source_tell_file(FILE *file)
{
#ifdef _WIN32
    return _ftelli64(file);
#else
    return (int64_t)ftello(file);
#endif
}

int bytec_source_open_file(bytec_source *source, const char *path)
{
    int64_t size;

    memset(source, 0, sizeof *source);
    source->file = fopen(path, "rb");
    if (source->file == NULL) {
        return -1;
    }
    if (source_seek_file(source->file, 0, SEEK_END) != 0) {
        fclose(source->file);
        source->file = NULL;
        return -1;
    }
    size = source_tell_file(source->file);
    if (size < 0) {
        fclose(source->file);
        source->file = NULL;
        return -1;
    }
    source->size = (uint64_t)size;
    return bytec_source_seek(source, 0);
}

void bytec_source_open_memory(bytec_source *source, const void *data, size_t size)
{
    memset(source, 0, sizeof *source);
    source->data = (const uint8_t *)data;
    source->size = (uint64_t)size;
}

void bytec_source_close(bytec_source *source)
{
    if (source->file != NULL) {
        fclose(source->file);
        source->file = NULL;
    }
    source->data = NULL;
    source->size = 0;
    source->pos = 0;
}

int bytec_source_seek(bytec_source *source, uint64_t offset)
{
    if (offset > source->size) {
        return -1;
    }
    if (source->file != NULL && source_seek_file(source->file, (int64_t)offset, SEEK_SET) != 0) {
        return -1;
    }
    source->pos = offset;
    return 0;
}

int bytec_source_read(bytec_source *source, void *dest, size_t len)
{
    if (len == 0) {
        return 0;
    }
    if (source->pos + len > source->size) {
        return -1;
    }
    if (source->file != NULL) {
        if (fread(dest, 1, len, source->file) != len) {
            return -1;
        }
    } else {
        memcpy(dest, source->data + source->pos, len);
    }
    source->pos += len;
    return 0;
}

int bytec_source_u8(bytec_source *source, uint8_t *value)
{
    return bytec_source_read(source, value, 1);
}

int bytec_source_u16(bytec_source *source, uint16_t *value)
{
    uint8_t buffer[2];

    if (bytec_source_read(source, buffer, 2) != 0) {
        return -1;
    }
    *value = (uint16_t)(buffer[0] | (buffer[1] << 8));
    return 0;
}

int bytec_source_u32(bytec_source *source, uint32_t *value)
{
    uint8_t  buffer[4];
    uint32_t result = 0;
    int      i;

    if (bytec_source_read(source, buffer, 4) != 0) {
        return -1;
    }
    for (i = 0; i < 4; i++) {
        result |= (uint32_t)buffer[i] << (8 * i);
    }
    *value = result;
    return 0;
}

int bytec_source_u64(bytec_source *source, uint64_t *value)
{
    uint8_t  buffer[8];
    uint64_t result = 0;
    int      i;

    if (bytec_source_read(source, buffer, 8) != 0) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        result |= (uint64_t)buffer[i] << (8 * i);
    }
    *value = result;
    return 0;
}
