// kernel/fs/fat32.c
#include <stdint.h>
#include <stddef.h>

typedef struct fat32_bpb {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fats;
    uint32_t fat_size;
    uint32_t root_cluster;
} __attribute__((packed)) fat32_bpb_t;

static fat32_bpb_t *bpb;

void fat32_init(void *boot_sector)
{
    bpb = (fat32_bpb_t*)((uint8_t*)boot_sector + 11);
}

uint32_t fat32_cluster_to_lba(uint32_t cluster)
{
    return (cluster - 2) * bpb->sectors_per_cluster +
           bpb->reserved_sectors +
           bpb->fats * bpb->fat_size;
}
