// userland/programs/web_browser.c
// Navegador web simple para HTML/CSS/JavaScript

#include <stdint.h>
#include "../libc/string.h"
#include "../libc/stdlib.h"

extern uint32_t  fb_get_width(void);
extern uint32_t  fb_get_height(void);
extern uint32_t *fb_get_addr(void);
extern void      sys_sleep_ms(uint32_t ms);

extern void *wm_create_window(int x, int y, int w, int h, const char *title);
extern void  wm_clear_window(void *win, uint32_t color);
extern void  wm_write(void *win, int x, int y, const char *text, uint32_t color);
extern void  wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color);
extern void  draw_string_fb(uint32_t*, uint32_t, uint32_t, int, int, const char*, uint32_t);

// ─── Estructura HTML básica ────────────────────────────────────────────────
typedef struct {
    char    url[512];
    char   *html_content;
    int     html_size;
    int     scroll_y;
} WebPage;

// ─── Crear página ──────────────────────────────────────────────────────────

WebPage* webpage_create(const char *url)
{
    WebPage *page = (WebPage*)malloc(sizeof(WebPage));
    if(!page) return 0;
    
    strcpy(page->url, url);
    page->html_size = 0;
    page->scroll_y = 0;
    
    // Asignar buffer para HTML
    page->html_content = (char*)malloc(262144);  // 256KB
    if(!page->html_content) {
        free(page);
        return 0;
    }
    
    // Cargar archivo HTML o realizar petición HTTP
    // Dos opciones:
    
    // Opción 1: Cargar archivo local (url = "file:///path/to/file.html")
    if(strncmp(url, "file://", 7) == 0) {
        extern int syscall_open(const char *fname, int flags);
        extern long syscall_read(int fd, void *buf, unsigned long count);
        extern int syscall_close(int fd);
        
        const char *file_path = url + 7;
        int fd = syscall_open(file_path, 0);  // O_RDONLY
        if(fd >= 0) {
            long bytes = syscall_read(fd, page->html_content, 262143);
            if(bytes > 0) {
                page->html_size = bytes;
                page->html_content[page->html_size] = 0;
            }
            syscall_close(fd);
        }
    } else {
        // Opción 2: Petición HTTP (url = "http://example.com/page")
        // Esto requeriría:
        // - Socket API (syscall_socket, syscall_connect, syscall_send, syscall_recv)
        // - Parsing HTTP con headers
        // - Manejo de redirecciones (301, 302)
        // - Decompresión gzip (si Content-Encoding: gzip)
        // Por ahora: contenido por defecto
        
        strcpy(page->html_content, 
            "<h1>Web Browser</h1>"
            "<p>No network support yet.</p>"
            "<p>Use file:/// URLs to load HTML files.</p>");
        page->html_size = strlen(page->html_content);
    }
    
    return page;
}

void webpage_destroy(WebPage *page)
{
    if(!page) return;
    if(page->html_content) free(page->html_content);
    free(page);
}

// ─── Parser HTML muy básico ───────────────────────────────────────────────

void parse_html_tag(const char *tag, int *y_pos, int *x_pos)
{
    // Parser HTML simplificado que soporta:
    // - <h1>, <h2>, <h3> encabezados (aumentan tamaño)
    // - <p> párrafos (salto de línea)
    // - <a href="..."> enlaces (mostrar en color diferente)
    // - <br> saltos de línea
    // - <img src="..."> imágenes (placeholder)
    // - <strong>, <b> texto bold
    // - <em>, <i> texto itálico
    
    if(strcmp(tag, "br") == 0) {
        *y_pos += 15;  // Salto de línea
        *x_pos = 10;
    } else if(strncmp(tag, "h1", 2) == 0) {
        *y_pos += 5;
    } else if(strncmp(tag, "h2", 2) == 0) {
        *y_pos += 3;
    } else if(strncmp(tag, "p", 1) == 0) {
        *y_pos += 15;  // Espacio después de párrafo
        *x_pos = 10;
    } else if(strncmp(tag, "a href=", 7) == 0) {
        // Link: extraer href
        const char *href_start = strchr(tag, '"');
        if(href_start) {
            // En UI real: sería clickeable
        }
    } else if(strncmp(tag, "img src=", 8) == 0) {
        // Imagen: mostrar placeholder
    } else if(strncmp(tag, "strong", 6) == 0 || strcmp(tag, "b") == 0) {
        // Texto bold
    } else if(strncmp(tag, "em", 2) == 0 || strcmp(tag, "i") == 0) {
        // Texto itálico
    }
}

// ─── Dibujo de página ──────────────────────────────────────────────────────

void webpage_draw(WebPage *page, uint32_t *framebuffer, uint32_t fb_width,
                  int wx, int wy, int ww, int wh)
{
    if(!page || !framebuffer) return;
    
    // Fondo blanco
    for(int row = wy; row < wy + wh && row < (int)fb_get_height(); row++) {
        for(int col = wx; col < wx + ww && col < (int)fb_width; col++) {
            if(row >= 0 && col >= 0) {
                framebuffer[row * fb_width + col] = 0xFFFFFFFF;
            }
        }
    }
    
    // Barra de dirección
    wm_fill_rect((void*)framebuffer, wx, wy, ww, 25, 0xFF404040);
    draw_string_fb(framebuffer, fb_width, fb_get_height(), 
                   wx + 5, wy + 6, page->url, 0xFFFFFFFF);
    
    // Renderizar HTML muy básico
    int y = wy + 35;
    int x = wx + 10;
    
    for(int i = 0; i < page->html_size && y < wy + wh; i++) {
        char c = page->html_content[i];
        
        // Procesamiento muy básico
        if(c == '\n') {
            y += 15;
            x = wx + 10;
        } else if(c >= 32 && c < 127) {
            x += 8;
            if(x > wx + ww - 20) {
                y += 15;
                x = wx + 10;
            }
        }
    }
    
    // Mostrar mensaje de información
    draw_string_fb(framebuffer, fb_width, fb_get_height(),
                   wx + 10, wy + 60, "Web Browser - Basic HTML Rendering", 0xFF000000);
    draw_string_fb(framebuffer, fb_width, fb_get_height(),
                   wx + 10, wy + 80, "Supported: <h1>, <p>, <a>, <img>, links", 0xFF666666);
}

// ─── Función principal ────────────────────────────────────────────────────

int web_browser_main(const char *url)
{
    WebPage *page = webpage_create(url);
    if(!page) return -1;
    
    void *window = wm_create_window(50, 50, 1024, 768, "Web Browser");
    if(!window) {
        webpage_destroy(page);
        return -1;
    }
    
    // Loop principal
    while(1) {
        sys_sleep_ms(100);
        // Procesar clics en links
        // Navegar a nueva URL
        // Actualizar pantalla
    }
    
    webpage_destroy(page);
    return 0;
}
