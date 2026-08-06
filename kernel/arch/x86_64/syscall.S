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

    /* Guardar RSP del usuario y cargar RSP del kernel (de cpu_local/TSS)
     * En esta fase simple usamos la pila del kernel que ya tenemos activa.
     * Una implementación completa guardaría RSP usuario en cpu_local y
     * cargaría el RSP del kernel desde ahí. */

    /* Construir el bloque de registros para syscall_handler(uint64_t *regs).
     * Layout del bloque (offsets de 8B desde RSP, de más bajo a más alto):
     *   [0]  r9   [1]  r8   [2]  r10   [3]  rdx
     *   [4]  rsi  [5]  rdi  [6]  rax   [7]  r11 (rflags usuario)
     *   [8]  rcx (rip usuario)
     *
     * Se pushean en orden inverso para que el array quede en orden ascendente.
     */
    push %rcx           /* [8] RIP usuario */
    push %r11           /* [7] RFLAGS usuario */
    push %rax           /* [6] número de syscall */
    push %rdi           /* [5] arg1 */
    push %rsi           /* [4] arg2 */
    push %rdx           /* [3] arg3 */
    push %r10           /* [2] arg4 (en syscalls Linux r10 sustituye rcx) */
    push %r8            /* [1] arg5 */
    push %r9            /* [0] arg6 */

    /* RSP ahora apunta al bloque → pasarlo como primer argumento */
    mov  %rsp, %rdi
    call syscall_handler
    /* Valor de retorno en RAX */

    /* Restaurar en orden inverso */
    pop %r9
    pop %r8
    pop %r10
    pop %rdx
    pop %rsi
    pop %rdi
    /* Descartar el slot de RAX (ya tenemos el retorno en RAX) */
    add $8, %rsp
    pop %r11            /* RFLAGS usuario */
    pop %rcx            /* RIP usuario */

    /* Restaurar GS del usuario */
    swapgs

    /* Volver a modo usuario */
    sysretq
