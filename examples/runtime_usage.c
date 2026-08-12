#include <stdio.h>

#include "bytec.h"

int main(int argc, char **argv)
{
    bytec_archive *archive;
    bytec_files    files;
    bytec_buffer   buffer;
    bytec_status   status = BYTEC_OK;
    size_t         i;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <archive> <entry>\n", argv[0]);
        return 2;
    }

    archive = bytec_open_file(argv[1], &status);
    if (archive == NULL) {
        fprintf(stderr, "%s\n", bytec_status_text(status));
        return 1;
    }

    status = bytec_read_all(archive, &files);
    if (status == BYTEC_OK) {
        for (i = 0; i < files.count; i++) {
            printf("%s %lu\n", files.items[i].path, (unsigned long)files.items[i].size);
        }
        bytec_free_files(&files);
    }

    status = bytec_read_entry(archive, argv[2], &buffer);
    if (status == BYTEC_OK) {
        fwrite(buffer.data, 1, buffer.size, stdout);
        bytec_free_buffer(&buffer);
    } else {
        fprintf(stderr, "%s\n", bytec_status_text(status));
    }

    bytec_close(archive);
    return status == BYTEC_OK ? 0 : 1;
}
