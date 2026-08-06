// kernel/fs/initrd.c
#include <stdint.h>
#include <stddef.h>

typedef struct initrd_header {
    uint32_t magic;
    uint32_t file_count;
} initrd_header_t;

typedef struct initrd_file {
    char name[64];
    uint32_t offset;
    uint32_t size;
} initrd_file_t;

static uint8_t *initrd_base;

void initrd_init(void *addr)
{
    initrd_base = (uint8_t*)addr;
}

void *initrd_get(const char *name, uint32_t *size)
{
    initrd_header_t *hdr = (initrd_header_t*)initrd_base;
    initrd_file_t *files = (initrd_file_t*)(initrd_base + sizeof(*hdr));

    for (uint32_t i = 0; i < hdr->file_count; i++) {
        if (!name[0] || files[i].name[0] == name[0]) {
            *size = files[i].size;
            return initrd_base + files[i].offset;
        }
    }
    return NULL;
}
