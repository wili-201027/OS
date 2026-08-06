.section .text
.global cpu_cli
.global cpu_hlt

cpu_cli:
    cli
    ret

cpu_hlt:
    hlt
    ret
