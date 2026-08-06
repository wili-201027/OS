// userland/compositor/compositor.cpp
// Compositor glassmorphism corriendo en ring-0.
// Frame loop: input → animate → render → repeat.

#include <stdint.h>
#include <stddef.h>
#include "window_manager.h"

extern "C" {
    uint32_t  fb_get_width(void);
    uint32_t  fb_get_height(void);
    uint32_t *fb_get_addr(void);
    void      sys_sleep_ms(uint32_t ms);
    uint64_t  scheduler_get_ticks(void);

    // PS/2 keyboard raw read (no espera, retorna 0 si vacío)
    // Implementado en kernel/drivers/ps2.c
    uint8_t   ps2_read_scancode_nowait(void);
}

// VGA text debug output at row
static void vga_write_row(int row, const char *msg) {
    volatile uint16_t *vga = (volatile uint16_t*)0xB8000;
    volatile uint16_t *line = vga + (row * 80);
    
    // Clear line first
    for(int i = 0; i < 80; ++i) {
        line[i] = 0x0F00 | (uint8_t)' ';
    }
    
    // Write message
    for(int i = 0; msg[i] && i < 80; ++i) {
        line[i] = 0x0F00 | (uint8_t)msg[i];
    }
}

extern "C" void render_frame(void);
extern "C" void input_router_poll(void);
extern "C" void wm_register_taskbar(void *win);
extern "C" void scene_graph_tick(void);
extern "C" void wm_draw_cursor(uint32_t*, uint32_t, uint32_t);
extern "C" void *wm_create_window(int x, int y, int w, int h, const char *title);
extern "C" void wm_clear_window(void *win, uint32_t color);
extern "C" void wm_write(void *win, int x, int y, const char *text, uint32_t color);
extern "C" void wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color);
extern "C" void wm_set_window_style(void *win, uint32_t flags);
extern "C" void wm_start_spawn_animation(void *win, int target_w, int target_h);
extern "C" void draw_string_fb(uint32_t*, uint32_t, uint32_t, int, int, const char*, uint32_t);
extern "C" void draw_string_fb_scaled(uint32_t*, uint32_t, uint32_t, int, int, const char*, uint32_t, int);
extern "C" uint32_t *get_back_buffer(uint32_t w, uint32_t h);
extern "C" void flip_frame(uint32_t w, uint32_t h);
extern "C" void render_wallpaper(uint32_t *fb, uint32_t w, uint32_t h);
extern "C" void compositor_mark_dirty(void);

static bool s_dirty_flag = true;

// ── Tick counter display en esquina superior derecha ─────────────────────────
static void draw_clock(uint32_t *fb, uint32_t w)
{
    uint64_t ticks = scheduler_get_ticks();
    uint64_t secs  = ticks / 18;   // ~18 ticks/s con PIT a 18.2Hz

    // HH:MM:SS (módulo)
    uint32_t ss = (uint32_t)(secs % 60);
    uint32_t mm = (uint32_t)((secs / 60) % 60);
    uint32_t hh = (uint32_t)((secs / 3600) % 24);

    char buf[9];
    buf[0] = '0' + hh/10; buf[1] = '0' + hh%10; buf[2] = ':';
    buf[3] = '0' + mm/10; buf[4] = '0' + mm%10; buf[5] = ':';
    buf[6] = '0' + ss/10; buf[7] = '0' + ss%10; buf[8] = 0;

    // Fondo negro semitransparente detrás del reloj (fila 4, col W-76)
    int cx = (int)w - 76;
    int cy = 4;
    for(int row=cy-2; row<cy+8; ++row)
        for(int col=cx-4; col<(int)w-4; ++col)
            if(row>=0 && (uint32_t)row<fb_get_height() && col>=0 && (uint32_t)col<w)
                fb[row*w+col] = 0xCC000000 | (fb[row*w+col] & 0x3F3F3F);

    // Dibujar texto (font 5px alta, hardcoded)
    // Usamos el mismo font3x5 del window_manager
    draw_string_fb_scaled(fb, w, fb_get_height(), cx, cy, buf, 0xFFFFFFFF, 2);
}

