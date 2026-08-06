// kernel/ipc/shared_memory.c
#include <stdint.h>
#include <stddef.h>

#define SHM_MAX_PAGES 256

typedef struct shm_region {
    uint64_t phys_pages[SHM_MAX_PAGES];
    uint32_t page_count;
} shm_region_t;

extern uint64_t pmm_alloc_pages(int);
extern void pmm_free_pages(uint64_t, int);
extern void vmm_map(void *, uint64_t, uint64_t, uint64_t);

shm_region_t *shm_create(uint32_t pages)
{
    extern void *slab_alloc(uint32_t);
    if (pages > SHM_MAX_PAGES) return NULL;

    shm_region_t *shm = (shm_region_t*)slab_alloc(sizeof(shm_region_t));
    shm->page_count = pages;

    for (uint32_t i = 0; i < pages; ++i)
        shm->phys_pages[i] = pmm_alloc_pages(0);

    return shm;
}

void shm_map(shm_region_t *shm, void *space, uint64_t vaddr, uint64_t flags)
{
    for (uint32_t i = 0; i < shm->page_count; ++i) {
        vmm_map(space,
                vaddr + i * 4096,
                shm->phys_pages[i],
                flags);
    }
}

void shm_destroy(shm_region_t *shm)
{
    for (uint32_t i = 0; i < shm->page_count; ++i)
        pmm_free_pages(shm->phys_pages[i], 0);
}
