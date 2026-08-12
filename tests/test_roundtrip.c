#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytec.h"
#include "bytec_io.h"

#define SAMPLE_DIR   "bytec_sample"
#define ARCHIVE_PATH "bytec_sample.byc"

static int failures = 0;

static void check(int condition, const char *label)
{
    if (!condition) {
        failures++;
    }
    printf("%s %s\n", condition ? "PASS" : "FAIL", label);
}

static int write_file(const char *path, const char *text, size_t repeat)
{
    FILE  *file;
    size_t i;

    if (bytec_mkdir_for_file(path) != 0) {
        return -1;
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    for (i = 0; i < repeat; i++) {
        fputs(text, file);
    }
    fclose(file);
    return 0;
}

static uint8_t *slurp(const char *path, size_t *out_size)
{
    FILE    *file = fopen(path, "rb");
    uint8_t *data;
    long     size;

    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    data = (uint8_t *)malloc((size_t)size);
    if (data == NULL || fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *out_size = (size_t)size;
    return data;
}

static int same_as_file(const char *path, const uint8_t *data, size_t size)
{
    size_t   expected_size = 0;
    uint8_t *expected = slurp(path, &expected_size);
    int      equal;

    if (expected == NULL) {
        return 0;
    }
    equal = expected_size == size && memcmp(expected, data, size) == 0;
    free(expected);
    return equal;
}

static const bytec_file *find_file(const bytec_files *files, const char *path)
{
    size_t i;

    for (i = 0; i < files->count; i++) {
        if (strcmp(files->items[i].path, path) == 0) {
            return &files->items[i];
        }
    }
    return NULL;
}

int main(void)
{
    bytec_archive *archive;
    bytec_files    files;
    bytec_buffer   buffer;
    bytec_status   status = BYTEC_OK;
    uint8_t       *archive_bytes;
    size_t         archive_size = 0;
    const bytec_file *item;

    check(write_file(SAMPLE_DIR "/alpha.txt", "byte compressor sample line\n", 5000) == 0,
          "create alpha.txt");
    check(write_file(SAMPLE_DIR "/nested/beta.txt", "0123456789", 20000) == 0,
          "create nested/beta.txt");
    check(write_file(SAMPLE_DIR "/nested/deep/gamma.bin", "\x01\x02\x03\x04", 1) == 0,
          "create nested/deep/gamma.bin");

    check(bytec_pack(SAMPLE_DIR, ARCHIVE_PATH) == BYTEC_OK, "pack");

    archive = bytec_open_file(ARCHIVE_PATH, &status);
    check(archive != NULL, "open archive from file");
    if (archive == NULL) {
        return 1;
    }
    check(bytec_archive_index(archive)->count >= 5, "index size");

    check(bytec_read_all(archive, &files) == BYTEC_OK, "read all into memory");
    item = find_file(&files, "alpha.txt");
    check(item != NULL && same_as_file(SAMPLE_DIR "/alpha.txt", item->data, item->size),
          "alpha.txt matches");
    item = find_file(&files, "nested/deep/gamma.bin");
    check(item != NULL && same_as_file(SAMPLE_DIR "/nested/deep/gamma.bin", item->data, item->size),
          "nested/deep/gamma.bin matches");
    bytec_free_files(&files);

    check(bytec_read_entry(archive, "nested/beta.txt", &buffer) == BYTEC_OK,
          "read single entry");
    check(same_as_file(SAMPLE_DIR "/nested/beta.txt", buffer.data, buffer.size),
          "single entry matches");
    bytec_free_buffer(&buffer);

    check(bytec_read_entry(archive, "missing.txt", &buffer) == BYTEC_E_NOTFOUND,
          "missing entry reported");

    check(bytec_read_subtree(archive, "nested/deep", &files) == BYTEC_OK,
          "read subtree");
    check(files.count == 2, "subtree size");
    bytec_free_files(&files);

    check(bytec_read_subtree(archive, "nope", &files) == BYTEC_E_NOTFOUND,
          "missing subtree reported");

    bytec_close(archive);

    archive_bytes = slurp(ARCHIVE_PATH, &archive_size);
    check(archive_bytes != NULL, "load archive bytes");
    archive = bytec_open_memory(archive_bytes, archive_size, &status);
    check(archive != NULL, "open archive from memory");
    check(bytec_read_entry(archive, "alpha.txt", &buffer) == BYTEC_OK,
          "read entry from memory archive");
    check(same_as_file(SAMPLE_DIR "/alpha.txt", buffer.data, buffer.size),
          "memory archive entry matches");
    bytec_free_buffer(&buffer);
    bytec_close(archive);
    free(archive_bytes);

    printf("%s\n", failures == 0 ? "all tests passed" : "tests failed");
    return failures == 0 ? 0 : 1;
}
