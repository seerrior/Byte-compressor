````markdown
# Byte Compressor

A small C99 library that packs a folder into a single archive and unpacks it **into memory**. Nothing is written to disk when you decompress — no temp files, no extracted folder next to your binary. The data lives in RAM for as long as you need it.

Useful when you want to:

- bundle assets (textures, shaders, configs, levels) inside your executable and read them straight from memory;
- ship one file instead of an `assets/` folder;
- avoid leaving anything on disk.

No dependencies beyond the C standard library. Works on Windows, Linux and macOS.

## Features

- custom LZSS compressor (64 KiB window, hash-chain match search)
- 1 MiB blocks, so large files don't blow up memory
- CRC32 per entry, verified while decompressing
- index at the end of the archive — read one file without touching the rest
- CLI tool and CMake helpers to compress a folder at build time

## Build

```bash
cmake -B build
cmake --build build
```

or

```bash
make
make test
```

You get `libbytec`, the `bytec` CLI and an example program.

## Quick start

Pack a folder:

```bash
bytec pack assets assets.byc
```

Read it back at runtime:

```c
#include "bytec.h"

bytec_status   status;
bytec_archive *archive = bytec_open_file("assets.byc", &status);
bytec_buffer   shader;

if (bytec_read_entry(archive, "shaders/main.frag", &shader) == BYTEC_OK) {
    use_shader((const char *)shader.data, shader.size);
    bytec_free_buffer(&shader);
}

bytec_close(archive);
```

Everything at once:

```c
bytec_files files;

if (bytec_read_all(archive, &files) == BYTEC_OK) {
    for (size_t i = 0; i < files.count; i++) {
        if (!files.items[i].is_dir) {
            load_asset(files.items[i].path, files.items[i].data, files.items[i].size);
        }
    }
    bytec_free_files(&files);
}
```

One folder only:

```c
bytec_files files;

if (bytec_read_subtree(archive, "shaders", &files) == BYTEC_OK) {
    /* ... */
    bytec_free_files(&files);
}
```

Paths are relative to the packed folder and always use `/`. Each file buffer has an extra `\0` after `size`, so text assets work as C strings.

## Embedding the archive in your binary

Compress the folder at build time and get a C header with the bytes:

```cmake
include(cmake/BytecPack.cmake)

bytec_embed_folder(
    TARGET  assets_blob
    FOLDER  ${CMAKE_CURRENT_SOURCE_DIR}/assets
    HEADER  ${CMAKE_BINARY_DIR}/generated/assets.h
    SYMBOL  bytec_assets
)

target_link_libraries(my_app PRIVATE bytec assets_blob)
```

Then open it without ever touching the filesystem:

```c
#include "assets.h"

bytec_archive *archive = bytec_open_memory(bytec_assets, bytec_assets_size, &status);
```

`bytec_open_memory` does not copy the buffer — keep it alive until `bytec_close`.

If you just want the archive file produced during the build:

```cmake
bytec_pack_folder(
    TARGET  assets_archive
    FOLDER  ${CMAKE_CURRENT_SOURCE_DIR}/assets
    OUTPUT  ${CMAKE_BINARY_DIR}/assets.byc
)
```

## API

```c
bytec_status bytec_pack(const char *source_dir, const char *archive_path);

bytec_archive *bytec_open_file(const char *archive_path, bytec_status *status);
bytec_archive *bytec_open_memory(const void *data, size_t size, bytec_status *status);
void           bytec_close(bytec_archive *archive);

const bytec_index *bytec_archive_index(const bytec_archive *archive);

bytec_status bytec_read_all(bytec_archive *archive, bytec_files *out_files);
bytec_status bytec_read_subtree(bytec_archive *archive, const char *entry_path, bytec_files *out_files);
bytec_status bytec_read_entry(bytec_archive *archive, const char *entry_path, bytec_buffer *out_buffer);

void bytec_free_files(bytec_files *files);
void bytec_free_buffer(bytec_buffer *buffer);

bytec_status bytec_read_index(const char *archive_path, bytec_index *out_index);
void         bytec_free_index(bytec_index *index);
const char  *bytec_status_text(bytec_status status);
```

Every call returns a `bytec_status`; `bytec_status_text` turns it into a message.

## CLI

The CLI is a thin wrapper that also knows how to write results to disk.

```bash
bytec pack    <folder> <archive>
bytec unpack  <archive> <folder>
bytec extract <archive> <entry> <folder>
bytec list    <archive>
bytec embed   <archive> <header.h> <symbol>
```

## Format

```
"BYTECAR1"
per-file blocks: [method u8][raw_len u32][packed_len u32][payload]
index: [count u64] { flags u8, name_len u16, name, size u64, packed_size u64, offset u64, crc32 u32 }
footer: [index_offset u64]["BYTECAR1"]
```

Blocks that don't compress are stored raw. The CLI rejects archive paths that would escape the destination folder.

## License

MIT
````
