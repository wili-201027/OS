#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void io_wait(void) { outb(0x80, 0); }

void pic_remap(void)
{
    // ICW1: iniciar inicialización en cascada
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();

    // ICW2: remap  IRQ0-7 → vectores 0x20-0x27
    //              IRQ8-15 → vectores 0x28-0x2F
    outb(0x21, 0x20); io_wait();
    outb(0xA1, 0x28); io_wait();

    // ICW3: cascada (maestro en IRQ2)
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();

    // ICW4: modo 8086
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    // OCW1: máscaras
    //   Maestro: 0xFC = 1111 1100  →  solo IRQ0 (timer) e IRQ1 (teclado) habilitados
    //   Esclavo:  0xFF = todo enmascarado (no necesitamos disco, etc. todavía)
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

// Envía End-Of-Interrupt al 8259 PIC (debe llamarse al FINAL del handler de IRQ)
// vec = número de vector recibido (0x20-0x2F)
void pic_eoi(uint8_t vec)
{
    if (vec >= 0x28)          // IRQ de esclavo (8-15)
        outb(0xA0, 0x20);     // EOI al PIC esclavo
    outb(0x20, 0x20);         // EOI al PIC maestro (siempre)
}
