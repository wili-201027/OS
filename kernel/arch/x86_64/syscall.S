/* kernel/arch/x86_64/syscall.S
 *
 * Entry point SYSCALL (x86-64 ABI de Linux, adaptado).
 *
 * Convención al entrar (syscall instruction):
 *   RAX = número de syscall
 *   RDI, RSI, RDX, R10, R8, R9 = argumentos 1-6
 *   RCX = RIP de retorno (guardado por SYSCALL)
 *   R11 = RFLAGS de usuario (guardado por SYSCALL)
 *
 * El MSR LSTAR debe apuntar a syscall_entry.
 * El MSR STAR debe configurar los selectores CS/SS para kernel y usuario.
 * El MSR SFMASK debe enmascarar al menos IF para evitar interrupciones
 * anidadas antes de hacer swapgs.
 *
 * Registros que el kernel puede modificar libremente (caller-saved en C ABI):
 *   RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11
 *
 * Registros que debemos preservar (callee-saved):
 *   RBX, RBP, R12-R15 — no los tocamos en el asm, solo los pasa el C handler.
 *
 * NOTA DE SEGURIDAD:
 *   • swapgs ANTES de cualquier acceso a memoria del kernel via GS.
 *   • sysretq es seguro solo si RCX no está bajo control del usuario con
 *     una dirección no-canónica → la syscall_handler debe validar RCX.
 *   • R11 (RFLAGS usuario) se restaura tal cual; el kernel no lo modifica.
 */

    .text
    .global syscall_entry
syscall_entry:
    /* Cambiar al GS del kernel (apunta a cpu_local_t) */
    swapgs

    /* Guardar todos los registros de usuario necesarios para reanudar la tarea
     * si el syscall decide ceder la CPU.
     */
    push %r15
    push %r14
    push %r13
    push %r12
    push %rbp
    push %rbx

    /* Guardar el valor original de RSP antes de empujar los argumentos */
    lea 48(%rsp), %r12

    push %r9
    push %r8
    push %r10
    push %rdx
    push %rsi
    push %rdi
    push %rax           /* número de syscall */
    push %r11           /* RFLAGS usuario */
    push %rcx           /* RIP usuario */

    /* RSP ahora apunta al registro RCX guardado; el arreglo regs[] usa el orden
     * [r9,r8,r10,rdx,rsi,rdi,rax,r11,rcx] para facilitar el despachador. */
    lea 64(%rsp), %rdi
    call syscall_handler
    /* Valor de retorno en RAX */

    /* Restaurar registros de usuario y preservar el valor de retorno en RAX. */
    mov %rax, %r10
    pop %rcx
    pop %r11
    pop %rax
    mov %r10, %rax
    pop %rdi
    pop %rsi
    pop %rdx
    pop %r10
    pop %r8
    pop %r9
    pop %rbx
    pop %rbp
    pop %r12
    pop %r13
    pop %r14
    pop %r15

    /* Restaurar GS del usuario */
    swapgs

    /* Volver a modo usuario */
    sysretq
