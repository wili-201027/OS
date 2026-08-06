// kernel/objects/elf64.c
// FIX: VIRT_OFFSET cambiado a 0 (identity mapping).

#include <stdint.h>
#include <stddef.h>

#define ELF_MAGIC   0x464C457F
#define PT_LOAD     1

#define PTE_P   (1ULL << 0)
#define PTE_RW  (1ULL << 1)
#define PTE_US  (1ULL << 2)

typedef struct {
    uint32_t magic;
    uint8_t  class, data, version, osabi;
    uint8_t  pad[8];
    uint16_t type, machine;
    uint32_t version2;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} __attribute__((packed)) elf64_hdr_t;

typedef struct {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
} __attribute__((packed)) elf64_phdr_t;

static inline uint64_t elf_flags_to_pte(uint32_t elf_flags)
{
    uint64_t pte = PTE_P | PTE_US;
    if (elf_flags & 2) pte |= PTE_RW;
    return pte;
}

static int elf_validate(elf64_hdr_t *hdr)
{
    return hdr->magic   == ELF_MAGIC &&
           hdr->class   == 2         &&
           hdr->machine == 0x3E;
}

uint64_t elf_entry(void *image)
{
    return ((elf64_hdr_t*)image)->entry;
}

int elf_load(void *image, void *address_space)
{
    elf64_hdr_t *hdr = (elf64_hdr_t*)image;
    if (!elf_validate(hdr)) return -1;

    elf64_phdr_t *ph = (elf64_phdr_t*)((uint8_t*)image + hdr->phoff);

    extern void     vmm_map(void *, uint64_t, uint64_t, uint64_t);
    extern uint64_t pmm_alloc_pages(int);

    for (uint16_t i = 0; i < hdr->phnum; i++) {
        if (ph[i].type != PT_LOAD) continue;

        uint64_t seg_vaddr  = ph[i].vaddr;
        uint64_t seg_filesz = ph[i].filesz;
        uint64_t seg_memsz  = ph[i].memsz;
        uint8_t *src        = (uint8_t*)image + ph[i].offset;
        uint64_t pte_flags  = elf_flags_to_pte(ph[i].flags);
        uint64_t pages      = (seg_memsz + 4095) >> 12;

        for (uint64_t p = 0; p < pages; p++) {
            uint64_t phys = pmm_alloc_pages(0);
            if (!phys) return -2;

            vmm_map(address_space,
                    seg_vaddr + p * 4096,
                    phys,
                    pte_flags);

            // FIX: identity mapping → acceder físico directamente (sin VIRT_OFFSET)
            uint8_t *dst  = (uint8_t *)phys;
            uint64_t foff = p * 4096;
            uint64_t copy = 0;
            if (foff < seg_filesz)
                copy = (seg_filesz - foff > 4096) ? 4096 : (seg_filesz - foff);

            for (uint64_t b = 0; b < copy; b++)
                dst[b] = src[foff + b];
            for (uint64_t b = copy; b < 4096; b++)
                dst[b] = 0;
        }
    }
    return 0;
}
