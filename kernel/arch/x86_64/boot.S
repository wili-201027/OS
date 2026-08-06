.section .boot
.code32

# ── Multiboot2 header con tag de framebuffer ──────────────────────────────────
.align 8
multiboot_header:
    .long  0xe85250d6
    .long  0
    .long  multiboot_header_end - multiboot_header
    .long  -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header))

    # Tag tipo 5: framebuffer 1024x768x32
    .align 8
    .short 5          # type
    .short 1          # flags = 1 (optional, no failure si no disponible)
    .long  20         # size
    .long  1024       # width
    .long  768        # height
    .long  32         # bpp

    # End tag
    .align 8
    .short 0
    .short 0
    .long  8
multiboot_header_end:

# ── Guardar argumentos GRUB antes de tocar registros ─────────────────────────
.section .data
.align 4
saved_mb2_info:  .long 0
saved_mb2_magic: .long 0

.section .boot
.code32
.global _start
_start:
    cli
    movl %ebx, saved_mb2_info
    movl %eax, saved_mb2_magic
    movl $stack_top, %esp

    # ── Limpiar 5 tablas (PML4 + PDPT + PD_low + PD_mid + PD_hi) ─────────────
    movl $p4_table, %ebp
    movl $(5 * 1024), %ecx
    xorl %eax, %eax
.Lclear:
    movl %eax, (%ebp)
    addl $4, %ebp
    decl %ecx
    jnz  .Lclear

    # PML4[0] → PDPT
    movl $p3_table, %eax
    orl  $0x03, %eax
    movl %eax, p4_table
    movl $0,   p4_table + 4

    # PDPT[0] → PD_low (0–1 GB)
    movl $p2_table_low, %eax
    orl  $0x03, %eax
    movl %eax, p3_table
    movl $0,   p3_table + 4

    # PDPT[1] → PD_mid (1–2 GB: cubre MB2 info con -m 2G)
    movl $p2_table_mid, %eax
    orl  $0x03, %eax
    movl %eax, p3_table + 8
    movl $0,   p3_table + 12

    # PDPT[3] → PD_hi  (3–4 GB: LAPIC 0xFEE00000, VGA FB 0xE0000000+)
    movl $p2_table_hi, %eax
    orl  $0x03, %eax
    movl %eax, p3_table + 24
    movl $0,   p3_table + 28

    # PD_low: 0–1 GB identidad (512 × 2MB)
    movl $p2_table_low, %ebp
    xorl %ecx, %ecx
.Llow:
    movl %ecx, %eax
    shll $21, %eax
    orl  $0x83, %eax
    movl %eax, (%ebp)
    movl $0,   4(%ebp)
    addl $8, %ebp
    incl %ecx
    cmpl $512, %ecx
    jne  .Llow

    # PD_mid: 1–2 GB identidad (512 × 2MB)
    movl $p2_table_mid, %ebp
    xorl %ecx, %ecx
.Lmid:
    movl %ecx, %eax
    shll $21, %eax
    addl $0x40000000, %eax
    orl  $0x83, %eax
    movl %eax, (%ebp)
    movl $0,   4(%ebp)
    addl $8, %ebp
    incl %ecx
    cmpl $512, %ecx
    jne  .Lmid

    # PD_hi: 3–4 GB identidad (512 × 2MB) — VGA FB + LAPIC + PCI
    movl $p2_table_hi, %ebp
    xorl %ecx, %ecx
.Lhi:
    movl %ecx, %eax
    shll $21, %eax
    addl $0xC0000000, %eax
    orl  $0x83, %eax
    movl %eax, (%ebp)
    movl $0,   4(%ebp)
    addl $8, %ebp
    incl %ecx
    cmpl $512, %ecx
    jne  .Lhi

    movl $p4_table, %eax
    movl %eax, %cr3

    movl %cr4, %eax
    orl  $(1 << 5), %eax
    movl %eax, %cr4

    movl $0xC0000080, %ecx
    rdmsr
    orl  $((1 << 8) | 1), %eax
    wrmsr

    movl %cr0, %eax
    orl  $(1 << 31), %eax
    movl %eax, %cr0

    lgdt gdt64_ptr
    ljmp $0x08, $long64

.code64
long64:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movabs $stack_top, %rsp
    movl  saved_mb2_info(%rip),  %edi
    movl  saved_mb2_magic(%rip), %esi
    call kernel_main
.Lidle:
    hlt
    jmp .Lidle

.section .data
.align 8
gdt64:
    .quad 0x0000000000000000
    .quad 0x00af9a000000ffff    # 0x08 kernel code 64-bit
    .quad 0x00af92000000ffff    # 0x10 kernel data
    .quad 0x00affa000000ffff    # 0x18 user code  64-bit (DPL=3)
    .quad 0x00aff2000000ffff    # 0x20 user data         (DPL=3)
gdt64_end:
gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

.global kernel_stack_top
.align 8
kernel_stack_top: .quad stack_top

.section .bss
.align 4096
p4_table:      .skip 4096
p3_table:      .skip 4096
p2_table_low:  .skip 4096
p2_table_mid:  .skip 4096
p2_table_hi:   .skip 4096
.align 16
stack_bottom: .skip 65536
stack_top:
.global stack_top
