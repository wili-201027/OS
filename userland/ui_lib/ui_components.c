// userland/ui_lib/ui_components.c
// Implementación de componentes UI

#include "ui_components.h"
#include "../libc/stdlib.h"
#include "../libc/string.h"

extern void draw_string_fb(uint32_t*, uint32_t, uint32_t, int, int, const char*, uint32_t);

/* scheduler_get_ticks is provided by the kernel (exposed to userland via libc/syscalls) */
extern uint64_t scheduler_get_ticks(void);

// ─── BOTONES ────────────────────────────────────────────────────────────────

Button* button_create(int x, int y, int w, int h, const char *text)
{
    Button *btn = (Button*)malloc(sizeof(Button));
    if(!btn) return 0;
    
    btn->x = x;
    btn->y = y;
    btn->w = w;
    btn->h = h;
    btn->text = text;
    btn->is_hovered = 0;
    btn->is_pressed = 0;
    btn->on_click = 0;
    btn->caller = 0;
    
    return btn;
}

void button_draw(Button *btn, uint32_t *fb, uint32_t fb_w, int win_y)
{
    if(!btn || !fb) return;
    
    uint32_t bg_color = btn->is_pressed ? COLOR_ACCENT :
                        btn->is_hovered ? COLOR_ACCENT_LIGHT : COLOR_BG_LIGHT;
    
    // Dibujar fondo del botón
    for(int row = btn->y + win_y; row < btn->y + btn->h + win_y; row++) {
        for(int col = btn->x; col < btn->x + btn->w; col++) {
            fb[row * fb_w + col] = bg_color;
        }
    }
    
    // Dibujar borde
    for(int row = btn->y + win_y; row < btn->y + btn->h + win_y; row++) {
        fb[row * fb_w + btn->x] = COLOR_BORDER;
        fb[row * fb_w + (btn->x + btn->w - 1)] = COLOR_BORDER;
    }
    for(int col = btn->x; col < btn->x + btn->w; col++) {
        fb[(btn->y + win_y) * fb_w + col] = COLOR_BORDER;
        fb[(btn->y + btn->h - 1 + win_y) * fb_w + col] = COLOR_BORDER;
    }
    
    // Dibujar texto
    draw_string_fb(fb, fb_w, 2000, btn->x + 5, btn->y + win_y + 4, btn->text, COLOR_TEXT_PRIMARY);
}

int button_hit_test(Button *btn, int x, int y)
{
    if(!btn) return 0;
    return (x >= btn->x && x < btn->x + btn->w &&
            y >= btn->y && y < btn->y + btn->h);
}

void button_destroy(Button *btn)
{
    if(btn) free(btn);
}

// ─── CAMPOS DE TEXTO ────────────────────────────────────────────────────────

TextField* textfield_create(int x, int y, int w, int h, int max_len)
{
    TextField *tf = (TextField*)malloc(sizeof(TextField));
    if(!tf) return 0;
    
    tf->x = x;
    tf->y = y;
    tf->w = w;
    tf->h = h;
    tf->max_len = max_len;
    tf->is_focused = 0;
    tf->cursor_pos = 0;
    
    tf->text = (char*)malloc(max_len + 1);
    if(!tf->text) {
        free(tf);
        return 0;
    }
    tf->text[0] = 0;
    
    return tf;
}

void textfield_draw(TextField *tf, uint32_t *fb, uint32_t fb_w, int win_y)
{
    if(!tf || !fb) return;
    
    // Fondo blanco con borde azul si está enfocado
    uint32_t border = tf->is_focused ? COLOR_ACCENT : COLOR_BORDER;
    
    for(int row = tf->y + win_y; row < tf->y + tf->h + win_y; row++) {
        for(int col = tf->x; col < tf->x + tf->w; col++) {
            fb[row * fb_w + col] = 0xFFFFFFFF;
        }
    }
    
    // Dibujar borde
    for(int row = tf->y + win_y; row < tf->y + tf->h + win_y; row++) {
        fb[row * fb_w + tf->x] = border;
        fb[row * fb_w + (tf->x + tf->w - 1)] = border;
    }
    for(int col = tf->x; col < tf->x + tf->w; col++) {
        fb[(tf->y + win_y) * fb_w + col] = border;
        fb[(tf->y + tf->h - 1 + win_y) * fb_w + col] = border;
    }
    
    // Dibujar texto
    draw_string_fb(fb, fb_w, 2000, tf->x + 3, tf->y + win_y + 3, tf->text, COLOR_TEXT_PRIMARY);
    
    // Dibujar cursor si está enfocado
    if(tf->is_focused && (scheduler_get_ticks() % 20) < 10) {
        int cursor_x = tf->x + 3 + (tf->cursor_pos * 7);
        for(int row = tf->y + win_y + 2; row < tf->y + tf->h + win_y - 2; row++) {
            if(cursor_x >= tf->x && cursor_x < tf->x + tf->w) {
                fb[row * fb_w + cursor_x] = COLOR_ACCENT;
            }
        }
    }
}

