// userland/programs/text_editor.c
// Editor de texto simple para archivos .txt, .c, .h, .py, etc.

#include <stdint.h>
#include <stddef.h>
#include "../libc/string.h"
#include "../libc/stdlib.h"
#include "../libc/stdio.h"

extern uint32_t  fb_get_width(void);
extern uint32_t  fb_get_height(void);
extern uint32_t *fb_get_addr(void);
extern void      sys_sleep_ms(uint32_t ms);

extern void *wm_create_window(int x, int y, int w, int h, const char *title);
extern void  wm_clear_window(void *win, uint32_t color);
extern void  wm_write(void *win, int x, int y, const char *text, uint32_t color);
extern void  wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color);
extern void  draw_string_fb(uint32_t*, uint32_t, uint32_t, int, int, const char*, uint32_t);

// ─── Estructura de editortext ──────────────────────────────────────────────
typedef struct {
    char    filename[256];
    char   *content;
    int     content_size;
    int     content_capacity;
    int     cursor_line;
    int     cursor_col;
    int     scroll_line;
    int     modified;
} TextEditor;

// ─── Funciones principales ────────────────────────────────────────────────

TextEditor* editor_create(const char *filename)
{
    TextEditor *ed = (TextEditor*)malloc(sizeof(TextEditor));
    if(!ed) return 0;
    
    ed->content_capacity = 65536;  // 64KB inicial
    ed->content = (char*)malloc(ed->content_capacity);
    if(!ed->content) {
        free(ed);
        return 0;
    }
    
    strcpy(ed->filename, filename);
    ed->content_size = 0;
    ed->cursor_line = 0;
    ed->cursor_col = 0;
    ed->scroll_line = 0;
    ed->modified = 0;
    
    // Cargar archivo desde filesystem
    extern int syscall_open(const char *fname, int flags);
    extern long syscall_read(int fd, void *buf, unsigned long count);
    extern int syscall_close(int fd);
    
    int fd = syscall_open(filename, 0);  // O_RDONLY
    if(fd >= 0) {
        long bytes_read = syscall_read(fd, ed->content, ed->content_capacity - 1);
        if(bytes_read > 0) {
            ed->content_size = bytes_read;
            ed->content[ed->content_size] = 0;
        }
        syscall_close(fd);
    }
    
    return ed;
}

void editor_destroy(TextEditor *ed)
{
    if(!ed) return;
    if(ed->content) free(ed->content);
    free(ed);
}

void editor_insert_char(TextEditor *ed, char c)
{
    if(!ed) return;
    if(ed->content_size + 1 >= ed->content_capacity) {
        // Expandir buffer
        ed->content_capacity *= 2;
        char *new_content = (char*)malloc(ed->content_capacity);
        if(!new_content) return;
        memcpy(new_content, ed->content, ed->content_size);
        free(ed->content);
        ed->content = new_content;
    }
    
    // Encontrar posición del cursor en bytes
    int cursor_byte_pos = 0;
    int current_line = 0;
    int current_col = 0;
    
    for(int i = 0; i < ed->content_size; i++) {
        if(current_line == ed->cursor_line && current_col == ed->cursor_col) {
            cursor_byte_pos = i;
            break;
        }
        
        if(ed->content[i] == '\n') {
            current_line++;
            current_col = 0;
        } else {
            current_col++;
        }
    }
    
    // Si cursor está al final, insertar al final
    if(ed->cursor_line * 100 + ed->cursor_col >= ed->content_size) {
        cursor_byte_pos = ed->content_size;
    }
    
    // Desplazar contenido a la derecha (hacer espacio)
    for(int i = ed->content_size; i > cursor_byte_pos; i--) {
        ed->content[i] = ed->content[i - 1];
    }
    
    // Insertar carácter
    ed->content[cursor_byte_pos] = c;
    ed->content_size++;
    ed->content[ed->content_size] = 0;
    ed->modified = 1;
    
    // Actualizar posición cursor
    if(c == '\n') {
        ed->cursor_line++;
        ed->cursor_col = 0;
    } else if(c == '\t') {
        ed->cursor_col += 4;  // Tab = 4 espacios
    } else {
        ed->cursor_col++;
    }
}

// Función para guardar archivo
void editor_save(TextEditor *ed)
{
    if(!ed) return;
    
    extern int syscall_open(const char *fname, int flags);
    extern long syscall_write(int fd, const void *buf, unsigned long count);
    extern int syscall_close(int fd);
    
    // Abrir para escritura: O_WRONLY | O_CREAT | O_TRUNC = 0x41
    int fd = syscall_open(ed->filename, 0x41);
    if(fd < 0) return;
    
    // Escribir contenido
    if(syscall_write(fd, ed->content, ed->content_size) == (long)ed->content_size) {
        ed->modified = 0;
    }
    
    syscall_close(fd);
}

// Función para eliminar carácter (backspace)
void editor_delete_char(TextEditor *ed)
{
    if(!ed || ed->cursor_col == 0) return;
    
    // Encontrar posición del cursor
    int cursor_byte_pos = 0;
    int current_line = 0;
    int current_col = 0;
    
    for(int i = 0; i < ed->content_size; i++) {
        if(current_line == ed->cursor_line && current_col == ed->cursor_col) {
            cursor_byte_pos = i;
            break;
        }
        
        if(ed->content[i] == '\n') {
            current_line++;
            current_col = 0;
        } else {
            current_col++;
        }
    }
    
    if(cursor_byte_pos == 0) return;
    
    // Desplazar contenido a la izquierda
    for(int i = cursor_byte_pos - 1; i < ed->content_size - 1; i++) {
        ed->content[i] = ed->content[i + 1];
    }
    
    ed->content_size--;
    ed->content[ed->content_size] = 0;
    ed->modified = 1;
    
    // Retroceder cursor
    ed->cursor_col--;
}

