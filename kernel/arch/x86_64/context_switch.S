/* kernel/arch/x86_64/context_switch.S
 *
 * void switch_to(void *old_ctx, void *new_ctx)
 *
 * cpu_context_t layout (scheduler.h):
 *   0x00  r15
 *   0x08  r14
 *   0x10  r13
 *   0x18  r12
 *   0x20  r11
 *   0x28  r10
 *   0x30  r9
 *   0x38  r8
 *   0x40  rbp
 *   0x48  rdi
 *   0x50  rsi
 *   0x58  rdx
 *   0x60  rcx
 *   0x68  rbx
 *   0x70  rax
 *   0x78  rsp     (RSP del task a restaurar)
 *   0x80  rip     (entry point o retorno)
 *   0x88  cs      (selector de código, e.g. 0x1B para usuario)
 *   0x90  rflags
 *   0x98  ss      (selector de datos, e.g. 0x23 para usuario)
 *   0xA0  cr3     (CR3 para user task, 0 para kernel task)
 *
 * Si cs == 0x08 (kernel): ret simple.
 * Si cs == 0x1B (usuario): iretq con ss/rsp/rflags/cs/rip en el stack.
 *
 * NOTA sobre iretq:
 *   iretq espera en el stack (dirección creciente):
 *     [RSP+0]  = RIP
 *     [RSP+8]  = CS
 *     [RSP+16] = RFLAGS
 *     [RSP+24] = RSP (usuario)
 *     [RSP+32] = SS  (usuario)
 *   => hay que hacer push en ORDEN INVERSO: ss, rsp_user, rflags, cs, rip
 *
 * NOTA sobre CR3:
 *   Si cr3 != 0, cargar la tabla de páginas del task JUSTO ANTES de iretq.
 *   Esto evita que el código del kernel se crashee tras cambiar CR3.
 */

    .text
    .global switch_to
switch_to:
    /* RDI = old_ctx (puede ser NULL), RSI = new_ctx */

    /* ── Guardar contexto actual (si old_ctx != NULL) ───────────────────── */
    test %rdi, %rdi
    jz   .Lrestore

    mov %r15, 0x00(%rdi)
    mov %r14, 0x08(%rdi)
    mov %r13, 0x10(%rdi)
    mov %r12, 0x18(%rdi)
    mov %r11, 0x20(%rdi)
    mov %r10, 0x28(%rdi)
    mov %r9,  0x30(%rdi)
    mov %r8,  0x38(%rdi)
    mov %rbp, 0x40(%rdi)
    mov %rdi, 0x48(%rdi)
    mov %rsi, 0x50(%rdi)
    mov %rdx, 0x58(%rdi)
    mov %rcx, 0x60(%rdi)
    mov %rbx, 0x68(%rdi)
    mov %rax, 0x70(%rdi)

    /* Guardar RSP + RIP (retorno a quien llamó switch_to) */
    mov %rsp, 0x78(%rdi)
    lea .Lreturn(%rip), %rax
    mov %rax, 0x80(%rdi)

    /* Guardar selectores y rflags */
    mov %cs,  %eax;  movl %eax, 0x88(%rdi)
    pushfq;          popq  0x90(%rdi)
    mov %ss,  %eax;  movl %eax, 0x98(%rdi)

.Lrestore:
    /* ── Restaurar registros generales (excepto rsp/rip/rsi) ────────────── */
    mov 0x00(%rsi), %r15
    mov 0x08(%rsi), %r14
    mov 0x10(%rsi), %r13
    mov 0x18(%rsi), %r12
    mov 0x20(%rsi), %r11
    mov 0x28(%rsi), %r10
    mov 0x30(%rsi), %r9
    mov 0x38(%rsi), %r8
    mov 0x40(%rsi), %rbp
    mov 0x48(%rsi), %rdi
    mov 0x58(%rsi), %rdx
    mov 0x60(%rsi), %rcx
    mov 0x68(%rsi), %rbx
    mov 0x70(%rsi), %rax

    /* ── Decidir: ¿kernel (ret) o usuario (iretq)? ──────────────────────── */
    movl 0x88(%rsi), %ecx      /* cs */
    cmp  $0x08, %ecx
    je   .Lkernel_ret

    /* ── Salto a USUARIO: construir frame para iretq en stack de usuario ── */
    /* NOTA CRÍTICA: Leer todos los valores del contexto ANTES de cambiar CR3,
     * porque después CR3 el kernel stack no será accesible.
     * Usar R12-R15 como temporales para guardar valores.
     */
    mov 0x78(%rsi), %r12       /* RSP usuario */
    mov 0x80(%rsi), %r13       /* RIP */
    movl 0x88(%rsi), %r14d     /* CS (32-bit) */
    mov 0x90(%rsi), %r15       /* RFLAGS */
    mov 0x98(%rsi), %r8        /* SS */
    mov 0xA0(%rsi), %r9        /* CR3 */
    mov 0x50(%rsi), %r10       /* RSI nuevo */
    
    /* Cargar CR3 ahora (si no es cero) */
    test %r9, %r9
    jz .Lskip_cr3_load
    mov %r9, %cr3
.Lskip_cr3_load:
    
    /* Cambiar a user stack */
    mov %r12, %rsp
    
    /* Construir frame para iretq en el user stack (ahora mapeado) */
    /* iretq espera:
     *   [RSP]    = RIP
     *   [RSP+8]  = CS
     *   [RSP+16] = RFLAGS
     *   [RSP+24] = RSP_usuario
     *   [RSP+32] = SS
     */
    sub $40, %rsp               /* Allocate 5 slots (40 bytes) */
    mov %r8,  32(%rsp)          /* SS */
    mov %r12, 24(%rsp)          /* RSP_usuario */
    mov %r15, 16(%rsp)          /* RFLAGS */
    mov %r14, 8(%rsp)           /* CS */
    mov %r13, 0(%rsp)           /* RIP */
    
    mov %r10, %rsi              /* Restore RSI */

    iretq

.Lkernel_ret:
    /* ── Salto a KERNEL: restaurar RSP y hacer ret ───────────────────────── */
    mov 0x78(%rsi), %rsp   /* restaurar RSP */
    mov 0x80(%rsi), %rbx   /* RIP guardado */
    mov 0x50(%rsi), %rsi   /* restaurar RSI */
    jmp *%rbx

.Lreturn:
    ret
