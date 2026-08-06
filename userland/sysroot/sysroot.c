#include "sysroot.h"
#include <string.h>

static void copy_string(char *dst, const char *src, uint32_t max_len)
{
    if (!dst || !src) return;
    for (uint32_t i = 0; i + 1 < max_len && src[i]; ++i) dst[i] = src[i];
    dst[max_len - 1] = 0;
}

void sysroot_init(sysroot_layout_t *layout)
{
    if (!layout) return;
    memset(layout, 0, sizeof(*layout));
    sysroot_add_entry(layout, "/bin", "dir", "/usr/bin");
    sysroot_add_entry(layout, "/dev", "dir", "/dev");
    sysroot_add_entry(layout, "/etc", "dir", "/etc");
    sysroot_add_entry(layout, "/lib", "dir", "/usr/lib");
    sysroot_add_entry(layout, "/usr", "dir", "/usr");
    sysroot_add_entry(layout, "/tmp", "dir", "/tmp");
}

int sysroot_add_entry(sysroot_layout_t *layout, const char *path, const char *kind, const char *target)
{
    if (!layout || !path || !kind || !target || layout->count >= SYSROOT_MAX_ENTRIES) return -1;
    sysroot_entry_t *entry = &layout->entries[layout->count++];
    copy_string(entry->path, path, sizeof(entry->path));
    copy_string(entry->kind, kind, sizeof(entry->kind));
    copy_string(entry->target, target, sizeof(entry->target));
    return 0;
}

int sysroot_lookup(const sysroot_layout_t *layout, const char *path, sysroot_entry_t *out)
{
    if (!layout || !path || !out) return -1;
    for (uint32_t i = 0; i < layout->count; ++i) {
        if (strcmp(layout->entries[i].path, path) == 0) {
            *out = layout->entries[i];
            return 0;
        }
    }
    return -1;
}
