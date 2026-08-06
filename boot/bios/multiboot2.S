.set MB2_MAGIC, 0xE85250D6
.set MB2_ARCH,  0
.set MB2_LEN,   header_end - header_start
.set MB2_CHK, -(MB2_MAGIC + MB2_ARCH + MB2_LEN)

.section .multiboot
header_start:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_LEN
    .long MB2_CHK
    .short 0
    .short 0
    .long 8
header_end:

.section .text
.code32
.global multiboot2_entry
multiboot2_entry:
    mov %cr4, %eax
    or $0x20, %eax         # PAE
    mov %eax, %cr4

    mov $pml4, %eax
    mov %eax, %cr3

    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0

    ljmp $0x28, $long_mode_entry

.code64
long_mode_entry:
    mov $0x30, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    mov $0x100000, %rax    # kernel entry
    jmp *%rax

.align 4096
pml4:
    .quad pdpt | 0x03

pdpt:
    .quad pd | 0x03

pd:
    .rept 512
    .quad (.-pd) * 0x200000 | 0x83
    .endr
