// userland/file_manager/file_manager.h
// Gestor de archivos tipo Windows

#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "file_types.h"
#include <stdint.h>

// ─── Modos de vista ────────────────────────────────────────────────────────
typedef enum {
    VIEW_MODE_ICONS,    // Vista de iconos grandes
    VIEW_MODE_LIST,     // Vista de lista detallada
    VIEW_MODE_COMPACT,  // Vista compacta
} ViewMode;

// ─── Estructura del Gestor de Archivos ─────────────────────────────────────
typedef struct FileManager {
    char           current_path[512];   // Directorio actual
    FileInfo      *files;               // Array de archivos
    uint32_t       file_count;          // Cantidad de archivos
    uint32_t       file_capacity;       // Capacidad del array
    int32_t        selected_idx;        // Índice seleccionado (-1 si ninguno)
    ViewMode       view_mode;           // Modo de vista actual
    
    void          *parent_window;       // Referencia a ventana padre
    uint32_t       scroll_offset;       // Para scrolling
    
} FileManager;

// ─── Funciones principales ────────────────────────────────────────────────

// Crear un nuevo gestor de archivos
FileManager* fm_create(const char *initial_path, void *parent_window);

// Destruir gestor de archivos
void fm_destroy(FileManager *fm);

// Navegar a un directorio
void fm_navigate(FileManager *fm, const char *path);

// Ir al directorio padre
void fm_go_parent(FileManager *fm);

// Obtener archivo seleccionado
FileInfo* fm_get_selected(FileManager *fm);

// Cambiar selección
void fm_select_file(FileManager *fm, int index);

// Mover selección (arriba/abajo)
void fm_move_selection(FileManager *fm, int delta);

// Cambiar modo de vista
void fm_set_view_mode(FileManager *fm, ViewMode mode);

// Refrescar lista de archivos
void fm_refresh(FileManager *fm);

// ─── Dibujo ────────────────────────────────────────────────────────────────

// Dibujar el contenido del gestor en una ventana
void fm_draw(FileManager *fm, uint32_t *framebuffer, uint32_t fb_width, 
             uint32_t fb_height, int win_x, int win_y, int win_w, int win_h);

// Dibujar barra de navegación
void fm_draw_navbar(FileManager *fm, uint32_t *framebuffer, uint32_t fb_width,
                    int x, int y, int w, int h);

#endif // FILE_MANAGER_H
