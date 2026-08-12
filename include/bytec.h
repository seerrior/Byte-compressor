#ifndef BYTEC_H
#define BYTEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BYTEC_OK = 0,
    BYTEC_E_ARG,
    BYTEC_E_IO,
    BYTEC_E_MEMORY,
    BYTEC_E_FORMAT,
    BYTEC_E_NOTFOUND
} bytec_status;

typedef struct bytec_archive bytec_archive;

typedef struct {
    char    *path;
    uint64_t size;
    uint64_t packed_size;
    int      is_dir;
} bytec_entry;

typedef struct {
    bytec_entry *items;
    size_t       count;
} bytec_index;

typedef struct {
    uint8_t *data;
    size_t   size;
} bytec_buffer;

typedef struct {
    char    *path;
    uint8_t *data;
    size_t   size;
    int      is_dir;
} bytec_file;

typedef struct {
    bytec_file *items;
    size_t      count;
} bytec_files;

bytec_status bytec_pack(const char *source_dir, const char *archive_path);

bytec_archive *bytec_open_file(const char *archive_path, bytec_status *status);

bytec_archive *bytec_open_memory(const void *data, size_t size, bytec_status *status);

void bytec_close(bytec_archive *archive);

const bytec_index *bytec_archive_index(const bytec_archive *archive);

bytec_status bytec_read_all(bytec_archive *archive, bytec_files *out_files);

bytec_status bytec_read_subtree(bytec_archive *archive, const char *entry_path,
                                bytec_files *out_files);

bytec_status bytec_read_entry(bytec_archive *archive, const char *entry_path,
                              bytec_buffer *out_buffer);

void bytec_free_files(bytec_files *files);

void bytec_free_buffer(bytec_buffer *buffer);

bytec_status bytec_read_index(const char *archive_path, bytec_index *out_index);

void bytec_free_index(bytec_index *index);

const char *bytec_status_text(bytec_status status);

#ifdef __cplusplus
}
#endif

#endif
