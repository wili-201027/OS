// kernel/mm/slab.c
//
// Asignador de objetos de tamaño fijo (slab allocator simplificado).
// Si el tamaño pedido supera el cache más grande, hace fallback directo al PMM.

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

extern uint64_t pmm_alloc_pages(int order);
extern void     pmm_free_pages(uint64_t paddr, int order);

typedef struct slab {
    struct slab *next;
    uint64_t    free_bitmap;
    uint32_t    obj_size;
    uint32_t    total_objs;
} slab_t;

typedef struct slab_cache {
    uint32_t  obj_size;
    slab_t   *slabs;
} slab_cache_t;

// Caches estándar: 32, 64, 128, 256, 512, 1024, 2048, 4096 bytes
static slab_cache_t caches[] = {
    {   32, 0 },
    {   64, 0 },
    {  128, 0 },
    {  256, 0 },
    {  512, 0 },
    { 1024, 0 },
    { 2048, 0 },
    { 4096, 0 },   // ← antes faltaba este; scheduler_init pide 4096
};
#define NCACHES (sizeof(caches) / sizeof(caches[0]))

// Crea un nuevo slab: asigna una página y divide en objetos de tamaño fijo.
static slab_t *slab_create(uint32_t obj_size)
{
    uint64_t paddr = pmm_alloc_pages(0);
    if (!paddr) return (slab_t*)0;

    slab_t *slab = (slab_t *)paddr;
    slab->obj_size   = obj_size;
    slab->total_objs = (PAGE_SIZE - sizeof(slab_t)) / obj_size;
    if (slab->total_objs > 64) slab->total_objs = 64;   // bitmap = 64 bits
    slab->free_bitmap = (slab->total_objs == 64)
                        ? ~0ULL
                        : (1ULL << slab->total_objs) - 1;
    slab->next = (slab_t*)0;
    return slab;
}

// Asigna un objeto de al menos `size` bytes.
// Para tamaños > 4096 hace fallback a páginas enteras del PMM.
void *slab_alloc(uint32_t size)
{
    // Caso especial: tamaño mayor que cualquier cache → página(s) directas
    if (size > 4096) {
        int order = 0;
        uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        while ((1u << order) < pages) order++;
        uint64_t paddr = pmm_alloc_pages(order);
        return paddr ? (void *)paddr : (void*)0;
    }

    // Buscar el cache con obj_size >= size
    for (size_t i = 0; i < NCACHES; ++i) {
        if (caches[i].obj_size < size) continue;

        // Buscar un slab con espacio libre
        slab_t *slab = caches[i].slabs;
        while (slab && slab->free_bitmap == 0)
            slab = slab->next;

        // Crear slab nuevo si no hay ninguno con espacio
        if (!slab) {
            slab = slab_create(caches[i].obj_size);
            if (!slab) return (void*)0;
            slab->next = caches[i].slabs;
            caches[i].slabs = slab;
        }

        // Tomar el primer bit libre
        int idx = __builtin_ctzll(slab->free_bitmap);
        slab->free_bitmap &= ~(1ULL << idx);

        return (void *)((uint8_t *)slab + sizeof(slab_t)
                        + (uint32_t)idx * caches[i].obj_size);
    }
    return (void*)0;
}

// Libera un objeto devuelto por slab_alloc.
void slab_free(void *ptr)
{
    if (!ptr) return;
    // El slab_t está al principio de la página (alineada a 4K)
    slab_t *slab = (slab_t *)((uintptr_t)ptr & ~(uintptr_t)(PAGE_SIZE - 1));
    uint64_t offset = (uintptr_t)ptr - ((uintptr_t)slab + sizeof(slab_t));
    uint64_t idx    = offset / slab->obj_size;
    slab->free_bitmap |= (1ULL << idx);
}
