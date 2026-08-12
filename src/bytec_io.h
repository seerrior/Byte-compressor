#ifndef BYTEC_IO_H
#define BYTEC_IO_H

#include <stdio.h>

typedef int (*bytec_walk_fn)(const char *rel_path, int is_dir, void *user);

int   bytec_is_dir(const char *path);
int   bytec_mkdir_p(const char *path);
int   bytec_mkdir_for_file(const char *path);
char *bytec_path_join(const char *base, const char *tail);
char *bytec_str_dup(const char *text);
void  bytec_path_normalize(char *path);
int   bytec_path_is_safe(const char *rel_path);
int   bytec_walk(const char *root, bytec_walk_fn fn, void *user);

#endif
