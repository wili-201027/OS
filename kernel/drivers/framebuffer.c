// kernel/drivers/framebuffer.c
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static uint32_t *s_fb    = (uint32_t*)0;
static uint32_t  s_w     = 0;
static uint32_t  s_h     = 0;
static uint32_t  s_pitch = 0;

// Simple serial helper for debug
static inline uint8_t inb(uint16_t p) { (void)p; return 0; }
static inline void outb(uint16_t port, uint8_t v) { (void)port; (void)v; }
static void serial_putc(char c) { (void)c; }
static void serial_puts(const char *s){ for(;s&&*s;++s){ if(*s=='\n') serial_putc('\r'); serial_putc(*s);} }

static void serial_putu(uint64_t v){ 
    if(!v){ serial_putc('0'); serial_putc('\n'); return; } 
    char b[32]; int n=0; 
    while(v){ b[n++] = '0' + (int)(v%10); v/=10; } 
    for(int i=n-1;i>=0;--i) serial_putc(b[i]); 
    serial_putc('\n'); 
}

static void serial_hex64(uint64_t v){ 
    static const char h[]="0123456789ABCDEF"; 
    serial_puts("0x"); 
    for(int i=15;i>=0;--i) serial_putc(h[(v>>(i*4))&0xF]); 
    serial_putc('\n');
}

static int cpu_has_sse2(void)
{
    return 0;
}

void fb_copy_pixels(const uint32_t *src, uint32_t *dst, uint32_t pixel_count)
{
    if (!src || !dst || pixel_count == 0) return;

    memcpy(dst, src, pixel_count * sizeof(uint32_t));
}

void fb_init(void *addr, uint32_t w, uint32_t h, uint32_t pitch)
{
    s_fb    = (uint32_t*)addr;
    s_w     = w;
    s_h     = h;
    s_pitch = pitch ? pitch : (w * 4);

    serial_puts("[fb_init] addr="); serial_hex64((uint64_t)(uintptr_t)addr);
    serial_puts("[fb_init] w="); serial_putu((uint64_t)s_w);
    serial_puts("[fb_init] h="); serial_putu((uint64_t)s_h);
    serial_puts("[fb_init] pitch="); serial_putu((uint64_t)s_pitch);

    // Clear to black using pitch-aware copy
    if(s_fb && w && h){
        uint8_t *base = (uint8_t*)s_fb;
        for(uint32_t y = 0; y < h; ++y){
            uint32_t *row = (uint32_t*)(base + (uint64_t)y * s_pitch);
            for(uint32_t x = 0; x < w; ++x) {
                row[x] = 0xFF000000u; // Opaque Black
            }
        }
    }
}

uint32_t  fb_get_width(void)  { return s_w; }
uint32_t  fb_get_height(void) { return s_h; }
uint32_t  fb_get_pitch(void)  { return s_pitch; }
uint32_t *fb_get_addr(void)   { return s_fb; }

void fb_put(uint32_t x, uint32_t y, uint32_t color)
{
    if(s_fb && x < s_w && y < s_h) {
        uint8_t *row_bytes = (uint8_t*)s_fb + ((uint64_t)y * s_pitch);
        ((uint32_t*)row_bytes)[x] = color;
    }
}
