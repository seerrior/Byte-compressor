#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytec.h"
#include "bytec_io.h"

static int usage(const char *program)
{
    fprintf(stderr,
            "usage:\n"
            "  %s pack <folder> <archive>\n"
            "  %s unpack <archive> <folder>\n"
            "  %s extract <archive> <entry> <folder>\n"
            "  %s list <archive>\n"
            "  %s embed <archive> <header.h> <symbol>\n",
            program, program, program, program, program);
    return 2;
}

static int fail(bytec_status status)
{
    fprintf(stderr, "%s\n", bytec_status_text(status));
    return 1;
}

static int write_files(const bytec_files *files, const char *dest_dir)
{
    size_t i;

    if (bytec_mkdir_p(dest_dir) != 0) {
        return fail(BYTEC_E_IO);
    }

    for (i = 0; i < files->count; i++) {
        const bytec_file *item = &files->items[i];
        char             *target;
        FILE             *output;

        if (!bytec_path_is_safe(item->path)) {
            return fail(BYTEC_E_FORMAT);
        }
        target = bytec_path_join(dest_dir, item->path);
        if (target == NULL) {
            return fail(BYTEC_E_MEMORY);
        }
        if (item->is_dir) {
            int status = bytec_mkdir_p(target);
            free(target);
            if (status != 0) {
                return fail(BYTEC_E_IO);
            }
            continue;
        }
        if (bytec_mkdir_for_file(target) != 0) {
            free(target);
            return fail(BYTEC_E_IO);
        }
        output = fopen(target, "wb");
        free(target);
        if (output == NULL) {
            return fail(BYTEC_E_IO);
        }
        if (item->size > 0 && fwrite(item->data, 1, item->size, output) != item->size) {
            fclose(output);
            return fail(BYTEC_E_IO);
        }
        fclose(output);
    }
    return 0;
}

static int run_restore(const char *archive_path, const char *entry_path, const char *dest_dir)
{
    bytec_archive *archive;
    bytec_files    files;
    bytec_status   status = BYTEC_OK;
    int            result;

    archive = bytec_open_file(archive_path, &status);
    if (archive == NULL) {
        return fail(status);
    }

    status = entry_path == NULL ? bytec_read_all(archive, &files)
                                : bytec_read_subtree(archive, entry_path, &files);
    bytec_close(archive);
    if (status != BYTEC_OK) {
        return fail(status);
    }

    result = write_files(&files, dest_dir);
    bytec_free_files(&files);
    return result;
}

static int run_list(const char *archive_path)
{
    bytec_index  index;
    bytec_status status = bytec_read_index(archive_path, &index);
    size_t       i;

    if (status != BYTEC_OK) {
        return fail(status);
    }
    for (i = 0; i < index.count; i++) {
        printf("%s%s\n", index.items[i].path, index.items[i].is_dir ? "/" : "");
    }
    bytec_free_index(&index);
    return 0;
}

static int run_embed(const char *archive_path, const char *header_path, const char *symbol)
{
    FILE  *input = fopen(archive_path, "rb");
    FILE  *output;
    size_t total = 0;
    int    value;

    if (input == NULL) {
        return fail(BYTEC_E_IO);
    }
    output = fopen(header_path, "wb");
    if (output == NULL) {
        fclose(input);
        return fail(BYTEC_E_IO);
    }

    fprintf(output, "#ifndef BYTEC_EMBED_%s\n#define BYTEC_EMBED_%s\n\n", symbol, symbol);
    fprintf(output, "static const unsigned char %s[] = {", symbol);
    while ((value = fgetc(input)) != EOF) {
        if (total > 0) {
            fputc(',', output);
        }
        fputs((total % 12 == 0) ? "\n    " : " ", output);
        fprintf(output, "0x%02x", (unsigned)value);
        total++;
    }
    fprintf(output, "\n};\n\n");
    fprintf(output, "static const unsigned long %s_size = %luul;\n\n", symbol,
            (unsigned long)total);
    fprintf(output, "#endif\n");

    fclose(input);
    if (fclose(output) != 0) {
        return fail(BYTEC_E_IO);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        return usage(argv[0]);
    }

    if (strcmp(argv[1], "pack") == 0 && argc == 4) {
        bytec_status status = bytec_pack(argv[2], argv[3]);
        return status == BYTEC_OK ? 0 : fail(status);
    }
    if (strcmp(argv[1], "unpack") == 0 && argc == 4) {
        return run_restore(argv[2], NULL, argv[3]);
    }
    if (strcmp(argv[1], "extract") == 0 && argc == 5) {
        return run_restore(argv[2], argv[3], argv[4]);
    }
    if (strcmp(argv[1], "list") == 0 && argc == 3) {
        return run_list(argv[2]);
    }
    if (strcmp(argv[1], "embed") == 0 && argc == 5) {
        return run_embed(argv[2], argv[3], argv[4]);
    }

    return usage(argv[0]);
}
