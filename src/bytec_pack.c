#include <stdlib.h>
#include <string.h>

#include "bytec.h"
#include "bytec_crc.h"
#include "bytec_format.h"
#include "bytec_io.h"
#include "bytec_lz.h"

typedef struct {
    bytec_record *items;
    size_t        count;
    size_t        capacity;
} bytec_record_list;

typedef struct {
    bytec_record_list *list;
    const char        *root;
    FILE              *archive;
    uint8_t           *raw;
    uint8_t           *packed;
    bytec_status       status;
} bytec_pack_context;

static int list_push(bytec_record_list *list, const bytec_record *record)
{
    if (list->count == list->capacity) {
        size_t        capacity = list->capacity ? list->capacity * 2 : 64;
        bytec_record *items = (bytec_record *)realloc(list->items, capacity * sizeof *items);

        if (items == NULL) {
            return -1;
        }
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = *record;
    return 0;
}

static bytec_status pack_file_data(bytec_pack_context *context, const char *full_path,
                                   bytec_record *record)
{
    FILE   *input = fopen(full_path, "rb");
    size_t  read_len;

    if (input == NULL) {
        return BYTEC_E_IO;
    }

    record->offset = (uint64_t)bytec_tell(context->archive);
    record->size = 0;
    record->packed_size = 0;
    record->crc = 0;

    while ((read_len = fread(context->raw, 1, BYTEC_BLOCK_SIZE, input)) > 0) {
        size_t   packed_len = bytec_lz_compress(context->raw, read_len, context->packed,
                                                bytec_lz_bound(BYTEC_BLOCK_SIZE));
        uint8_t  method = BYTEC_METHOD_LZ;
        uint8_t *payload = context->packed;

        if (packed_len == 0 || packed_len >= read_len) {
            method = BYTEC_METHOD_STORE;
            packed_len = read_len;
            payload = context->raw;
        }

        if (bytec_put_u8(context->archive, method) != 0 ||
            bytec_put_u32(context->archive, (uint32_t)read_len) != 0 ||
            bytec_put_u32(context->archive, (uint32_t)packed_len) != 0 ||
            fwrite(payload, 1, packed_len, context->archive) != packed_len) {
            fclose(input);
            return BYTEC_E_IO;
        }

        record->crc = bytec_crc32(record->crc, context->raw, read_len);
        record->size += read_len;
        record->packed_size += packed_len + 9;
    }

    if (ferror(input)) {
        fclose(input);
        return BYTEC_E_IO;
    }
    fclose(input);
    return BYTEC_OK;
}

static int pack_visit(const char *rel_path, int is_dir, void *user)
{
    bytec_pack_context *context = (bytec_pack_context *)user;
    bytec_record        record;
    char               *full_path;

    memset(&record, 0, sizeof record);
    record.flags = is_dir ? BYTEC_FLAG_DIR : BYTEC_FLAG_FILE;
    record.path = bytec_str_dup(rel_path);
    if (record.path == NULL) {
        context->status = BYTEC_E_MEMORY;
        return -1;
    }
    bytec_path_normalize(record.path);

    if (strlen(record.path) > 0xffff) {
        free(record.path);
        context->status = BYTEC_E_ARG;
        return -1;
    }

    if (!is_dir) {
        full_path = bytec_path_join(context->root, rel_path);
        if (full_path == NULL) {
            free(record.path);
            context->status = BYTEC_E_MEMORY;
            return -1;
        }
        context->status = pack_file_data(context, full_path, &record);
        free(full_path);
        if (context->status != BYTEC_OK) {
            free(record.path);
            return -1;
        }
    }

    if (list_push(context->list, &record) != 0) {
        free(record.path);
        context->status = BYTEC_E_MEMORY;
        return -1;
    }
    return 0;
}

static bytec_status write_index(FILE *archive, const bytec_record_list *list)
{
    uint64_t index_offset = (uint64_t)bytec_tell(archive);
    size_t   i;

    if (bytec_put_u64(archive, (uint64_t)list->count) != 0) {
        return BYTEC_E_IO;
    }
    for (i = 0; i < list->count; i++) {
        const bytec_record *record = &list->items[i];
        size_t              name_len = strlen(record->path);

        if (bytec_put_u8(archive, record->flags) != 0 ||
            bytec_put_u16(archive, (uint16_t)name_len) != 0 ||
            fwrite(record->path, 1, name_len, archive) != name_len ||
            bytec_put_u64(archive, record->size) != 0 ||
            bytec_put_u64(archive, record->packed_size) != 0 ||
            bytec_put_u64(archive, record->offset) != 0 ||
            bytec_put_u32(archive, record->crc) != 0) {
            return BYTEC_E_IO;
        }
    }
    if (bytec_put_u64(archive, index_offset) != 0 ||
        fwrite(BYTEC_MAGIC, 1, BYTEC_MAGIC_SIZE, archive) != BYTEC_MAGIC_SIZE) {
        return BYTEC_E_IO;
    }
    return BYTEC_OK;
}

bytec_status bytec_pack(const char *source_dir, const char *archive_path)
{
    bytec_record_list  list;
    bytec_pack_context context;
    bytec_status       status;

    if (source_dir == NULL || archive_path == NULL) {
        return BYTEC_E_ARG;
    }
    if (!bytec_is_dir(source_dir)) {
        return BYTEC_E_NOTFOUND;
    }

    memset(&list, 0, sizeof list);
    memset(&context, 0, sizeof context);
    context.list = &list;
    context.root = source_dir;
    context.status = BYTEC_OK;

    context.archive = fopen(archive_path, "wb");
    context.raw = (uint8_t *)malloc(BYTEC_BLOCK_SIZE);
    context.packed = (uint8_t *)malloc(bytec_lz_bound(BYTEC_BLOCK_SIZE));
    if (context.archive == NULL) {
        free(context.raw);
        free(context.packed);
        return BYTEC_E_IO;
    }
    if (context.raw == NULL || context.packed == NULL) {
        fclose(context.archive);
        free(context.raw);
        free(context.packed);
        return BYTEC_E_MEMORY;
    }

    if (fwrite(BYTEC_MAGIC, 1, BYTEC_MAGIC_SIZE, context.archive) != BYTEC_MAGIC_SIZE) {
        status = BYTEC_E_IO;
    } else if (bytec_walk(source_dir, pack_visit, &context) != 0) {
        status = context.status != BYTEC_OK ? context.status : BYTEC_E_IO;
    } else {
        status = write_index(context.archive, &list);
    }

    if (fclose(context.archive) != 0 && status == BYTEC_OK) {
        status = BYTEC_E_IO;
    }
    free(context.raw);
    free(context.packed);
    bytec_free_records(list.items, list.count);

    if (status != BYTEC_OK) {
        remove(archive_path);
    }
    return status;
}
