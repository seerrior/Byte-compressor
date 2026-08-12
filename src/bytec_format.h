#ifndef BYTEC_FORMAT_H
#define BYTEC_FORMAT_H

#include <stdio.h>
#include <stdint.h>

#include "bytec.h"
#include "bytec_source.h"

#define BYTEC_MAGIC       "BYTECAR1"
#define BYTEC_MAGIC_SIZE  8
#define BYTEC_FOOTER_SIZE 16
#define BYTEC_BLOCK_SIZE  (1u << 20)

#define BYTEC_METHOD_STORE 0
#define BYTEC_METHOD_LZ    1

#define BYTEC_FLAG_FILE 0
#define BYTEC_FLAG_DIR  1

typedef struct {
    char    *path;
    uint64_t size;
    uint64_t packed_size;
    uint64_t offset;
    uint32_t crc;
    uint8_t  flags;
} bytec_record;

typedef int (*bytec_sink_fn)(const uint8_t *data, size_t len, void *user);

int     bytec_put_u8(FILE *file, uint8_t value);
int     bytec_put_u16(FILE *file, uint16_t value);
int     bytec_put_u32(FILE *file, uint32_t value);
int     bytec_put_u64(FILE *file, uint64_t value);
int64_t bytec_tell(FILE *file);

bytec_status bytec_load_records(bytec_source *source,
                                bytec_record **out_records, size_t *out_count);

void bytec_free_records(bytec_record *records, size_t count);

bytec_status bytec_decode_entry(bytec_source *source, const bytec_record *record,
                                bytec_sink_fn sink, void *user);

#endif
