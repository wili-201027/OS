/* kernel/arch/x86_64/isr_stubs.S
 *
 * 256 stubs de interrupción, 16 bytes exactos cada uno.
 * Layout tras isr_common_stub (RSP apunta a r15):
 *
 *   +000  r15 ... +112 rax    (15 regs × 8 B)
 *   +120  vector_num
 *   +128  error_code  (real o 0 ficticio)
 *   +136  RIP  +144 CS  +152 RFLAGS  +160 RSP  +168 SS  (CPU)
 */

.section .text
.code64

/* ── Dispatcher común ────────────────────────────────────────────────────── */
isr_common_stub:
    /* Guardar todos los GPR (15 × 8 = 120 bytes) */
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    /* Si venimos de CPL=3, cargamos los segmentos del kernel.
     * Esto es necesario porque SYSRET puede dejar selectores nulos. */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es

    /* arg0 = número de vector (RSP+120 tras los 15 pushes) */
    movq 120(%rsp), %rdi
    /* arg1 = puntero al frame completo */
    movq %rsp,      %rsi

    /* Alinear stack a 16 bytes sin tocar %rbp (ya está en el frame) */
    movq %rsp, %r15          /* scratch temporal — también está en el frame */
    andq $-16,  %rsp
    call isr_dispatch
    movq %r15, %rsp

    /* Restaurar segmentos de datos (pueden haber sido modificados) */
    /* Solo si el handler los pudo cambiar; aquí es defensa en profundidad */

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    /* Descartar vector_num y error_code */
    addq $16, %rsp
    iretq

/* ── Macros de stubs exactamente 16 bytes ───────────────────────────────── */
.macro STUB_NOERR n
    .byte 0x6A, 0x00                        /* push $0  (2 B) */
    .byte 0x68 ; .long \n                   /* push $vec (5 B) */
    .byte 0xE9 ; .long isr_common_stub - (. + 4)  /* jmp rel32 (5 B) */
    .byte 0xCC, 0xCC, 0xCC, 0xCC           /* padding (4 B) */
.endm

.macro STUB_ERR n
    .byte 0x66, 0x90                        /* NOP 2B */
    .byte 0x68 ; .long \n
    .byte 0xE9 ; .long isr_common_stub - (. + 4)
    .byte 0xCC, 0xCC, 0xCC, 0xCC
.endm

/* ── Array de 256 stubs ──────────────────────────────────────────────────── */
.global isr_stubs_base
.p2align 4
isr_stubs_base:
STUB_NOERR  0   /* #DE Divide Error           */
STUB_NOERR  1   /* #DB Debug                  */
STUB_NOERR  2   /*     NMI                    */
STUB_NOERR  3   /* #BP Breakpoint             */
STUB_NOERR  4   /* #OF Overflow               */
STUB_NOERR  5   /* #BR BOUND Range            */
STUB_NOERR  6   /* #UD Invalid Opcode         */
STUB_NOERR  7   /* #NM Device Not Available   */
STUB_ERR    8   /* #DF Double Fault           */
STUB_NOERR  9   /* (obsoleto)                 */
STUB_ERR   10   /* #TS Invalid TSS            */
STUB_ERR   11   /* #NP Segment Not Present    */
STUB_ERR   12   /* #SS Stack Fault            */
STUB_ERR   13   /* #GP General Protection     */
STUB_ERR   14   /* #PF Page Fault             */
STUB_NOERR 15
STUB_NOERR 16   /* #MF x87 FPU               */
STUB_ERR   17   /* #AC Alignment Check        */
STUB_NOERR 18   /* #MC Machine Check          */
STUB_NOERR 19   /* #XM SIMD FP               */
STUB_NOERR 20   /* #VE Virtualization         */
STUB_ERR   21   /* #CP Control Protection     */
STUB_NOERR 22
STUB_NOERR 23
STUB_NOERR 24
STUB_NOERR 25
STUB_NOERR 26
STUB_NOERR 27
STUB_NOERR 28
STUB_ERR   29
STUB_ERR   30   /* #SX Security               */
STUB_NOERR 31

/* IRQs 32-255 */
.set _v, 32
.rept 224
    .byte 0x6A, 0x00
    .byte 0x68 ; .long _v
    .byte 0xE9 ; .long isr_common_stub - (. + 4)
    .byte 0xCC, 0xCC, 0xCC, 0xCC
    .set _v, _v + 1
.endr
