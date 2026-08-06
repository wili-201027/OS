.intel_syntax noprefix
.section .text
.global syscall0

syscall0:
    mov rax, rdi    # El primer argumento (n) pasa a RAX
    syscall         # ¡Salto al Kernel!
    ret

.global syscall1
syscall1:
    /* syscall1(uint64_t n, uint64_t a1) */
    mov rax, rdi    /* n -> rax */
    mov rdi, rsi    /* a1 -> rdi */
    syscall
    ret

.global syscall2
syscall2:
    /* syscall2(uint64_t n, uint64_t a1, uint64_t a2) */
    mov rax, rdi    /* n -> rax */
    mov rdi, rsi    /* a1 -> rdi */
    mov rsi, rdx    /* a2 -> rsi */
    syscall
    ret

.global syscall3
syscall3:
    /* syscall3(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3) */
    mov rax, rdi    /* n -> rax */
    mov rdi, rsi    /* a1 -> rdi */
    mov rsi, rdx    /* a2 -> rsi */
    mov rdx, rcx    /* a3 -> rdx */
    syscall
    ret
