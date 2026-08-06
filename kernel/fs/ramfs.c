// kernel/fs/ramfs.c
#include <stdint.h>
#include <stddef.h>

#define RAMFS_MAX_FILES 128
#define RAMFS_DATA_SIZE 4096

typedef struct ramfs_node {
    char name[64];
    uint8_t data[RAMFS_DATA_SIZE];
    uint64_t size;
} ramfs_node_t;

static ramfs_node_t nodes[RAMFS_MAX_FILES];
static int node_count = 0;

void *slab_alloc(uint32_t);

ramfs_node_t *ramfs_create(const char *name)
{
    ramfs_node_t *n = &nodes[node_count++];
    for (int i = 0; i < 64 && name[i]; i++)
        n->name[i] = name[i];
    n->size = 0;
    return n;
}

int ramfs_write(ramfs_node_t *n, const void *buf, uint64_t len)
{
    if (len > RAMFS_DATA_SIZE) len = RAMFS_DATA_SIZE;
    for (uint64_t i = 0; i < len; i++)
        n->data[i] = ((uint8_t*)buf)[i];
    n->size = len;
    return len;
}

void ramfs_init(void)
{
    node_count = 0;
    // Aquí podrías inicializar estructuras de datos o montar el fs base
    for(int i = 0; i < RAMFS_MAX_FILES; i++) {
        nodes[i].name[0] = '\0';
        nodes[i].size = 0;
    }
}