#include <stdint.h>
#include <stddef.h>

extern void *wm_create_window(int x, int y, int w, int h, const char *title);
extern void wm_clear_window(void *win, uint32_t color);
extern void wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color);
extern void wm_write(void *win, int x, int y, const char *text, uint32_t color);
extern void wm_start_spawn_animation(void *win, int target_w, int target_h);
extern void compositor_mark_dirty(void);
extern void sysroot_prepare(void);
extern int sysroot_lookup_path(const char *path, char *out, uint32_t out_size);

void wm_start_spawn_animation(void *win, int target_w, int target_h)
{
    (void)win;
    (void)target_w;
    (void)target_h;
}

void compositor_mark_dirty(void)
{
}

static void *s_shell_window = 0;

void graphics_server_start(void)
{
    sysroot_prepare();

    s_shell_window = wm_create_window(30, 40, 620, 380, "Graphical Shell");
    if (s_shell_window) {
        wm_clear_window(s_shell_window, 0xFF07131F);
        wm_fill_rect(s_shell_window, 0, 0, 620, 24, 0xFF2E6BFF);
        wm_write(s_shell_window, 14, 8, "GPT-OS Graphical Shell", 0xFFFFFFFF);
        wm_write(s_shell_window, 14, 44, "Userland apps request windows from the compositor.", 0xFF8DC6FF);

        char resolved[64] = {0};
        if (sysroot_lookup_path("/bin", resolved, sizeof(resolved)) == 0) {
            wm_write(s_shell_window, 14, 66, resolved, 0xFF70C6FF);
        } else {
            wm_write(s_shell_window, 14, 66, "/bin -> /usr/bin", 0xFF70C6FF);
        }

        wm_write(s_shell_window, 14, 92, "Ring3 apps must stay off the raw framebuffer.", 0xFF5CFFB2);
        wm_fill_rect(s_shell_window, 14, 120, 180, 24, 0xFF10263A);
        wm_write(s_shell_window, 24, 126, "sysroot: ready", 0xFF70C6FF);
        wm_start_spawn_animation(s_shell_window, 620, 380);
        compositor_mark_dirty();
    }
}
