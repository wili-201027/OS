/* kernel/arch/x86_64/gdt_flush.S
 *
 * Funciones de bajo nivel para cargar GDT / IDT / TSS.
 *
 * load_gdt(struct gdt_ptr *p)  — carga GDT y recarga todos los selectores
 * load_idt(struct idtr  *p)    — carga IDT
 * load_tss(uint16_t sel)       — carga el TSS (ltr)
 */

.section .text
.code64

/* ─── load_gdt ───────────────────────────────────────────────────────────────
 * void load_gdt(struct gdt_ptr *p);
 * RDI = puntero a {uint16_t limit; uint64_t base}
 */
.global load_gdt
load_gdt:
    lgdt (%rdi)

    /* Recargar selectores de datos (0x10 = kernel data) */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Recargar CS (0x08 = kernel code) mediante far-return */
    pushq $0x08                 /* nuevo CS */
    lea   .Lreload_cs(%rip), %rax
    pushq %rax                  /* dirección de retorno */
    lretq                       /* far-return: pop RIP, pop CS */
.Lreload_cs:
    ret

/* ─── load_idt ───────────────────────────────────────────────────────────────
 * void load_idt(struct idtr *p);
 */
.global load_idt
load_idt:
    lidt (%rdi)
    ret

/* ─── load_tss ───────────────────────────────────────────────────────────────
 * void load_tss(uint16_t sel);
 * DI = selector TSS (e.g. 0x28)
 */
.global load_tss
load_tss:
    ltr %di
    ret
