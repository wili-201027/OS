.code16
.global _start
_start:
    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x7C00, %sp
    sti

    mov $0x7E00, %bx        # Stage2 load address
    mov $0x02, %ah         # BIOS read sectors
    mov $0x01, %al         # sectors to read
    mov $0x00, %ch
    mov $0x02, %cl         # sector 2
    mov $0x00, %dh
    mov $0x80, %dl         # first HDD
    int $0x13
    jc disk_error

    jmp 0x0000:0x7E00

disk_error:
    hlt
    jmp disk_error

.org 510
.word 0xAA55