// ── Spawn ventanas iniciales ──────────────────────────────────────────────────
static bool s_spawned = false;

static void spawn_initial_windows(void)
{
    if(s_spawned) return;
    s_spawned = true;

    uint32_t W = fb_get_width();
    uint32_t H = fb_get_height();
    if(W == 0 || H == 0) return;

    // Terminal window
    void *term = wm_create_window(40, 40, 540, 360, "TERMINAL");
    if(term) {
        wm_clear_window(term, 0xFF07131F);
        wm_fill_rect(term, 10, 10, 520, 1, 0xFF3D6DFF);
        wm_write(term, 15, 20, "GPT-OS TERMINAL", 0xFFEAF6FF);
        wm_write(term, 15, 40, "Type text to enter commands", 0xFF8DC6FF);
        wm_write(term, 15, 60, "Press ALT+F4 to close window", 0xFF5CFFB2);
        wm_write(term, 15, 80, "", 0xFF3D6DFF);
        wm_write(term, 15, 95, "boot log:", 0xFFFFD76A);
        wm_write(term, 20, 115, "[KERNEL] initialized", 0xFF5CFFB2);
        wm_write(term, 20, 135, "[DRIVERS] PS/2, AHCI ready", 0xFF5CFFB2);
        wm_write(term, 20, 155, "[COMPOSITOR] started", 0xFF5CFFB2);
        wm_write(term, 20, 175, "[IPC] ports allocated", 0xFF5CFFB2);
        wm_write(term, 20, 195, "ready >", 0xFF70C6FF);
        wm_set_window_style(term, W_STYLE_GLASS);
        wm_start_spawn_animation(term, 540, 360);
    }
    
    // System Monitor window
    void *sysmon = wm_create_window((int)W-420, 40, 380, 280, "SYSTEM MONITOR");
    if(sysmon) {
        wm_clear_window(sysmon, 0xFF091422);
        wm_fill_rect(sysmon, 10, 10, 360, 1, 0xFF4B86FF);
        wm_write(sysmon, 15, 20, "SYSTEM STATUS", 0xFFEAF6FF);
        wm_write(sysmon, 15, 45, "CPU: 0% | RAM: 256 MB free", 0xFF8DC6FF);
        wm_write(sysmon, 15, 65, "UPTIME: 00:00:45", 0xFF8DC6FF);
        wm_write(sysmon, 15, 85, "PROCESSES: 4 running", 0xFF8DC6FF);
        wm_write(sysmon, 15, 105, "", 0xFF4B86FF);
        wm_write(sysmon, 15, 120, "ACTIVE SERVERS:", 0xFFFFD76A);
        wm_write(sysmon, 20, 140, "- Device Manager", 0xFF5CFFB2);
        wm_write(sysmon, 20, 160, "- Filesystem", 0xFF5CFFB2);
        wm_write(sysmon, 20, 180, "- GPU Driver", 0xFF5CFFB2);
        wm_write(sysmon, 20, 200, "- Quantum Simulator", 0xFF5CFFB2);
        wm_write(sysmon, 15, 225, "All systems normal", 0xFF70C6FF);
        wm_set_window_style(sysmon, W_STYLE_GLASS);
        wm_start_spawn_animation(sysmon, 380, 280);
    }
    
    // Taskbar window
    void *taskbar = wm_create_window(0, (int)H-46, (int)W, 44, nullptr);
    if(taskbar) {
        wm_clear_window(taskbar, 0xFF030812);
        wm_fill_rect(taskbar, 0, 0, (int)W, 1, 0xFF3D6DFF);
        wm_fill_rect(taskbar, 10, 8, 78, 24, 0xFF10263A);
        wm_fill_rect(taskbar, 100, 8, 78, 24, 0xFF10263A);
        wm_fill_rect(taskbar, 190, 8, 78, 24, 0xFF10263A);
        wm_write(taskbar, 24, 14, "TERMINAL", 0xFF70C6FF);
        wm_write(taskbar, 114, 14, "SYSTEM", 0xFF70C6FF);
        wm_write(taskbar, 204, 14, "FILES", 0xFF70C6FF);
        wm_write(taskbar, (int)W-80, 10, "60 FPS", 0xFF5CFFB2);
        wm_register_taskbar(taskbar);
        // taskbar should be immediate (no spawn animation)
    }
}

