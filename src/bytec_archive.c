#include <stdlib.h>
#include <string.h>

#include "bytec.h"
#include "bytec_format.h"
#include "bytec_io.h"
#include "bytec_source.h"

struct bytec_archive {
    bytec_source  source;
    bytec_record *records;
    bytec_index   index;
};

typedef struct {
    uint8_t *data;
    size_t   used;
    size_t   capacity;
} bytec_memory_sink;

static int memory_sink(const uint8_t *data, size_t len, void *user)
{
    bytec_memory_sink *sink = (bytec_memory_sink *)user;

    if (sink->used + len > sink->capacity) {
        return -1;
    }
    memcpy(sink->data + sink->used, data, len);
    sink->used += len;
    return 0;
}

static int path_in_subtree(const char *path, const char *prefix)
{
    size_t len;

    if (prefix == NULL) {
        return 1;
    }
    len = strlen(prefix);
    if (strcmp(path, prefix) == 0) {
        return 1;
    }
    return strncmp(path, prefix, len) == 0 && path[len] == '/';
}

static bytec_status archive_build_index(bytec_archive *archive, size_t count)
{
    size_t i;

    if (count == 0) {
        return BYTEC_OK;
    }
    archive->index.items = (bytec_entry *)calloc(count, sizeof *archive->index.items);
    if (archive->index.items == NULL) {
        return BYTEC_E_MEMORY;
    }
    for (i = 0; i < count; i++) {
        archive->index.items[i].path = archive->records[i].path;
        archive->index.items[i].size = archive->records[i].size;
        archive->index.items[i].packed_size = archive->records[i].packed_size;
        archive->index.items[i].is_dir = archive->records[i].flags == BYTEC_FLAG_DIR;
    }
    archive->index.count = count;
    return BYTEC_OK;
}

static bytec_archive *archive_finish(bytec_archive *archive, bytec_status *status)
{
    bytec_record *records = NULL;
    size_t        count = 0;
    bytec_status  result;

    result = bytec_load_records(&archive->source, &records, &count);
    if (result == BYTEC_OK) {
        archive->records = records;
        result = archive_build_index(archive, count);
        if (result != BYTEC_OK) {
            bytec_free_records(records, count);
            archive->records = NULL;
        }
    }
    if (result != BYTEC_OK) {
        bytec_source_close(&archive->source);
        free(archive);
        if (status != NULL) {
            *status = result;
        }
        return NULL;
    }
    if (status != NULL) {
        *status = BYTEC_OK;
    }
    return archive;
}

bytec_archive *bytec_open_file(const char *archive_path, bytec_status *status)
{
    bytec_archive *archive;

    if (archive_path == NULL) {
        if (status != NULL) {
            *status = BYTEC_E_ARG;
        }
        return NULL;
    }
    archive = (bytec_archive *)calloc(1, sizeof *archive);
    if (archive == NULL) {
        if (status != NULL) {
            *status = BYTEC_E_MEMORY;
        }
        return NULL;
    }
    if (bytec_source_open_file(&archive->source, archive_path) != 0) {
        free(archive);
        if (status != NULL) {
            *status = BYTEC_E_IO;
        }
        return NULL;
    }
    return archive_finish(archive, status);
}

bytec_archive *bytec_open_memory(const void *data, size_t size, bytec_status *status)
{
    bytec_archive *archive;

    if (data == NULL) {
        if (status != NULL) {
            *status = BYTEC_E_ARG;
        }
        return NULL;
    }
    archive = (bytec_archive *)calloc(1, sizeof *archive);
    if (archive == NULL) {
        if (status != NULL) {
            *status = BYTEC_E_MEMORY;
        }
        return NULL;
    }
    bytec_source_open_memory(&archive->source, data, size);
    return archive_finish(archive, status);
}

void bytec_close(bytec_archive *archive)
{
    if (archive == NULL) {
        return;
    }
    bytec_source_close(&archive->source);
    bytec_free_records(archive->records, archive->index.count);
    free(archive->index.items);
    free(archive);
}

const bytec_index *bytec_archive_index(const bytec_archive *archive)
{
    return archive == NULL ? NULL : &archive->index;
}

static bytec_status archive_read_record(bytec_archive *archive, size_t position,
                                        uint8_t **out_data, size_t *out_size)
{
    const bytec_record *record = &archive->records[position];
    bytec_memory_sink   sink;
    bytec_status        status;

    *out_data = NULL;
    *out_size = 0;

    if (record->flags == BYTEC_FLAG_DIR) {
        return BYTEC_OK;
    }
    if (record->size > (uint64_t)(size_t)-1) {
        return BYTEC_E_MEMORY;
    }

    sink.capacity = (size_t)record->size;
    sink.used = 0;
    sink.data = (uint8_t *)malloc(sink.capacity + 1);
    if (sink.data == NULL) {
        return BYTEC_E_MEMORY;
    }
    sink.data[sink.capacity] = 0;

    status = bytec_decode_entry(&archive->source, record, memory_sink, &sink);
    if (status != BYTEC_OK || sink.used != sink.capacity) {
        free(sink.data);
        return status != BYTEC_OK ? status : BYTEC_E_FORMAT;
    }

    *out_data = sink.data;
    *out_size = sink.used;
    return BYTEC_OK;
}

