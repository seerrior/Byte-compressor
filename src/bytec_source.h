#ifndef BYTEC_SOURCE_H
#define BYTEC_SOURCE_H

#include <stdio.h>
#include <stdint.h>

typedef struct {
    FILE          *file;
    const uint8_t *data;
    uint64_t       size;
    uint64_t       pos;
} bytec_source;

int  bytec_source_open_file(bytec_source *source, const char *path);
void bytec_source_open_memory(bytec_source *source, const void *data, size_t size);
void bytec_source_close(bytec_source *source);

int bytec_source_seek(bytec_source *source, uint64_t offset);
int bytec_source_read(bytec_source *source, void *dest, size_t len);

int bytec_source_u8(bytec_source *source, uint8_t *value);
int bytec_source_u16(bytec_source *source, uint16_t *value);
int bytec_source_u32(bytec_source *source, uint32_t *value);
int bytec_source_u64(bytec_source *source, uint64_t *value);

#endif