void textfield_insert_char(TextField *tf, char c)
{
    if(!tf || tf->cursor_pos >= tf->max_len) return;
    
    // Insertar carácter en posición del cursor
    for(int i = strlen(tf->text); i > tf->cursor_pos; i--) {
        tf->text[i] = tf->text[i-1];
    }
    tf->text[tf->cursor_pos] = c;
    tf->text[strlen(tf->text) + 1] = 0;
    tf->cursor_pos++;
}

void textfield_backspace(TextField *tf)
{
    if(!tf || tf->cursor_pos == 0) return;
    
    for(int i = tf->cursor_pos - 1; i < (int)strlen(tf->text); i++) {
        tf->text[i] = tf->text[i+1];
    }
    tf->cursor_pos--;
}

void textfield_destroy(TextField *tf)
{
    if(!tf) return;
    if(tf->text) free(tf->text);
    free(tf);
}

// ─── SCROLLBARS ─────────────────────────────────────────────────────────────

Scrollbar* scrollbar_create(int x, int y, int w, int h, int total, int visible, int is_vertical)
{
    Scrollbar *sb = (Scrollbar*)malloc(sizeof(Scrollbar));
    if(!sb) return 0;
    
    sb->x = x;
    sb->y = y;
    sb->w = w;
    sb->h = h;
    sb->total_size = total;
    sb->visible_size = visible;
    sb->scroll_pos = 0;
    sb->is_vertical = is_vertical;
    
    return sb;
}

void scrollbar_draw(Scrollbar *sb, uint32_t *fb, uint32_t fb_w, uint32_t fb_h)
{
    if(!sb || !fb) return;
    
    // Dibujar fondo
    if(sb->is_vertical) {
        for(int row = sb->y; row < sb->y + sb->h && row < (int)fb_h; row++) {
            for(int col = sb->x; col < sb->x + sb->w; col++) {
                fb[row * fb_w + col] = COLOR_BG_LIGHT;
            }
        }
    }
    
    // Calcular tamaño y posición del thumb
    int thumb_size = (sb->visible_size * sb->h) / sb->total_size;
    int thumb_pos = (sb->scroll_pos * sb->h) / sb->total_size;
    
    // Dibujar thumb (la parte que se arrastra)
    if(sb->is_vertical) {
        for(int row = sb->y + thumb_pos; row < sb->y + thumb_pos + thumb_size && row < sb->y + sb->h; row++) {
            for(int col = sb->x; col < sb->x + sb->w; col++) {
                fb[row * fb_w + col] = COLOR_ACCENT;
            }
        }
    }
}

void scrollbar_scroll(Scrollbar *sb, int delta)
{
    if(!sb) return;
    scrollbar_set_position(sb, sb->scroll_pos + delta);
}

void scrollbar_set_position(Scrollbar *sb, int pos)
{
    if(!sb) return;
    sb->scroll_pos = pos;
    if(sb->scroll_pos < 0) sb->scroll_pos = 0;
    if(sb->scroll_pos + (int)sb->visible_size > (int)sb->total_size) {
        sb->scroll_pos = sb->total_size - sb->visible_size;
    }
}

void scrollbar_destroy(Scrollbar *sb)
{
    if(sb) free(sb);
}

// ─── STUBS para las demás funciones ──────────────────────────────────────────

ComboBox* combobox_create(int x, int y, int w, int h, char **options, int count)
{
    ComboBox *cb = (ComboBox*)malloc(sizeof(ComboBox));
    if(!cb) return 0;
    
    cb->x = x; cb->y = y; cb->w = w; cb->h = h;
    cb->options = options;
    cb->option_count = count;
    cb->selected_idx = 0;
    cb->is_open = 0;
    
    return cb;
}

void combobox_draw(ComboBox *cb, uint32_t *fb, uint32_t fb_w, int win_y) { }
void combobox_toggle(ComboBox *cb) { if(cb) cb->is_open = !cb->is_open; }
void combobox_destroy(ComboBox *cb) { if(cb) free(cb); }

CheckBox* checkbox_create(int x, int y, int w, int h, const char *label)
{
    CheckBox *chk = (CheckBox*)malloc(sizeof(CheckBox));
    if(!chk) return 0;
    
    chk->x = x; chk->y = y; chk->w = w; chk->h = h;
    chk->label = label;
    chk->is_checked = 0;
    
    return chk;
}

void checkbox_draw(CheckBox *cb, uint32_t *fb, uint32_t fb_w, int win_y) { }
void checkbox_toggle(CheckBox *cb) { if(cb) cb->is_checked = !cb->is_checked; }
void checkbox_destroy(CheckBox *cb) { if(cb) free(cb); }

extern uint64_t scheduler_get_ticks(void);
