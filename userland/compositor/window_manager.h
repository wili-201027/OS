#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void *wm_create_window(int x, int y, int w, int h, const char *title);
void  wm_destroy_window(void *win);
int   wm_window_get_id(void *win);
void* wm_get_window_by_id(int wid);

void  wm_clear_window(void *win, uint32_t color);
void  wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color);
void  wm_write(void *win, int x, int y, const char *text, uint32_t color);

void  wm_register_taskbar(void *win);
void  wm_toggle_menu(void);
void  wm_cycle_focus(int direction);
void  wm_move_focused_window(int dx,int dy);

void  wm_handle_mouse(int ax,int ay,bool btn,bool pressed,bool released);
void  wm_handle_key(uint8_t sc,bool pressed);
void  wm_draw_cursor(uint32_t*fb,uint32_t fbw,uint32_t fbh);
void  wm_get_cursor_pos(int *x, int *y);

// Theme API: select predefined themes by id (0=dark/default,1=glass,2=mint)
void  wm_set_theme(int theme_id);
void  wm_set_window_style(void *win, uint32_t flags);
// Animation control
void  wm_start_spawn_animation(void *win, int target_w, int target_h);
void  wm_start_close_animation(void *win, int collapse_ms);
void  wm_tick_animations(uint64_t now_ticks);

// Minimal DOM-like element API for window structure
typedef struct wm_element wm_element;
wm_element* wm_create_element(const char *tag, const char *id);
void wm_append_child(wm_element *parent, wm_element *child);
void wm_set_element_style(wm_element *el, uint32_t style_flags);
wm_element* wm_find_element_by_id(void *win, const char *id);

#ifdef __cplusplus
}
#endif

#endif // WINDOW_MANAGER_H

// Per-window style flags (same as internal)
#define W_STYLE_GLASS    0x1
#define W_STYLE_NO_DECOR 0x2
