.global _start
_start:
    mov %rsp, %rdi
    call main
1:  jmp 1b
