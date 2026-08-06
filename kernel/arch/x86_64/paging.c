// kernel/arch/x86_64/paging.c
// FIX: VIRT_OFFSET cambiado de 0xFFFF800000000000 a 0.
// El kernel corre con identity mapping (físico == virtual).
// Usar el offset higher-half causaba page faults al acceder tablas de páginas.

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE    4096
#define PT_ENTRIES   512

typedef uint64_t pte_t;

#define PTE_P   (1ULL << 0)
#define PTE_RW  (1ULL << 1)
#define PTE_US  (1ULL << 2)
#define PTE_PS  (1ULL << 7)

// Identity mapping: dirección física == dirección virtual
static inline pte_t *paddr_to_virt(uint64_t paddr) {
    return (pte_t *)paddr;
}

// --- Simple serial helpers (local copy so we can print diagnostics here) ---
static inline void outb_dx(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb_dx(uint16_t port) {
    uint8_t v;
    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static void serial_putc(char c) {
    while (!(inb_dx(0x3FD) & 0x20)) { /* busy-wait */ }
    outb_dx(0x3F8, (uint8_t)c);
}
static void serial_puts(const char *s) {
    while (s && *s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}
static void serial_hex64(uint64_t v) {
    static const char h[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 15; i >= 0; --i)
        serial_putc(h[(v >> (i * 4)) & 0xF]);
}

static inline int is_canonical_addr(uint64_t a) {
    int64_t s = (int64_t)a >> 47;
    return (s == 0) || (s == -1);
}

extern uint64_t pmm_alloc_pages(int order);

static uint64_t new_page_table(void)
{
    uint64_t paddr = pmm_alloc_pages(0);
    if (!paddr) return 0;
    pte_t *v = paddr_to_virt(paddr);
    for (int i = 0; i < PT_ENTRIES; ++i) v[i] = 0;
    return paddr;
}

static uint64_t split_huge_page(uint64_t pde_entry)
{
    uint64_t huge_base  = pde_entry & ~((uint64_t)0x1FFFFF);
    uint64_t huge_flags = (pde_entry & 0xFFF & ~PTE_PS) | PTE_P | PTE_RW;
    uint64_t pt_phys = new_page_table();
    if (!pt_phys) return 0;
    pte_t *pt = paddr_to_virt(pt_phys);
    for (int i = 0; i < PT_ENTRIES; ++i)
        pt[i] = (huge_base + (uint64_t)i * PAGE_SIZE) | huge_flags;
    return pt_phys;
}

static uint64_t g_pml4_phys = 0;

void map_page_to_pml4(uint64_t pml4_phys, uint64_t vaddr,
                      uint64_t paddr, uint64_t flags)
{
    uint64_t p4i = (vaddr >> 39) & 0x1FF;
    uint64_t p3i = (vaddr >> 30) & 0x1FF;
    uint64_t p2i = (vaddr >> 21) & 0x1FF;
    uint64_t p1i = (vaddr >> 12) & 0x1FF;

    uint64_t mid = PTE_P | PTE_RW | (flags & PTE_US);

    pte_t *pml4v = paddr_to_virt(pml4_phys);
    if (!is_canonical_addr((uint64_t)pml4v)) {
        serial_puts("[map_page] pml4v non-canonical\n");
        serial_puts(" pml4_phys="); serial_hex64(pml4_phys); serial_puts("\n");
        return;
    }

    uint64_t entry4 = pml4v[p4i];
    if (!(entry4 & PTE_P)) {
        uint64_t n = new_page_table(); if (!n) return;
        pml4v[p4i] = n | mid;
        entry4 = pml4v[p4i];
    }
    uint64_t p3 = entry4 & ~0xFFFULL;
    if (!is_canonical_addr(p3)) {
        serial_puts("[map_page] p3 non-canonical\n");
        serial_puts(" pml4v[p4i]="); serial_hex64(entry4);
        serial_puts(" p3="); serial_hex64(p3); serial_puts("\n");
        return;
    }

    pte_t *pdptv = paddr_to_virt(p3);
    if (!is_canonical_addr((uint64_t)pdptv)) {
        serial_puts("[map_page] pdptv non-canonical\n");
        serial_puts(" pdpt_phys="); serial_hex64(p3); serial_puts("\n");
        return;
    }

    uint64_t entry3 = pdptv[p3i];
    if (!(entry3 & PTE_P)) {
        uint64_t n = new_page_table(); if (!n) return;
        pdptv[p3i] = n | mid;
        entry3 = pdptv[p3i];
    }
    uint64_t p2 = entry3 & ~0xFFFULL;
    if (!is_canonical_addr(p2)) {
        serial_puts("[map_page] p2 non-canonical\n");
        serial_puts(" pdptv[p3i]="); serial_hex64(entry3);
        serial_puts(" p2="); serial_hex64(p2); serial_puts("\n");
        return;
    }

    pte_t *pdv = paddr_to_virt(p2);
    if (!is_canonical_addr((uint64_t)pdv)) {
        serial_puts("[map_page] pdv non-canonical\n");
        serial_puts(" p2="); serial_hex64(p2); serial_puts("\n");
        return;
    }

    uint64_t entry2 = pdv[p2i];
    if (!(entry2 & PTE_P)) {
        uint64_t n = new_page_table(); if (!n) return;
        pdv[p2i] = n | mid;
        entry2 = pdv[p2i];
    } else if (entry2 & PTE_PS) {
        uint64_t n = split_huge_page(entry2); if (!n) return;
        pdv[p2i] = n | mid;
        asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
        entry2 = pdv[p2i];
    }

    uint64_t p1 = entry2 & ~0xFFFULL;
    if (!is_canonical_addr(p1)) {
        serial_puts("[map_page] p1 non-canonical\n");
        serial_puts(" pdv[p2i]="); serial_hex64(entry2);
        serial_puts(" p1="); serial_hex64(p1); serial_puts("\n");
        return;
    }

    pte_t *ptv = paddr_to_virt(p1);
    if (!is_canonical_addr((uint64_t)ptv)) {
        serial_puts("[map_page] ptv non-canonical\n");
        serial_puts(" p1="); serial_hex64(p1); serial_puts("\n");
        return;
    }

    ptv[p1i] = (paddr & ~0xFFFULL) | (flags & 0xFFFULL) | PTE_P;
    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}

void map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags)
{
    if (!g_pml4_phys) {
        g_pml4_phys = new_page_table();
        if (!g_pml4_phys) return;
    }
    map_page_to_pml4(g_pml4_phys, vaddr, paddr, flags);
}

void identity_map_region(uint64_t start, uint64_t size)
{
    const uint64_t HUGE = 0x200000ULL;
    uint64_t base = start & ~(HUGE - 1);
    uint64_t end  = (start + size + HUGE - 1) & ~(HUGE - 1);
    if (!g_pml4_phys) { g_pml4_phys = new_page_table(); if (!g_pml4_phys) return; }
    for (uint64_t a = base; a < end; a += HUGE) {
        uint64_t p4 = (a >> 39) & 0x1FF;
        uint64_t p3 = (a >> 30) & 0x1FF;
        uint64_t p2 = (a >> 21) & 0x1FF;
        pte_t *pml4v = paddr_to_virt(g_pml4_phys);
        if (!(pml4v[p4] & PTE_P)) {
            uint64_t n = new_page_table(); if (!n) return;
            pml4v[p4] = n | PTE_P | PTE_RW;
        }
        pte_t *pdptv = paddr_to_virt(pml4v[p4] & ~0xFFFULL);
        if (!(pdptv[p3] & PTE_P)) {
            uint64_t n = new_page_table(); if (!n) return;
            pdptv[p3] = n | PTE_P | PTE_RW;
        }
        pte_t *pdv = paddr_to_virt(pdptv[p3] & ~0xFFFULL);
        pdv[p2] = (a & ~(HUGE - 1)) | PTE_P | PTE_RW | PTE_PS;
    }
}

void load_paging(void)
{
    if (!g_pml4_phys) return;
    asm volatile("mov %0, %%cr3" :: "r"(g_pml4_phys) : "memory");
}

uint64_t paging_get_pml4_phys(void) { return g_pml4_phys; }
