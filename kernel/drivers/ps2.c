// kernel/drivers/ps2.c
// Driver PS/2 amb mode no-bloquejant per al compositor.

#include <stdint.h>

static inline uint8_t inb(uint16_t p){
    uint8_t r; asm volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r;
}
static inline void outb(uint16_t p,uint8_t v){
    asm volatile("outb %0,%1"::"a"(v),"Nd"(p));
}

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

// Bit 0 de l'status: output buffer full (dades disponibles)
// Bit 5: dades de ratolí (mouse data)
#define PS2_OBF     0x01
#define PS2_MOUSE   0x20

void ps2_init(void)
{
    // Desactivar dispositius durant la inicialització
    outb(PS2_CMD, 0xAD);   // desactivar port 1 (teclat)
    outb(PS2_CMD, 0xA7);   // desactivar port 2 (ratolí)
    inb(PS2_DATA);         // buidar buffer

    // Llegir i modificar la configuració del controlador
    outb(PS2_CMD, 0x20);
    uint8_t cfg = inb(PS2_DATA);
    // Habilitar IRQ1 (teclat) i IRQ12 (ratolí), desactivar traducció
    cfg |=  0x03;   // bit0=IRQ1, bit1=IRQ12
    cfg &= ~0x40;   // bit6=traducció off
    outb(PS2_CMD, 0x60);
    outb(PS2_DATA, cfg);

    // Activar port 1 (teclat)
    outb(PS2_CMD, 0xAE);

    // Activar port 2 (ratolí)
    outb(PS2_CMD, 0xA8);

    // Enviar Enable Scanning al teclat
    outb(PS2_DATA, 0xF4);
    for(volatile int i=0;i<10000;i++);
    inb(PS2_DATA); // consumir ACK

    // Inicialitzar ratolí: enviar 0xFF (reset) al port 2
    outb(PS2_CMD, 0xD4);  // next byte goes to mouse
    outb(PS2_DATA, 0xFF); // reset
    for(volatile int i=0;i<100000;i++);
    inb(PS2_DATA); // ACK
    inb(PS2_DATA); // 0xAA (self-test OK)
    inb(PS2_DATA); // 0x00 (mouse ID)

    // Activar streaming del ratolí
    outb(PS2_CMD, 0xD4);
    outb(PS2_DATA, 0xF4); // enable reporting
    for(volatile int i=0;i<10000;i++);
    inb(PS2_DATA); // ACK
}

// Llegir scancode teclat sense bloqueig.
// Retorna 0 si no hi ha dades.
uint8_t ps2_read_scancode_nowait(void)
{
    uint8_t status = inb(PS2_STATUS);
    // Bit 0: output buffer full; bit 5: és de ratolí (no volem)
    if(!(status & PS2_OBF)) return 0;
    if(  status & PS2_MOUSE){ inb(PS2_DATA); return 0; } // discard mouse data
    return inb(PS2_DATA);
}

// Llegir byte de ratolí sense bloqueig.
// Retorna 0xFF si no hi ha dades (0 és un byte de ratolí vàlid).
uint8_t ps2_read_mouse_nowait(void)
{
    uint8_t status = inb(PS2_STATUS);
    if(!(status & PS2_OBF)) return 0xFF;
    if(!(status & PS2_MOUSE)){ inb(PS2_DATA); return 0xFF; } // discard kb data
    return inb(PS2_DATA);
}