// ── compositor_start ─────────────────────────────────────────────────────────
extern "C"
void compositor_start(void)
{
    vga_write_row(0, "[COMP] Starting compositor_start");
    
    uint32_t W = fb_get_width();
    uint32_t H = fb_get_height();
    uint32_t *fb = fb_get_addr();

    vga_write_row(1, "[COMP] Got FB dimensions");

    // Si no hay FB lineal válido, parpadear en VGA texto como fallback
    if(!fb || W == 0 || H == 0) {
        vga_write_row(2, "[COMP] ERROR: Invalid framebuffer");
        volatile uint16_t *vga = (volatile uint16_t*)0xB8000;
        uint64_t t = 0;
        for(;;) {
            uint8_t attr = (uint8_t)(0x1F + ((t >> 3) & 1));
            const char *msg = "  GPT-OS  |  No linear framebuffer  |  "
                              "Press Ctrl+C to exit QEMU  ";
            for(int i = 0; msg[i]; ++i)
                vga[i] = (uint16_t)((attr << 8) | (uint8_t)msg[i]);
            for(volatile int d=0;d<4000000;d++) asm volatile("pause");
            ++t;
        }
    }

    vga_write_row(2, "[COMP] Allocating back buffer");

    // Initialize back buffer before anything else
    uint32_t *back_fb = get_back_buffer(W, H);
    if (!back_fb) {
        // Fallback if malloc fails
        vga_write_row(3, "[COMP] ERROR: Back buffer allocation failed");
        volatile uint16_t *vga = (volatile uint16_t*)0xB8000;
        for(int i=0;i<80;++i) vga[i] = (0x4F<<8) | (uint8_t)'M';
        for(;;) asm volatile("hlt");
    }

    vga_write_row(3, "[COMP] Testing direct framebuffer write");

    // TEST: Write directly to visible framebuffer to verify it's accessible
    for(uint32_t i = 0; i < W*H; ++i) {
        fb[i] = 0xFF0040FF;  // Bright magenta pixels
    }
    
    vga_write_row(4, "[COMP] Direct write done, sleeping");
    sys_sleep_ms(500);
    
    vga_write_row(5, "[COMP] Drawing wallpaper to back buffer");

    // Draw initial wallpaper
    render_wallpaper(back_fb, W, H);
    
    vga_write_row(6, "[COMP] Flipping frame");
    flip_frame(W, H);

    vga_write_row(7, "[COMP] Spawning initial windows");
    spawn_initial_windows();

    vga_write_row(8, "[COMP] Entering main loop");

    if (s_dirty_flag) {
        render_frame();
        s_dirty_flag = false;
    }

    for(;;) {
        // 1. Input
        input_router_poll();

        // 2. Animaciones
        scene_graph_tick();

        // 3. Render only when dirty to avoid expensive full redraws
        if (s_dirty_flag) {
            render_frame();
            s_dirty_flag = false;
        }

        // 4. Atomic flip: copy back buffer to visible framebuffer
        flip_frame(W, H);

        // 5. Draw overlay elements directly on the visible framebuffer
        uint32_t *visible_fb = fb_get_addr();
        if (visible_fb) {
            draw_clock(visible_fb, W);
            wm_draw_cursor(visible_fb, W, H);
        }

        // 6. Pace (~60 fps, ~1ms per frame)
        sys_sleep_ms(1);
    }
}

extern "C"
void compositor_mark_dirty(void) {
    s_dirty_flag = true;
}
