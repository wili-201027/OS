// kernel/mm/pmm.c
//
// Asignador físico de páginas con buddy allocator.
//
// REGLA DE SEGURIDAD:
//   pmm_init(start, end) recibe el rango de memoria LIBRE, es decir,
//   kernel_main debe pasarle un start que esté por encima del propio
//   kernel + módulos GRUB.  Actualmente lo llamamos con start = 2 MB
//   (0x200000), por lo que las páginas en 0x100000-0x1FFFFF (kernel)
//   nunca se asignan.

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE   4096
// FIX: Identity mapping (vaddr == paddr), not higher-half kernel offset
#define KERNEL_VIRTUAL_OFFSET 0
#define MAX_ORDER   10       // bloques de hasta 2^10 páginas = 4 MB

typedef struct buddy_block {
    struct buddy_block *next;
} buddy_block_t;

static buddy_block_t *free_lists[MAX_ORDER + 1];
static uint8_t  *bitmap       = NULL;
static uint64_t  total_pages  = 0;
static uint64_t  free_count   = 0;
static uint64_t  mem_start_pa = 0;

// ── Bitmap helpers ───────────────────────────────────────────────────────────
static void bm_set  (uint64_t pg) { bitmap[pg >> 3] |=  (1u << (pg & 7)); }
static void bm_clear(uint64_t pg) { bitmap[pg >> 3] &= ~(1u << (pg & 7)); }

// ── Buddy helpers ────────────────────────────────────────────────────────────
static void buddy_push(uint64_t page_idx, int order)
{
    uint64_t vaddr = (mem_start_pa + page_idx * PAGE_SIZE) + KERNEL_VIRTUAL_OFFSET;
    buddy_block_t *blk = (buddy_block_t *)vaddr;
    
    blk->next = free_lists[order];
    free_lists[order] = blk;
}

static uint64_t buddy_pop(int order)
{
    buddy_block_t *blk = free_lists[order];
    if (!blk) return (uint64_t)-1;
    free_lists[order] = blk->next;
    
    uint64_t vaddr = (uint64_t)blk;
    uint64_t paddr = vaddr - KERNEL_VIRTUAL_OFFSET;
    return (paddr - mem_start_pa) / PAGE_SIZE;
}

// ── pmm_init ─────────────────────────────────────────────────────────────────
void pmm_init(uint64_t mem_start, uint64_t mem_end)
{
    mem_start = (mem_start + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    mem_end   =  mem_end                    & ~(uint64_t)(PAGE_SIZE - 1);

    mem_start_pa = mem_start;
    total_pages  = (mem_end - mem_start) / PAGE_SIZE;
    free_count   = 0;

    bitmap = (uint8_t *)(mem_start + KERNEL_VIRTUAL_OFFSET);
    uint64_t bitmap_bytes = (total_pages + 7) / 8;
    uint64_t bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    for (int o = 0; o <= MAX_ORDER; ++o) free_lists[o] = NULL;

    // Marcar todo como usado, luego liberar las páginas post-bitmap
    for (uint64_t i = 0; i < ((bitmap_bytes + 3) & ~3ULL); ++i)
        bitmap[i] = 0xFF;

    for (uint64_t p = bitmap_pages; p < total_pages; ++p) {
        bm_clear(p);
        free_count++;
    }

    for (uint64_t i = 0; i < bitmap_pages; i++) {
        bm_set(i); 
    }

    for (uint64_t p = bitmap_pages; p < total_pages; ) {
        int order = MAX_ORDER;
        while (order > 0 &&
               ((p & ((1u << order) - 1)) != 0 ||
                p + (1u << order) > total_pages))
            order--;
        buddy_push(p, order);
        p += (1u << order);
    }
}

// ── pmm_alloc_pages ──────────────────────────────────────────────────────────
// Devuelve dirección FÍSICA, o 0 si no hay memoria.
uint64_t pmm_alloc_pages(int order)
{
    if (order < 0 || order > MAX_ORDER) return 0;

    for (int o = order; o <= MAX_ORDER; ++o) {
        uint64_t pg = buddy_pop(o);
        if (pg == (uint64_t)-1) continue;

        while (o > order) {
            --o;
            buddy_push(pg + (1u << o), o);
        }

        for (uint64_t i = 0; i < (1u << order); ++i)
            bm_set(pg + i);

        free_count -= (1u << order);
        return mem_start_pa + pg * PAGE_SIZE;
    }
    return 0;
}

// ── pmm_free_pages ───────────────────────────────────────────────────────────
void pmm_free_pages(uint64_t paddr, int order)
{
    if (!paddr || paddr < mem_start_pa) return;
    uint64_t pg = (paddr - mem_start_pa) / PAGE_SIZE;

    for (uint64_t i = 0; i < (1u << order); ++i)
        bm_clear(pg + i);

    free_count += (1u << order);
    buddy_push(pg, order);
}

// ── Información ──────────────────────────────────────────────────────────────
uint64_t pmm_free_page_count(void) { return free_count; }
uint64_t pmm_total_pages(void)     { return total_pages; }

// ── alloc_page_early ─────────────────────────────────────────────────────────
// Interfaz usada por vmm.c y paging.c durante el arranque temprano.
// En la zona identity-mapeada (0–512 MB): virt == phys, así que podemos
// devolver el puntero directamente.
void *alloc_page_early(void)
{
    uint64_t paddr = pmm_alloc_pages(0);
    return paddr ? (void *)paddr : (void *)0;
}
