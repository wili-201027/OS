.code16
.global stage2_start

stage2_start:
    cli
    call enable_a20

    lgdt gdt_desc

    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0
    ljmp $0x08, $protected_entry

enable_a20:
    in $0x92, %al
    or $0x02, %al
    out %al, $0x92
    ret

.code32
protected_entry:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x90000, %esp

    call load_kernel
    jmp multiboot2_entry

load_kernel:
    mov $0x100000, %edi    # kernel load addr
    mov $0x02, %ah
    mov $0x20, %al         # sectors
    mov $0x00, %ch
    mov $0x04, %cl
    mov $0x00, %dh
    mov $0x80, %dl
    int $0x13
    ret

.align 8
gdt:
    .quad 0
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF

gdt_desc:
    .word gdt_desc - gdt - 1
    .long gdt
