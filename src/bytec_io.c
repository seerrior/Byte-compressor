#include "bytec_io.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

char *bytec_str_dup(const char *text)
{
    size_t len;
    char  *copy;

    if (text == NULL) {
        return NULL;
    }
    len = strlen(text);
    copy = (char *)malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

void bytec_path_normalize(char *path)
{
    size_t i;

    if (path == NULL) {
        return;
    }
    for (i = 0; path[i] != '\0'; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
    while (i > 1 && path[i - 1] == '/') {
        path[--i] = '\0';
    }
}

char *bytec_path_join(const char *base, const char *tail)
{
    size_t base_len;
    size_t tail_len;
    char  *out;

    if (tail == NULL) {
        return NULL;
    }
    if (base == NULL || base[0] == '\0') {
        return bytec_str_dup(tail);
    }

    base_len = strlen(base);
    tail_len = strlen(tail);
    while (base_len > 0 && (base[base_len - 1] == '/' || base[base_len - 1] == '\\')) {
        base_len--;
    }

    out = (char *)malloc(base_len + tail_len + 2);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, base, base_len);
    out[base_len] = '/';
    memcpy(out + base_len + 1, tail, tail_len + 1);
    return out;
}

int bytec_path_is_safe(const char *rel_path)
{
    size_t i = 0;

    if (rel_path == NULL || rel_path[0] == '\0') {
        return 0;
    }
    if (rel_path[0] == '/' || rel_path[0] == '\\') {
        return 0;
    }
    if (rel_path[1] == ':') {
        return 0;
    }

    while (rel_path[i] != '\0') {
        if (rel_path[i] == '.' && rel_path[i + 1] == '.' &&
            (rel_path[i + 2] == '/' || rel_path[i + 2] == '\\' || rel_path[i + 2] == '\0') &&
            (i == 0 || rel_path[i - 1] == '/' || rel_path[i - 1] == '\\')) {
            return 0;
        }
        i++;
    }
    return 1;
}

int bytec_is_dir(const char *path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return 0;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
#else
    struct stat info;
    if (stat(path, &info) != 0) {
        return 0;
    }
    return S_ISDIR(info.st_mode) ? 1 : 0;
#endif
}

static int bytec_mkdir_one(const char *path)
{
#ifdef _WIN32
    if (_mkdir(path) == 0) {
        return 0;
    }
#else
    if (mkdir(path, 0775) == 0) {
        return 0;
    }
#endif
    return bytec_is_dir(path) ? 0 : -1;
}

int bytec_mkdir_p(const char *path)
{
    char  *work;
    size_t i;
    int    status = 0;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }
    work = bytec_str_dup(path);
    if (work == NULL) {
        return -1;
    }
    bytec_path_normalize(work);

    for (i = 0; work[i] != '\0'; i++) {
        if (work[i] == '/' && i > 0) {
            work[i] = '\0';
            if (work[i - 1] != ':') {
                status = bytec_mkdir_one(work);
            }
            work[i] = '/';
            if (status != 0) {
                free(work);
                return -1;
            }
        }
    }
    status = bytec_mkdir_one(work);
    free(work);
    return status;
}

int bytec_mkdir_for_file(const char *path)
{
    char  *work;
    size_t i;
    size_t cut = 0;
    int    status = 0;

    work = bytec_str_dup(path);
    if (work == NULL) {
        return -1;
    }
    bytec_path_normalize(work);

    for (i = 0; work[i] != '\0'; i++) {
        if (work[i] == '/') {
            cut = i;
        }
    }
    if (cut > 0) {
        work[cut] = '\0';
        status = bytec_mkdir_p(work);
    }
    free(work);
    return status;
}

static int bytec_walk_dir(const char *root, const char *prefix,
                          bytec_walk_fn fn, void *user)
{
    int status = 0;

#ifdef _WIN32
    WIN32_FIND_DATAA data;
    HANDLE           handle;
    char            *pattern = bytec_path_join(root, "*");

    if (pattern == NULL) {
        return -1;
    }
    handle = FindFirstFileA(pattern, &data);
    free(pattern);
    if (handle == INVALID_HANDLE_VALUE) {
        return -1;
    }

    do {
        char *rel;
        char *child;
        int   is_dir;

        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }
        is_dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
        rel = bytec_path_join(prefix, data.cFileName);
        child = bytec_path_join(root, data.cFileName);
        if (rel == NULL || child == NULL) {
            free(rel);
            free(child);
            status = -1;
            break;
        }

        status = fn(rel, is_dir, user);
        if (status == 0 && is_dir) {
            status = bytec_walk_dir(child, rel, fn, user);
        }
        free(rel);
        free(child);
    } while (status == 0 && FindNextFileA(handle, &data));

    FindClose(handle);
#else
    DIR           *dir = opendir(root);
    struct dirent *item;

    if (dir == NULL) {
        return -1;
    }
    while (status == 0 && (item = readdir(dir)) != NULL) {
        char *rel;
        char *child;
        int   is_dir;

        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) {
            continue;
        }
        rel = bytec_path_join(prefix, item->d_name);
        child = bytec_path_join(root, item->d_name);
        if (rel == NULL || child == NULL) {
            free(rel);
            free(child);
            status = -1;
            break;
        }
        is_dir = bytec_is_dir(child);

        status = fn(rel, is_dir, user);
        if (status == 0 && is_dir) {
            status = bytec_walk_dir(child, rel, fn, user);
        }
        free(rel);
        free(child);
    }
    closedir(dir);
#endif

    return status;
}

int bytec_walk(const char *root, bytec_walk_fn fn, void *user)
{
    if (root == NULL || fn == NULL || !bytec_is_dir(root)) {
        return -1;
    }
    return bytec_walk_dir(root, NULL, fn, user);
}
