// kernel/arch/x86_64/idt.c
//
// Cambios respecto a la versión anterior:
//   • #DF (vector 8) ahora usa IST=1: tiene su propio stack dedicado
//     (definido en tss.c), por lo que si el stack del kernel se desborda
//     el handler de Double Fault sigue funcionando en lugar de triple-faultar.
//   • NMI (vector 2) también usa IST=2 para el mismo motivo.

#include <stdint.h>
#include <stddef.h>

/* Minimal serial helpers for diagnostics (local to this file) */
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static void serial_c(char c) { while (!(inb(0x3FD) & 0x20)); outb(0x3F8, (uint8_t)c); }
static void serial_s(const char *s) { for (; *s; ++s) { if (*s == '\n') serial_c('\r'); serial_c(*s); } }
static void serial_u(uint64_t v) { char buf[21]; int n = 0; if (!v) { serial_c('0'); return; } while (v) { buf[n++] = '0' + (int)(v % 10); v /= 10; } for (int i = n-1; i >= 0; --i) serial_c(buf[i]); }

#define IDT_ENTRIES     256
#define ISR_STUB_SIZE   16

struct __attribute__((packed)) idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};

struct __attribute__((packed)) idtr {
    uint16_t limit;
    uint64_t base;
};

static struct idt_entry idt[IDT_ENTRIES] __attribute__((aligned(16)));
static struct idtr      idtr_descr;

extern void load_idt(struct idtr *p);
extern char isr_stubs_base[];

typedef void (*interrupt_handler_t)(void *);
static interrupt_handler_t idt_handlers[IDT_ENTRIES];

/* ─── idt_set_entry ────────────────────────────────────────────────────── */
static void idt_set_entry(int vec, void *handler, uint8_t ist,
                          uint8_t type_attr, uint16_t sel)
{
    uint64_t addr = (uint64_t)handler;
    idt[vec].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[vec].selector    = sel;
    idt[vec].ist         = ist & 0x7;
    idt[vec].type_attr   = type_attr;
    idt[vec].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vec].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vec].zero        = 0;
}

/* ─── idt_register_handler ─────────────────────────────────────────────── */
void idt_register_handler(int vec, interrupt_handler_t h)
{
    if (vec >= 0 && vec < IDT_ENTRIES)
        idt_handlers[vec] = h;
}

/* ─── idt_init ─────────────────────────────────────────────────────────── */
void idt_init(void)
{
    for (int i = 0; i < IDT_ENTRIES; ++i) {
        uintptr_t stub_addr = (uintptr_t)isr_stubs_base + (i * 16);

        uint8_t ist = 0;
        if (i == 2) ist = 2;  /* NMI */
        if (i == 8) ist = 1;  /* #DF */

        // 2. Pasamos la dirección calculada (casteada a void*)
        idt_set_entry(i, (void *)stub_addr, ist, 0x8E, 0x08);
    }

    idtr_descr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr_descr.base  = (uint64_t)idt;
    load_idt(&idtr_descr);
}

/* ─── isr_dispatch ─────────────────────────────────────────────────────── */
void isr_dispatch(int vec, void *regs)
{
    /* Diagnostic: log the vector number occasionally to narrow down storms */
    if ((vec >= 0x20 && vec <= 0x2F) || (vec < 32)) {
        serial_s("[ISR] vec="); serial_u((uint64_t)vec); serial_c('\n');
    }

    if (vec >= 0 && vec < IDT_ENTRIES && idt_handlers[vec]) {
        idt_handlers[vec](regs);
    } else {
        for (;;) { asm volatile("cli; hlt"); }
    }
}
