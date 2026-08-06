// userland/compositor/file_manager_main.c
// Integración del Gestor de Archivos en el compositor

#include "window_manager.h"
#include "../file_manager/file_manager.h"
#include "../file_manager/file_types.h"
#include "../app_launcher/app_launcher.h"
#include "../libc/string.h"
#include "../libc/stdio.h"

extern uint32_t  fb_get_width(void);
extern uint32_t  fb_get_height(void);
extern uint32_t *fb_get_addr(void);
extern void      draw_string_fb(uint32_t*, uint32_t, uint32_t, int, int, const char*, uint32_t);
extern uint8_t   ps2_read_scancode_nowait(void);
extern void      sys_sleep_ms(uint32_t ms);

// ─── Contexto global del gestor de archivos ──────────────────────────────────
typedef struct {
    FileManager *file_manager;
    void        *window;
    AppRegistry *app_registry;
    int         is_active;
} FileManagerContext;

static FileManagerContext fm_ctx = {0};

// ─── Inicializar el gestor de archivos ─────────────────────────────────────

void file_manager_init(void)
{
    if(fm_ctx.is_active) return;
    
    // Crear registro de aplicaciones
    fm_ctx.app_registry = app_registry_create();
    
    // Crear ventana del gestor
    fm_ctx.window = wm_create_window(10, 30, 960, 680, "File Manager");
    
    // Crear gestor de archivos
    fm_ctx.file_manager = fm_create("/", fm_ctx.window);
    
    fm_ctx.is_active = 1;
}

// ─── Destruir el gestor de archivos ────────────────────────────────────────

void file_manager_shutdown(void)
{
    if(!fm_ctx.is_active) return;
    
    if(fm_ctx.file_manager) {
        fm_destroy(fm_ctx.file_manager);
        fm_ctx.file_manager = 0;
    }
    
    if(fm_ctx.app_registry) {
        app_registry_destroy(fm_ctx.app_registry);
        fm_ctx.app_registry = 0;
    }
    
    fm_ctx.is_active = 0;
}

// ─── Procesar entrada del gestor ──────────────────────────────────────────

void file_manager_process_input(void)
{
    if(!fm_ctx.is_active || !fm_ctx.file_manager) return;
    
    uint8_t scancode = ps2_read_scancode_nowait();
    if(scancode == 0) return;
    
    switch(scancode) {
        // Navegación arriba/abajo
        case 0x48:  // Flecha arriba
            fm_move_selection(fm_ctx.file_manager, -1);
            break;
        case 0x50:  // Flecha abajo
            fm_move_selection(fm_ctx.file_manager, 1);
            break;
        
        // Navegar a carpeta o ejecutar
        case 0x1C:  // Enter
        {
            FileInfo *selected = fm_get_selected(fm_ctx.file_manager);
            if(selected) {
                if(selected->is_dir) {
                    // Navegar a la carpeta
                    char new_path[512];
                    strcpy(new_path, fm_ctx.file_manager->current_path);
                    strcat(new_path, "/");
                    strcat(new_path, selected->name);
                    fm_navigate(fm_ctx.file_manager, new_path);
                } else {
                    // Lanzar aplicación asociada
                    char full_path[512];
                    strcpy(full_path, fm_ctx.file_manager->current_path);
                    strcat(full_path, "/");
                    strcat(full_path, selected->name);
                    
                    app_launch_by_extension(fm_ctx.app_registry, full_path);
                }
            }
            break;
        }
        
        // Volver atrás
        case 0x0E:  // Backspace
            fm_go_parent(fm_ctx.file_manager);
            break;
        
        // Cambiar modo de vista (V)
        case 0x2F:  // V
        {
            ViewMode current = fm_ctx.file_manager->view_mode;
            ViewMode next = (current == VIEW_MODE_LIST) ? VIEW_MODE_ICONS : VIEW_MODE_LIST;
            fm_set_view_mode(fm_ctx.file_manager, next);
            break;
        }
        
        // Refrescar (F5)
        case 0x3F:  // F5
            fm_refresh(fm_ctx.file_manager);
            break;
    }
}

// ─── Dibujar el gestor de archivos ───────────────────────────────────────

void file_manager_draw(uint32_t *framebuffer, uint32_t fb_width, uint32_t fb_height)
{
    if(!fm_ctx.is_active || !fm_ctx.file_manager || !framebuffer) return;
    
    // Posición y dimensiones de la ventana
    int win_x = 10;
    int win_y = 30;
    int win_w = 960;
    int win_h = 680;
    
    // Dibujar fondo de la ventana
    for(int row = win_y; row < win_y + win_h && row < (int)fb_height; row++) {
        for(int col = win_x; col < win_x + win_w && col < (int)fb_width; col++) {
            framebuffer[row * fb_width + col] = 0xFFF5F5F5;
        }
    }
    
    // Titre de la ventana
    draw_string_fb(framebuffer, fb_width, fb_height,
                   win_x + 5, win_y + 5, "📁 File Manager", 0xFF000000);
    
    // Dibujar barra de navegación
    fm_draw_navbar(fm_ctx.file_manager, framebuffer, fb_width,
                   win_x, win_y + 20, win_w, 25);
    
    // Dibujar contenido del gestor
    fm_draw(fm_ctx.file_manager, framebuffer, fb_width, fb_height,
            win_x, win_y + 50, win_w, win_h - 70);
    
    // Dibujar barra de estado (bottom)
    FileInfo *selected = fm_get_selected(fm_ctx.file_manager);
    if(selected) {
        char status[256];
        sprintf(status, "Selected: %s | Type: %s | Size: %u bytes",
                selected->name, file_get_type_desc(selected->type), selected->size);
        
        draw_string_fb(framebuffer, fb_width, fb_height,
                       win_x + 5, win_y + win_h - 20, status, 0xFF666666);
    }
    
    // Controles de ayuda
    draw_string_fb(framebuffer, fb_width, fb_height,
                   win_x + 5, win_y + win_h - 10, 
                   "↑↓: Select | Enter: Open | Backspace: Back | V: View | F5: Refresh",
                   0xFF999999);
}

// ─── Función para ser llamada desde el compositor principal ───────────────

void file_manager_update(uint32_t *framebuffer, uint32_t fb_width, uint32_t fb_height)
{
    if(!fm_ctx.is_active) {
        file_manager_init();
    }
    
    file_manager_process_input();
    file_manager_draw(framebuffer, fb_width, fb_height);
}
