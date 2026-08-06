// kernel/panic.c
#include <stdint.h>

extern void fb_put(uint32_t x,uint32_t y,uint32_t c);
extern void cpu_cli(void);
extern void cpu_hlt(void);

static volatile uint16_t *const VGA_TEXT = (volatile uint16_t *)0xB8000;
static const uint16_t VGA_ATTR = 0x0C00; // red on black

static void vga_putc(char c, uint32_t *x, uint32_t *y)
{
    if (c == '\n' || c == '\r') {
        *x = 0;
        (*y)++;
        return;
    }

    if (*x >= 80) {
        *x = 0;
        (*y)++;
    }
    if (*y >= 25) {
        *y = 24;
    }

    VGA_TEXT[*y * 80 + *x] = VGA_ATTR | (uint8_t)c;
    (*x)++;
}

static void vga_puts(const char *msg, uint32_t *x, uint32_t *y)
{
    while (*msg) {
        vga_putc(*msg++, x, y);
    }
}

void panic(const char *msg)
{
    cpu_cli();

    uint32_t x = 0, y = 0;
    vga_puts("KERNEL PANIC: ", &x, &y);
    if (msg && *msg) {
        vga_puts(msg, &x, &y);
        vga_putc('\n', &x, &y);
    }

    // Fallback diagnostic using framebuffer when available.
    if (msg && *msg) {
        uint32_t fx = 0, fy = 0;
        const char *p = msg;
        while (*p) {
            fb_put(fx++, fy, 0xFF0000);
            p++;
            if (fx >= 80) { fx = 0; fy++; }
        }
    }

    for (;;) cpu_hlt();
}

void assert(int cond, const char *msg)
{
    if (!cond) panic(msg);
}
