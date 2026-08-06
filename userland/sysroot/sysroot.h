#ifndef GPTOS_SYSROOT_H
#define GPTOS_SYSROOT_H

#include <stdint.h>

#define SYSROOT_MAX_ENTRIES 64
#define SYSROOT_MAX_PATH 256

typedef struct {
    char path[SYSROOT_MAX_PATH];
    char kind[16];
    char target[SYSROOT_MAX_PATH];
} sysroot_entry_t;

typedef struct {
    sysroot_entry_t entries[SYSROOT_MAX_ENTRIES];
    uint32_t count;
} sysroot_layout_t;

void sysroot_init(sysroot_layout_t *layout);
int sysroot_add_entry(sysroot_layout_t *layout, const char *path, const char *kind, const char *target);
int sysroot_lookup(const sysroot_layout_t *layout, const char *path, sysroot_entry_t *out);

#endif
