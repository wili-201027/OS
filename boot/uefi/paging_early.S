.section .text
.global enable_early_paging

enable_early_paging:
    mov %cr4, %rax
    or $(1 << 5), %rax        # PAE
    mov %rax, %cr4

    mov %cr0, %rax
    or $(1 << 31), %rax       # PG
    mov %rax, %cr0

    ret
