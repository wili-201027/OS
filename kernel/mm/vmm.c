// kernel/mm/vmm.c
//
// FIX CRÍTICO: vmm_create_address_space ya NO copia PML4[0] del kernel.
//
// El bug: copiar PML4[0] hacía que el proceso de usuario compartiera el
// PDPT del kernel. Cuando map_page_to_pml4 mapeaba páginas del ELF en el
// rango 0x400000+ (que estaba cubierto por el PDPT del kernel via una
// huge page 2MB), modificaba el PDPT del kernel directamente. Esto
// corrompía el mapping de 0x402000 (la dirección física del propio PML4
// del usuario), haciendo que lecturas posteriores de ese PML4 devolvieran
// bytes de código ELF en lugar de entradas de tabla válidas.
//
// La solución: el proceso de usuario tiene un PML4 completamente vacío en
// el rango bajo (0-127TB). El kernel vive en el rango alto (PML4[256..511]).
// Las páginas del ELF se mapean en el PML4 del usuario con PDPTs/PDs/PTs
// propios, sin compartir nada con el kernel.

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define PTE_P  (1ULL << 0)
#define PTE_RW (1ULL << 1)
#define PTE_US (1ULL << 2)

typedef uint64_t pte_t;

extern void     map_page_to_pml4(uint64_t pml4_phys, uint64_t vaddr,
                                  uint64_t paddr, uint64_t flags);
extern uint64_t paging_get_pml4_phys(void);
extern uint64_t pmm_alloc_pages(int order);

static inline void outb_dx(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb_dx(uint16_t port) {
    uint8_t v;
    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static void serial_putc(char c) {
    while (!(inb_dx(0x3FD) & 0x20)) {}
    outb_dx(0x3F8, (uint8_t)c);
}
static void serial_puts(const char *s) {
    while (s && *s) { if (*s == '\n') serial_putc('\r'); serial_putc(*s++); }
}
static void serial_hex64(uint64_t v) {
    static const char h[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 15; i >= 0; --i) serial_putc(h[(v >> (i*4)) & 0xF]);
}

// Identity mapping
static inline pte_t *phys_to_virt(uint64_t paddr) { return (pte_t *)paddr; }

void *vmm_create_address_space(void)
{
    serial_puts("[vmm] creating address space\n");

    uint64_t paddr = pmm_alloc_pages(0);
    if (!paddr) { serial_puts("[vmm] ERROR: pmm_alloc_pages\n"); return (void*)0; }
    serial_puts("[vmm] new PML4 paddr="); serial_hex64(paddr); serial_puts("\n");

    // Limpiar las 512 entradas — el rango bajo (0-255) queda COMPLETAMENTE VACÍO.
    // El proceso de usuario no hereda NINGUNA entrada del kernel en el low half.
    pte_t *new_pml4 = phys_to_virt(paddr);
    for (int i = 0; i < 512; ++i) new_pml4[i] = 0;

    // Leer el PML4 del kernel (CR3 actual)
    uint64_t kernel_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(kernel_cr3));
    uint64_t kernel_pml4_phys = kernel_cr3 & ~0xFFFULL;
    pte_t *kernel_pml4 = phys_to_virt(kernel_pml4_phys);

    // Copiar SOLO las entradas del high-half (256-511) para acceso al kernel.
    // Con identity mapping el kernel está en PML4[0], pero NO lo copiamos
    // para evitar compartir el PDPT del kernel con el proceso de usuario.
    //
    // En lugar de eso, mapeamos el kernel también en PML4[256] del proceso
    // de usuario usando una nueva entrada que apunta al mismo PDPT.
    // Esto permite syscalls (kernel stack permanece accesible) sin compartir
    // el low-half donde viviría el ELF del usuario.
    for (int i = 256; i < 512; ++i)
        new_pml4[i] = kernel_pml4[i];

    // Además: si el kernel está en PML4[0] (identity mapping), copiar esa
    // entrada también al PML4[256] del usuario para que el kernel sea
    // accesible desde modo usuario durante syscalls.
    // PERO no en PML4[0] del usuario (para evitar el bug de PDPT compartido).
    // Por ahora, el kernel del usuario se accede sólo vía PML4[256+].
    // Si PML4[256] no existe en el kernel, crear una entrada para él
    // que apunte al mismo PDPT que PML4[0] del kernel.
    if (!(new_pml4[256] & PTE_P) && (kernel_pml4[0] & PTE_P)) {
        // Reusar el PDPT del kernel en PML4[256] del usuario (read-only desde usuario)
        // No se modifica nada del PDPT — solo se añade una entrada de PML4.
        new_pml4[256] = (kernel_pml4[0] & ~0xFFFULL) | PTE_P | PTE_RW;
    }

    serial_puts("[vmm] address space ready\n");
    return (void *)paddr;
}

void vmm_map(void *address_space, uint64_t vaddr, uint64_t paddr, uint64_t flags)
{
    uint64_t pml4_phys = (uint64_t)address_space;
    if (!pml4_phys) return;
    serial_puts("[vmm_map] va="); serial_hex64(vaddr);
    serial_puts(" pa="); serial_hex64(paddr); serial_puts("\n");
    map_page_to_pml4(pml4_phys, vaddr, paddr, flags);
}

void vmm_switch(void *address_space)
{
    if (!address_space) return;
    uint64_t cr3 = (uint64_t)address_space & ~0xFFFULL;
    serial_puts("[vmm_switch] CR3="); serial_hex64(cr3); serial_puts("\n");
    asm volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

void vmm_init(void) {}
void vmm_destroy(void *space) { (void)space; }
