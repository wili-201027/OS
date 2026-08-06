// userland/ui_lib/ui_components.h
// Librería de componentes UI reutilizables (Botones, Inputs, Scrollbars, etc.)

#ifndef UI_COMPONENTS_H
#define UI_COMPONENTS_H

#include <stdint.h>

// ─── Colores del tema Windows Moderno ──────────────────────────────────────
#define COLOR_ACCENT        0xFF0078D4   // Azul
#define COLOR_ACCENT_LIGHT  0xFF0063B1
#define COLOR_BG_LIGHT      0xFFF5F5F5   // Gris muy claro
#define COLOR_BG_MEDIUM     0xFFEFEFEF   // Gris medio
#define COLOR_BG_DARK       0xFF404040   // Gris oscuro
#define COLOR_TEXT_PRIMARY  0xFF000000   // Negro
#define COLOR_TEXT_LIGHT    0xFFFFFFFF   // Blanco
#define COLOR_BORDER        0xFFD0D0D0   // Gris borde

// ─── Esctructuras de componentes ──────────────────────────────────────────

// BOTÓN
typedef struct {
    int       x, y, w, h;
    const char *text;
    int       is_hovered;
    int       is_pressed;
    uint32_t  (*on_click)(void *caller);
    void     *caller;
} Button;

// CAMPO DE TEXTO
typedef struct {
    int       x, y, w, h;
    char     *text;
    int       max_len;
    int       is_focused;
    int       cursor_pos;
} TextField;

// SCROLLBAR
typedef struct {
    int       x, y, w, h;
    int       total_size;
    int       visible_size;
    int       scroll_pos;
    int       is_vertical;
} Scrollbar;

// COMBOBOX (Desplegable)
typedef struct {
    int       x, y, w, h;
    char    **options;
    int       option_count;
    int       selected_idx;
    int       is_open;
} ComboBox;

// CHECKBOX
typedef struct {
    int       x, y, w, h;
    const char *label;
    int       is_checked;
} CheckBox;

// ─── Funciones de componentes ───────────────────────────────────────────────

// Botones
Button* button_create(int x, int y, int w, int h, const char *text);
void    button_draw(Button *btn, uint32_t *fb, uint32_t fb_w, int win_y);
int     button_hit_test(Button *btn, int x, int y);
void    button_destroy(Button *btn);

// Campos de texto
TextField* textfield_create(int x, int y, int w, int h, int max_len);
void       textfield_draw(TextField *tf, uint32_t *fb, uint32_t fb_w, int win_y);
void       textfield_insert_char(TextField *tf, char c);
void       textfield_backspace(TextField *tf);
void       textfield_destroy(TextField *tf);

// Scrollbars
Scrollbar* scrollbar_create(int x, int y, int w, int h, int total, int visible, int is_vertical);
void       scrollbar_draw(Scrollbar *sb, uint32_t *fb, uint32_t fb_w, uint32_t fb_h);
void       scrollbar_scroll(Scrollbar *sb, int delta);
void       scrollbar_set_position(Scrollbar *sb, int pos);
void       scrollbar_destroy(Scrollbar *sb);

// ComboBox
ComboBox* combobox_create(int x, int y, int w, int h, char **options, int count);
void      combobox_draw(ComboBox *cb, uint32_t *fb, uint32_t fb_w, int win_y);
void      combobox_toggle(ComboBox *cb);
void      combobox_destroy(ComboBox *cb);

// CheckBox
CheckBox* checkbox_create(int x, int y, int w, int h, const char *label);
void      checkbox_draw(CheckBox *cb, uint32_t *fb, uint32_t fb_w, int win_y);
void      checkbox_toggle(CheckBox *cb);
void      checkbox_destroy(CheckBox *cb);

#endif // UI_COMPONENTS_H