void editor_draw(TextEditor *ed, void *window, uint32_t *fb, 
                 uint32_t fb_width, uint32_t fb_height,
                 int wx, int wy, int ww, int wh)
{
    if(!ed || !window) return;
    
    // Fondo blanco
    wm_clear_window(window, 0xFFFAFAFA);
    
    // Dibujar contenido
    int y = wy + 25;
    int line = 0;
    int col = 0;
    
    for(int i = 0; i < ed->content_size && y < wy + wh; i++) {
        char c = ed->content[i];
        
        if(c == '\n') {
            line++;
            col = 0;
            y += 12;  // Altura de línea con fuente monoespaciado
        } else {
            col++;
            // Dibujar carácter (con font monoespaciado de 8px ancho)
            // Usar draw_string_fb con caracteres individuales
            char buf[2] = {c, 0};
            draw_string_fb(fb, fb_width, fb_height, 
                          wx + 5 + (col-1) * 8, y, buf, 0xFF000000);
        }
    }
    
    // Mostrar información
    char info[64];
    sprintf(info, "Line: %d Col: %d Size: %d", ed->cursor_line, ed->cursor_col, ed->content_size);
    draw_string_fb(fb, fb_width, fb_height, wx + 5, wy + wh - 15, info, 0xFF666666);
}

// ─── Función principal de prueba ──────────────────────────────────────────

int text_editor_main(const char *filename)
{
    TextEditor *editor = editor_create(filename);
    if(!editor) return -1;
    
    void *window = wm_create_window(50, 50, 800, 600, filename);
    if(!window) {
        editor_destroy(editor);
        return -1;
    }
    
    wm_write(window, 10, 20, "Text Editor - GPT-OS", 0xFF000000);
    wm_write(window, 10, 40, "File: ", 0xFF000000);
    wm_write(window, 60, 40, filename, 0xFF0078D4);
    
    // Loop principal - Implementar event loop con entrada de teclado
    // El event loop integra:
    // - Lectura de input del window manager (teclado, mouse)
    // - Procesamiento de comandos (Ctrl+S, Ctrl+Q, etc)
    // - Actualización de display
    // - Gestión de cursor e inserción
    
    uint32_t *fb = fb_get_addr();
    uint32_t fb_width = fb_get_width();
    uint32_t fb_height = fb_get_height();
    
    int running = 1;
    while(running) {
        // Procesar entrada de teclado (cuando esté disponible):
        // extern int wm_get_event(void *win, int *event_type, int *key);
        // int event, key;
        // if(wm_get_event(window, &event, &key) == 0) {
        //     if(event == EVENT_KEY_PRESS) {
        //         if(key >= 32 && key < 127) {
        //             editor_insert_char(editor, key);
        //         } else if(key == KEY_BACKSPACE) {
        //             editor_delete_char(editor);
        //         } else if(key == KEY_ENTER) {
        //             editor_insert_char(editor, '\n');
        //         } else if(key == KEY_TAB) {
        //             editor_insert_char(editor, '\t');
        //         } else if(key == KEY_UP) {
        //             if(editor->cursor_line > 0) editor->cursor_line--;
        //         } else if(key == KEY_DOWN) {
        //             if(editor->cursor_line < 99) editor->cursor_line++;
        //         } else if(key == KEY_LEFT) {
        //             if(editor->cursor_col > 0) editor->cursor_col--;
        //         } else if(key == KEY_RIGHT) {
        //             editor->cursor_col++;
        //         }
        //     } else if(event == EVENT_KEY_CTRL) {
        //         if(key == 'S') {           // Ctrl+S: guardar
        //             editor_save(editor);
        //         } else if(key == 'Q') {   // Ctrl+Q: salir
        //             running = 0;
        //         } else if(key == 'A') {   // Ctrl+A: seleccionar todo
        //             // Implementar selección
        //         } else if(key == 'C') {   // Ctrl+C: copiar
        //             // Implementar copia
        //         } else if(key == 'V') {   // Ctrl+V: pegar
        //             // Implementar pegado
        //         }
        //     }
        // }
        
        // Actualizar pantalla
        editor_draw(editor, window, fb, fb_width, fb_height, 50, 50, 800, 600);
        
        // Mostrar información de estado
        char status[128];
        char modified_marker[2] = {editor->modified ? '*' : ' ', 0};
        sprintf(status, "Line: %d Col: %d Size: %d %s", 
                editor->cursor_line, editor->cursor_col, editor->content_size, modified_marker);
        draw_string_fb(fb, fb_width, fb_height, 50, 635, status, 0xFF666666);
        
        sys_sleep_ms(100);
    }
    
    // Preguntar si guardar cambios antes de salir
    if(editor->modified) {
        wm_write(window, 10, 300, "Save changes? (Y/N)", 0xFF000000);
        // Esperar entrada... (en implementación real)
        // Si Y: editor_save(editor);
    }
    
    editor_destroy(editor);
    return 0;
}
