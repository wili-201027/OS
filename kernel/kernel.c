// kernel/kernel.cpp
// El compositor glassmorphism corre en ring-0.
// Tras inicializar todos los subsistemas, kernel_main llama compositor_start()
// que contiene el frame loop y nunca retorna.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern uint8_t initrd_start[];
extern uint8_t initrd_end[];

void kernel_main(void* mb2_info, uint32_t mb2_magic);
void gdt_init(void);
void tss_init(void);
void idt_init(void);
void traps_init(void);
void pic_remap(void);
void cpu_local_init(unsigned int);
void pmm_init(unsigned long, unsigned long);
void vmm_init(void);
void heap_init(void);
void acpi_init(void*);
void pci_scan(void);
void ps2_init(void);
void fb_init(void*, unsigned int, unsigned int, unsigned int);
void ahci_init(unsigned long);
void scheduler_init(void);
void ramfs_init(void);
void fs_init(void);
void net_init(void);
void storage_init(void);
uint32_t storage_get_capabilities(void);
uint64_t scheduler_get_ticks(void);
/* Compositor (userland/compositor/compositor.c compilado en el kernel) */
void compositor_start(void);

// ── Serial ────────────────────────────────────────────────────────────────────
static inline void outb(uint16_t p, uint8_t v) { asm volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; asm volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static void serial_init() {
    outb(0x3F9,0x00); outb(0x3FB,0x80);
    outb(0x3F8,0x01); outb(0x3F9,0x00);
    outb(0x3FB,0x03); outb(0x3FA,0xC7); outb(0x3FC,0x0B);
}
static void serial_putc(char c) { while(!(inb(0x3FD)&0x20)); outb(0x3F8,(uint8_t)c); }
static void serial_puts(const char* s) {
    for(;s&&*s;++s){ if(*s=='\n') serial_putc('\r'); serial_putc(*s); }
}

static void serial_puthex(uint64_t v){ static const char h[]="0123456789ABCDEF"; serial_puts("0x"); for(int i=15;i>=0;--i) serial_putc(h[(v>>(i*4))&0xF]); }
static void serial_putu(uint64_t v){ if(!v){ serial_putc('0'); return; } char b[32]; int n=0; while(v){ b[n++] = '0' + (int)(v%10); v/=10; } for(int i=n-1;i>=0;--i) serial_putc(b[i]); }

// ── VGA text fallback (solo para diagnóstico pre-FB) ─────────────────────────
static volatile uint16_t* const vga_B = (volatile uint16_t*)0xB8000;
static int vga_col=0, vga_row=0;
static void vga_set(int r,int c,char ch,uint8_t a){
    if(r>=0&&r<25&&c>=0&&c<80) vga_B[r*80+c]=(uint16_t)((a<<8)|(uint8_t)ch);
}
static void vga_cls(uint8_t a){ for(int i=0;i<80*25;++i) vga_B[i]=(uint16_t)((a<<8)|' '); vga_col=vga_row=0; }
static void vga_scroll(){ for(int r=0;r<24;++r) for(int c=0;c<80;++c) vga_B[r*80+c]=vga_B[(r+1)*80+c]; for(int c=0;c<80;++c) vga_B[24*80+c]=0x0720; vga_row=24; }
static void vga_putc(char ch){ if(ch=='\n'){vga_col=0;if(++vga_row>=25)vga_scroll();return;} vga_set(vga_row,vga_col,ch,0x0F); if(++vga_col>=80){vga_col=0;if(++vga_row>=25)vga_scroll();} }
static void vga_puts(const char*s){while(s&&*s)vga_putc(*s++);}
static void vga_puthex(uint64_t v){ static const char h[]="0123456789ABCDEF"; vga_puts("0x"); for(int i=15;i>=0;--i) vga_putc(h[(v>>(i*4))&0xF]); }
static void vga_putu(uint64_t v){ if(!v){vga_putc('0');return;} char b[20];int n=0; while(v){b[n++]='0'+(int)(v%10);v/=10;} for(int i=n-1;i>=0;--i)vga_putc(b[i]); }

// ── Multiboot2 ────────────────────────────────────────────────────────────────
typedef struct Mb2Tag    { uint32_t type,size; } Mb2Tag;
typedef struct Mb2TagFb  { uint32_t type,size; uint64_t addr; uint32_t pitch,width,height; uint8_t bpp,fb_type; uint16_t res; } Mb2TagFb;
typedef struct Mb2TagMem { uint32_t type,size; uint32_t lower,upper; } Mb2TagMem;
typedef struct Mb2TagMod { uint32_t type,size; uint32_t start,end; char str[1]; } Mb2TagMod;

static void*     g_fb_addr = NULL;
static uint32_t  g_fb_w=0, g_fb_h=0, g_fb_bpp=0, g_fb_pitch=0;
static uint32_t  g_mem_kb=0;
static uint8_t  *g_initrd_start = NULL, *g_initrd_end = NULL;

static void parse_mb2(void* info) {
    if(!info) return;
    uint8_t* p = (uint8_t*)info + 8;
    while(true) {
        Mb2Tag* t = (Mb2Tag*)p;
        if(t->type==0) break;
        if(t->type==8) {
            Mb2TagFb *fb = (Mb2TagFb*)t;
            serial_puts("[MB2.FB] tag_size="); serial_putu(t->size);
            serial_puts(" addr="); serial_puthex(fb->addr);
            serial_puts(" pitch="); serial_putu(fb->pitch);
            serial_puts(" width="); serial_putu(fb->width);
            serial_puts(" height="); serial_putu(fb->height);
            serial_puts(" bpp="); serial_putu(fb->bpp);
            serial_puts("\n");
            g_fb_addr=(void*)(uintptr_t)fb->addr;
            g_fb_w=fb->width; g_fb_h=fb->height;
            g_fb_bpp=fb->bpp; g_fb_pitch=fb->pitch;
        }
        if(t->type==4) { Mb2TagMem *m = (Mb2TagMem*)t; g_mem_kb=m->upper; }
        if(t->type==3) {
            Mb2TagMod *m = (Mb2TagMod*)t;
            g_initrd_start=(uint8_t*)(uintptr_t)m->start;
            g_initrd_end  =(uint8_t*)(uintptr_t)m->end;
        }
        p += (t->size+7u)&~7u;
    }
}

// ── kernel_main ───────────────────────────────────────────────────────────────
void kernel_main(void* mb2_info, uint32_t mb2_magic)
{
    asm volatile("cli");
    serial_init();
    serial_puts("[BOOT] kernel_main\n");

    // 1. SSE/x87
    { uint64_t c0,c4;
      asm volatile("mov %%cr0,%0":"=r"(c0));
      c0=(c0&~(uint64_t)(1<<2)&~(uint64_t)(1<<3))|(1<<1);
      asm volatile("mov %0,%%cr0"::"r"(c0));
      asm volatile("mov %%cr4,%0":"=r"(c4));
      c4|=(uint64_t)(3<<9);
      asm volatile("mov %0,%%cr4"::"r"(c4)); }

    // 2. GDT/TSS/IDT/PIC
    gdt_init();   serial_puts("[+] GDT\n");
    tss_init();   serial_puts("[+] TSS\n");
    pic_remap();  serial_puts("[+] PIC\n");
    idt_init();   serial_puts("[+] IDT\n");
    traps_init(); serial_puts("[+] Traps\n");

    // 3. Parsear MB2
    if(mb2_magic == 0x36d76289) parse_mb2(mb2_info);
    serial_puts("[+] MB2 parsed\n");

    // 4. Diagnóstico VGA text (antes de tener FB lineal)
    vga_cls(0x01); // fondo azul oscuro
    vga_puts("GPT-OS booting");
    vga_puts(g_fb_addr ? "  [Linear FB detected]" : "  [VGA text mode]");

    // 5. Memoria — empezar en 4MB para dejar espacio al kernel + MB2 + FB tables
    pmm_init(0x400000, 0x10000000);  // 4 MB – 256 MB
    vmm_init();
    serial_puts("[+] Memory\n");

    // 6. CPU / plataforma
    cpu_local_init(0);
    acpi_init((void*)0x000E0000);
    pci_scan();

    // 7. Drivers
    ps2_init();
    ahci_init(0xFE000000);
    storage_init();
    serial_puts("[+] Drivers\n");

    // 8. Subsistemas
    scheduler_init();
    ramfs_init();
    fs_init();
    net_init();
    heap_init();
    serial_puts("[+] Subsystems\n");

    // 9. Inicializar framebuffer
    if(g_fb_addr && g_fb_w > 0 && g_fb_bpp == 32) {
        serial_puts("[FB] Linear "); serial_puts("\n");
        fb_init(g_fb_addr, g_fb_w, g_fb_h, g_fb_pitch);
    } else {
        // Fallback: QEMU VGA std sin VESA → framebuffer en 0xE0000000 (PCI BAR)
        // Intentar dirección estándar del VGA PCI
        fb_init((void*)0xE0000000, 1024, 768, 1024*4);
        serial_puts("[FB] Fallback 0xE0000000\n");
    }

    serial_puts("[+] Starting compositor\n");
    serial_puts("[SYSROOT] /bin /dev /etc /lib /usr prepared\n");
    asm volatile("sti");

    // 10. Lanzar compositor (frame loop, no retorna)
    compositor_start();

    // Si el compositor retorna (no debería), idle loop
    for(;;) asm volatile("hlt");
}