static bytec_status archive_read_many(bytec_archive *archive, const char *prefix,
                                      bytec_files *out_files)
{
    bytec_file *items = NULL;
    size_t      matched = 0;
    size_t      i;
    size_t      slot = 0;

    out_files->items = NULL;
    out_files->count = 0;

    for (i = 0; i < archive->index.count; i++) {
        if (path_in_subtree(archive->records[i].path, prefix)) {
            matched++;
        }
    }
    if (matched == 0) {
        return prefix == NULL ? BYTEC_OK : BYTEC_E_NOTFOUND;
    }

    items = (bytec_file *)calloc(matched, sizeof *items);
    if (items == NULL) {
        return BYTEC_E_MEMORY;
    }

    for (i = 0; i < archive->index.count; i++) {
        bytec_status status;

        if (!path_in_subtree(archive->records[i].path, prefix)) {
            continue;
        }
        items[slot].path = bytec_str_dup(archive->records[i].path);
        items[slot].is_dir = archive->records[i].flags == BYTEC_FLAG_DIR;
        if (items[slot].path == NULL) {
            out_files->items = items;
            out_files->count = slot + 1;
            bytec_free_files(out_files);
            return BYTEC_E_MEMORY;
        }
        status = archive_read_record(archive, i, &items[slot].data, &items[slot].size);
        slot++;
        if (status != BYTEC_OK) {
            out_files->items = items;
            out_files->count = slot;
            bytec_free_files(out_files);
            return status;
        }
    }

    out_files->items = items;
    out_files->count = matched;
    return BYTEC_OK;
}

bytec_status bytec_read_all(bytec_archive *archive, bytec_files *out_files)
{
    if (archive == NULL || out_files == NULL) {
        return BYTEC_E_ARG;
    }
    return archive_read_many(archive, NULL, out_files);
}

bytec_status bytec_read_subtree(bytec_archive *archive, const char *entry_path,
                                bytec_files *out_files)
{
    char        *wanted;
    bytec_status status;

    if (archive == NULL || entry_path == NULL || out_files == NULL) {
        return BYTEC_E_ARG;
    }
    wanted = bytec_str_dup(entry_path);
    if (wanted == NULL) {
        return BYTEC_E_MEMORY;
    }
    bytec_path_normalize(wanted);
    status = archive_read_many(archive, wanted, out_files);
    free(wanted);
    return status;
}

bytec_status bytec_read_entry(bytec_archive *archive, const char *entry_path,
                              bytec_buffer *out_buffer)
{
    char        *wanted;
    size_t       i;
    bytec_status status = BYTEC_E_NOTFOUND;

    if (archive == NULL || entry_path == NULL || out_buffer == NULL) {
        return BYTEC_E_ARG;
    }
    out_buffer->data = NULL;
    out_buffer->size = 0;

    wanted = bytec_str_dup(entry_path);
    if (wanted == NULL) {
        return BYTEC_E_MEMORY;
    }
    bytec_path_normalize(wanted);

    for (i = 0; i < archive->index.count; i++) {
        if (strcmp(archive->records[i].path, wanted) != 0) {
            continue;
        }
        status = archive->records[i].flags == BYTEC_FLAG_DIR
                     ? BYTEC_E_ARG
                     : archive_read_record(archive, i, &out_buffer->data, &out_buffer->size);
        break;
    }

    free(wanted);
    return status;
}

void bytec_free_buffer(bytec_buffer *buffer)
{
    if (buffer == NULL) {
        return;
    }
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
}

void bytec_free_files(bytec_files *files)
{
    size_t i;

    if (files == NULL) {
        return;
    }
    for (i = 0; i < files->count; i++) {
        free(files->items[i].path);
        free(files->items[i].data);
    }
    free(files->items);
    files->items = NULL;
    files->count = 0;
}

bytec_status bytec_read_index(const char *archive_path, bytec_index *out_index)
{
    bytec_archive     *archive;
    const bytec_index *source;
    bytec_entry       *items = NULL;
    bytec_status       status = BYTEC_OK;
    size_t             i;

    if (out_index == NULL) {
        return BYTEC_E_ARG;
    }
    out_index->items = NULL;
    out_index->count = 0;

    archive = bytec_open_file(archive_path, &status);
    if (archive == NULL) {
        return status;
    }
    source = &archive->index;

    if (source->count > 0) {
        items = (bytec_entry *)calloc(source->count, sizeof *items);
        if (items == NULL) {
            bytec_close(archive);
            return BYTEC_E_MEMORY;
        }
    }
    for (i = 0; i < source->count; i++) {
        items[i] = source->items[i];
        items[i].path = bytec_str_dup(source->items[i].path);
        if (items[i].path == NULL) {
            out_index->items = items;
            out_index->count = i;
            bytec_free_index(out_index);
            bytec_close(archive);
            return BYTEC_E_MEMORY;
        }
    }

    out_index->items = items;
    out_index->count = source->count;
    bytec_close(archive);
    return BYTEC_OK;
}

void bytec_free_index(bytec_index *index)
{
    size_t i;

    if (index == NULL) {
        return;
    }
    for (i = 0; i < index->count; i++) {
        free(index->items[i].path);
    }
    free(index->items);
    index->items = NULL;
    index->count = 0;
}

const char *bytec_status_text(bytec_status status)
{
    switch (status) {
    case BYTEC_OK:         return "ok";
    case BYTEC_E_ARG:      return "invalid argument";
    case BYTEC_E_IO:       return "io error";
    case BYTEC_E_MEMORY:   return "out of memory";
    case BYTEC_E_FORMAT:   return "corrupted archive";
    case BYTEC_E_NOTFOUND: return "not found";
    default:               return "unknown error";
    }
}
