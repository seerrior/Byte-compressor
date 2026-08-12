#include <stdlib.h>
#include <string.h>

#include "bytec_crc.h"
#include "bytec_format.h"
#include "bytec_lz.h"

void bytec_free_records(bytec_record *records, size_t count)
{
    size_t i;

    if (records == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        free(records[i].path);
    }
    free(records);
}

bytec_status bytec_load_records(bytec_source *source,
                                bytec_record **out_records, size_t *out_count)
{
    bytec_record *records = NULL;
    char          magic[BYTEC_MAGIC_SIZE];
    char          tail[BYTEC_MAGIC_SIZE];
    uint64_t      index_offset = 0;
    uint64_t      count = 0;
    uint64_t      i;

    if (source->size < BYTEC_MAGIC_SIZE + BYTEC_FOOTER_SIZE) {
        return BYTEC_E_FORMAT;
    }
    if (bytec_source_seek(source, 0) != 0 ||
        bytec_source_read(source, magic, BYTEC_MAGIC_SIZE) != 0 ||
        memcmp(magic, BYTEC_MAGIC, BYTEC_MAGIC_SIZE) != 0) {
        return BYTEC_E_FORMAT;
    }
    if (bytec_source_seek(source, source->size - BYTEC_FOOTER_SIZE) != 0 ||
        bytec_source_u64(source, &index_offset) != 0 ||
        bytec_source_read(source, tail, BYTEC_MAGIC_SIZE) != 0 ||
        memcmp(tail, BYTEC_MAGIC, BYTEC_MAGIC_SIZE) != 0) {
        return BYTEC_E_FORMAT;
    }
    if (bytec_source_seek(source, index_offset) != 0 ||
        bytec_source_u64(source, &count) != 0) {
        return BYTEC_E_FORMAT;
    }
    if (count > source->size) {
        return BYTEC_E_FORMAT;
    }

    if (count > 0) {
        records = (bytec_record *)calloc((size_t)count, sizeof *records);
        if (records == NULL) {
            return BYTEC_E_MEMORY;
        }
    }

    for (i = 0; i < count; i++) {
        uint16_t name_len = 0;

        if (bytec_source_u8(source, &records[i].flags) != 0 ||
            bytec_source_u16(source, &name_len) != 0) {
            goto broken;
        }
        records[i].path = (char *)malloc((size_t)name_len + 1);
        if (records[i].path == NULL) {
            bytec_free_records(records, (size_t)count);
            return BYTEC_E_MEMORY;
        }
        if (bytec_source_read(source, records[i].path, name_len) != 0) {
            goto broken;
        }
        records[i].path[name_len] = '\0';

        if (bytec_source_u64(source, &records[i].size) != 0 ||
            bytec_source_u64(source, &records[i].packed_size) != 0 ||
            bytec_source_u64(source, &records[i].offset) != 0 ||
            bytec_source_u32(source, &records[i].crc) != 0) {
            goto broken;
        }
    }

    *out_records = records;
    *out_count = (size_t)count;
    return BYTEC_OK;

broken:
    bytec_free_records(records, (size_t)count);
    return BYTEC_E_FORMAT;
}

bytec_status bytec_decode_entry(bytec_source *source, const bytec_record *record,
                                bytec_sink_fn sink, void *user)
{
    uint8_t     *raw = NULL;
    uint8_t     *packed = NULL;
    uint64_t     remaining;
    uint32_t     crc = 0;
    bytec_status status = BYTEC_OK;

    if (record->flags == BYTEC_FLAG_DIR || record->size == 0) {
        return record->crc == 0 ? BYTEC_OK : BYTEC_E_FORMAT;
    }
    if (bytec_source_seek(source, record->offset) != 0) {
        return BYTEC_E_FORMAT;
    }

    raw = (uint8_t *)malloc(BYTEC_BLOCK_SIZE);
    packed = (uint8_t *)malloc(bytec_lz_bound(BYTEC_BLOCK_SIZE));
    if (raw == NULL || packed == NULL) {
        status = BYTEC_E_MEMORY;
        goto done;
    }

    remaining = record->size;
    while (remaining > 0) {
        uint8_t  method = 0;
        uint32_t raw_len = 0;
        uint32_t packed_len = 0;

        if (bytec_source_u8(source, &method) != 0 ||
            bytec_source_u32(source, &raw_len) != 0 ||
            bytec_source_u32(source, &packed_len) != 0 ||
            raw_len == 0 || raw_len > BYTEC_BLOCK_SIZE ||
            packed_len > bytec_lz_bound(BYTEC_BLOCK_SIZE) ||
            (uint64_t)raw_len > remaining) {
            status = BYTEC_E_FORMAT;
            goto done;
        }

        if (method == BYTEC_METHOD_STORE) {
            if (packed_len != raw_len || bytec_source_read(source, raw, raw_len) != 0) {
                status = BYTEC_E_FORMAT;
                goto done;
            }
        } else if (method == BYTEC_METHOD_LZ) {
            if (bytec_source_read(source, packed, packed_len) != 0 ||
                bytec_lz_decompress(packed, packed_len, raw, raw_len) != 0) {
                status = BYTEC_E_FORMAT;
                goto done;
            }
        } else {
            status = BYTEC_E_FORMAT;
            goto done;
        }

        if (sink(raw, raw_len, user) != 0) {
            status = BYTEC_E_IO;
            goto done;
        }
        crc = bytec_crc32(crc, raw, raw_len);
        remaining -= raw_len;
    }

    if (crc != record->crc) {
        status = BYTEC_E_FORMAT;
    }

done:
    free(raw);
    free(packed);
    return status;
}
