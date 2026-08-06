// kernel/arch/x86_64/gdt.c
//
// GDT completamente auto-contenida: usa un array estático, no memory scratchpad.
// Orden de llamada requerido desde kernel_main():
//   1. gdt_init()    — carga GDT con entradas 0-4, NO carga TSS todavía
//   2. tss_init()    — llama a gdt_set_tss() y luego load_tss(GDT_TSS_SEL)
//
// Selectores:
//   0x00  null
//   0x08  kernel code
//   0x10  kernel data
//   0x18  user code
//   0x20  user data
//   0x28  TSS  (descriptor de 16 bytes: entradas 5+6)

#include <stdint.h>
#include <stddef.h>

#define GDT_NULL        0
#define GDT_KERN_CODE   1
#define GDT_KERN_DATA   2
#define GDT_USER_CODE   3
#define GDT_USER_DATA   4
#define GDT_TSS_LO      5   /* Los dos descriptores del TSS (16 bytes) */
#define GDT_TSS_HI      6
#define GDT_ENTRIES     7

#define GDT_TSS_SEL     (GDT_TSS_LO * 8)   /* = 0x28 */

/* ─── Estructuras ──────────────────────────────────────────────────────────── */
struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
};

struct __attribute__((packed)) gdt_ptr {
    uint16_t limit;
    uint64_t base;
};

/* ─── Almacenamiento estático (alineado) ───────────────────────────────────── */
static struct gdt_entry gdt_table[GDT_ENTRIES] __attribute__((aligned(8)));
static struct gdt_ptr   gdtr __attribute__((aligned(8)));

/* ─── Prototipos externos (ensamblador) ─────────────────────────────────────── */
extern void load_gdt(struct gdt_ptr *p);    /* lgdt + lretq para recargar CS */
extern void load_tss(uint16_t sel);

/* ─── Helper: escribe una entrada de 8 bytes ─────────────────────────────────── */
static void set_entry(int idx, uint32_t base, uint32_t limit,
                      uint8_t access, uint8_t gran_flags)
{
    struct gdt_entry *e = &gdt_table[idx];
    e->limit_low = (uint16_t)(limit & 0xFFFF);
    e->base_low  = (uint16_t)(base  & 0xFFFF);
    e->base_mid  = (uint8_t)((base >> 16) & 0xFF);
    e->access    = access;
    e->gran      = (uint8_t)(((limit >> 16) & 0x0F) | (gran_flags & 0xF0));
    e->base_high = (uint8_t)((base >> 24) & 0xFF);
}

/* ─── gdt_set_tss ─────────────────────────────────────────────────────────────
 * Escribe el descriptor TSS de 16 bytes en las entradas 5 y 6 del GDT.
 * Debe llamarse ANTES de load_tss().
 */
void gdt_set_tss(void *tss_addr, uint32_t tss_size)
{
    uint64_t base  = (uint64_t)tss_addr;
    uint32_t limit = tss_size - 1;

    /* Descriptor bajo (tipo 0x89 = TSS available 64-bit) */
    uint64_t lo = 0;
    lo |= (uint64_t)(limit & 0xFFFF);
    lo |= (uint64_t)(base  & 0xFFFFFF) << 16;
    lo |= (uint64_t)0x89ULL            << 40;   /* P=1, DPL=0, Type=9 */
    lo |= (uint64_t)((limit >> 16) & 0xF) << 48;
    lo |= (uint64_t)((base >> 24) & 0xFF) << 56;

    /* Descriptor alto: bits [63:32] de la base */
    uint64_t hi = (uint64_t)(base >> 32) & 0xFFFFFFFFULL;

    /* Escribir directamente en el array como uint64_t */
    ((uint64_t*)gdt_table)[GDT_TSS_LO] = lo;
    ((uint64_t*)gdt_table)[GDT_TSS_HI] = hi;

    /* Recargar la GDT (el puntero gdtr ya apunta a gdt_table, sólo actualizamos) */
    load_gdt(&gdtr);
}

/* ─── gdt_init ────────────────────────────────────────────────────────────────
 * Inicializa las 5 entradas estándar, carga la GDT.
 * NO carga el TSS (eso lo hace tss_init).
 */
void gdt_init(void)
{
    /* 0: null */
    set_entry(GDT_NULL,      0, 0,          0x00, 0x00);

    /* 1: kernel code 64-bit  (access 0x9A, gran 0xA0 → L=1 flag) */
    set_entry(GDT_KERN_CODE, 0, 0xFFFFFFFF, 0x9A, 0xA0);

    /* 2: kernel data          (access 0x92, gran 0xC0) */
    set_entry(GDT_KERN_DATA, 0, 0xFFFFFFFF, 0x92, 0xC0);

    /* 3: user code  64-bit    (DPL=3) */
    set_entry(GDT_USER_CODE, 0, 0xFFFFFFFF, 0xFA, 0xA0);

    /* 4: user data             (DPL=3) */
    set_entry(GDT_USER_DATA, 0, 0xFFFFFFFF, 0xF2, 0xC0);

    /* 5,6: TSS — reservadas a 0 por ahora; tss_init las rellena */
    ((uint64_t*)gdt_table)[GDT_TSS_LO] = 0;
    ((uint64_t*)gdt_table)[GDT_TSS_HI] = 0;

    /* Configurar GDTR */
    gdtr.limit = (uint16_t)(sizeof(gdt_table) - 1);
    gdtr.base  = (uint64_t)gdt_table;

    /* Cargar GDT y recargar todos los selectores de segmento */
    load_gdt(&gdtr);
    /* load_gdt ya recarga CS=0x08 y DS/ES/FS/GS/SS=0x10 internamente */
}
