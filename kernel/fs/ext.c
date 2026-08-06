// kernel/fs/ext2.c
#include <stdint.h>
#include <stddef.h>

#define EXT2_MAGIC 0xEF53

typedef struct ext2_superblock {
    uint32_t inodes;
    uint32_t blocks;
    uint32_t log_block_size;
    uint16_t magic;
} ext2_superblock_t;

static ext2_superblock_t *sb;

void ext2_init(void *disk)
{
    sb = (ext2_superblock_t*)((uint8_t*)disk + 1024);
    if (sb->magic != EXT2_MAGIC)
        return;
}

uint32_t ext2_block_size(void)
{
    return 1024 << sb->log_block_size;
}
