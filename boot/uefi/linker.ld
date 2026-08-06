OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS {
    text PT_LOAD FILEHDR PHDRS ;
    data PT_LOAD ;
}

SECTIONS
{
    . = 0x100000;

    /* ── Código ──────────────────────────────────────────────────────────── */
    .text : ALIGN(4K)
    {
        KEEP(*(.boot))       /* Multiboot2 header: debe ser el primer byte */
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4K) { *(.rodata .rodata.*) } :text

    /* ── Datos inicializados ─────────────────────────────────────────────── */
    .data : ALIGN(4K) { *(.data .data.*) } :data

    /* ── BSS (cero-inicializado por el kernel en arranque) ───────────────── */
    .bss : ALIGN(4K)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    } :data

    /* ── Símbolos de initrd ───────────────────────────────────────────────
     * GRUB carga el módulo 'module2' en alguna dirección física y nos pasa
     * su dirección a través del tag Multiboot2 de tipo 3 (módulo).
     * parse_multiboot2() en kernel.cpp actualiza estos punteros en tiempo
     * de ejecución; aquí solo necesitamos que el símbolo esté definido
     * para que el enlazador no se queje.
     * Declaración en C: extern uint8_t initrd_start[]; / initrd_end[];
     */
    . = ALIGN(4K);
    initrd_start = .;
    initrd_end   = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note.GNU-stack)
        *(.note.*)
    }
}
