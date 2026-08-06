// userland/file_manager/file_manager.c
// Implementación del Gestor de Archivos

#include "file_manager.h"
#include "../libc/stdlib.h"
#include "../libc/string.h"

// ─── Forward declarations ──────────────────────────────────────────────────
extern void draw_string_fb(uint32_t*, uint32_t, uint32_t, int, int, const char*, uint32_t);
extern void wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color);
extern void wm_write(void *win, int x, int y, const char *text, uint32_t color);
extern uint32_t fb_get_width(void);
extern uint32_t fb_get_height(void);

// ─── Stub: Lectura de directorio (implementar en kernel) ─────────────────────
// Conectar con syscalls del kernel para leer directorios

// Estructura de directorio (similar a dirent)
typedef struct {
    uint32_t inode;
    uint32_t size;
    uint16_t type;     // 1=regular, 2=directory, etc
    uint16_t length;
    char name[256];
} DirEntry;

// Implementación simplificada de sys_listdir usando syscalls
static int sys_listdir(const char *path, FileInfo *files, int max_count)
{
    extern int syscall_opendir(const char *path);
    extern int syscall_readdir(int fd, void *dirent);
    extern int syscall_closedir(int fd);
    
    if(!path || !files) return 0;
    
    // Abrir directorio
    int dirfd = syscall_opendir(path);
    if(dirfd < 0) return 0;
    
    int count = 0;
    DirEntry entry;
    
    // Leer entries del directorio
    while(count < max_count && syscall_readdir(dirfd, &entry) > 0) {
        // Saltar . y ..
        if(strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }
        
        // Copiar información a FileInfo (usar campos definidos en file_types.h)
        strcpy(files[count].name, entry.name);
        files[count].size = entry.size;
        files[count].is_dir = (entry.type == 2) ? 1 : 0;
        
        count++;
    }
    
    syscall_closedir(dirfd);
    return count;
}

static int string_len(const char *s) {
    int len = 0;
    while(s && *s) { len++; s++; }
    return len;
}

static char* string_copy(char *dst, const char *src, int max) {
    int i = 0;
    while(src && src[i] && i < max-1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
    return dst;
}

static char* path_join(char *dst, const char *base, const char *name, int max) {
    int blen = string_len(base);
    
    string_copy(dst, base, max);
    if(blen > 0 && dst[blen-1] != '/') {
        dst[blen] = '/';
        blen++;
    }
    string_copy(dst + blen, name, max - blen);
    return dst;
}

// Implementar bubble sort simple para ordenar archivos (directorios primero)
static void fm_sort_files(FileInfo *files, int count)
{
    if(!files || count <= 1) return;
    
    for(int i = 0; i < count - 1; i++) {
        for(int j = 0; j < count - i - 1; j++) {
            FileInfo f1 = files[j];
            FileInfo f2 = files[j + 1];
            
            // Directorios primero
            if((f1.is_dir == 0) && (f2.is_dir == 1)) {
                // Intercambiar
                FileInfo tmp = files[j];
                files[j] = files[j + 1];
                files[j + 1] = tmp;
            }
            // Si ambos son del mismo tipo, ordenar alfabéticamente
            else if(f1.is_dir == f2.is_dir) {
                if(string_len(f1.name) > 0 && string_len(f2.name) > 0) {
                    if(strcmp(f1.name, f2.name) > 0) {
                        FileInfo tmp = files[j];
                        files[j] = files[j + 1];
                        files[j + 1] = tmp;
                    }
                }
            }
        }
    }
}

// ─── Crear gestor ───────────────────────────────────────────────────────────

FileManager* fm_create(const char *initial_path, void *parent_window)
{
    FileManager *fm = (FileManager*)malloc(sizeof(FileManager));
    if(!fm) return 0;
    
    fm->parent_window = parent_window;
    fm->view_mode = VIEW_MODE_LIST;
    fm->selected_idx = -1;
    fm->scroll_offset = 0;
    fm->file_count = 0;
    fm->file_capacity = 256;
    
    fm->files = (FileInfo*)malloc(sizeof(FileInfo) * fm->file_capacity);
    if(!fm->files) {
        free(fm);
        return 0;
    }
    
    string_copy(fm->current_path, initial_path ? initial_path : "/", 512);
    
    fm_refresh(fm);
    
    return fm;
}

// ─── Destruir gestor ────────────────────────────────────────────────────────

void fm_destroy(FileManager *fm)
{
    if(!fm) return;
    if(fm->files) free(fm->files);
    free(fm);
}

// ─── Refrescar lista ────────────────────────────────────────────────────────

void fm_refresh(FileManager *fm)
{
    if(!fm) return;
    
    // Limpiar lista anterior
    fm->file_count = 0;
    fm->selected_idx = -1;
    
    // Llamar a syscall para obtener contenido del directorio
    // NOTA: sys_listdir es un stub que debe implementarse en kernel
    int count = sys_listdir(fm->current_path, fm->files, fm->file_capacity);
    
    if(count > 0) {
        fm->file_count = count;
        
        // Clasificar archivos (directorios primero, luego alfabéticamente)
        fm_sort_files(fm->files, fm->file_count);
    }
}

// ─── Navegación ────────────────────────────────────────────────────────────

void fm_navigate(FileManager *fm, const char *path)
{
    if(!fm || !path) return;
    
    string_copy(fm->current_path, path, 512);
    fm_refresh(fm);
}

void fm_go_parent(FileManager *fm)
{
    if(!fm) return;
    
    // Buscar último '/' y truncar
    char *last_slash = 0;
    for(int i = 0; fm->current_path[i]; i++) {
        if(fm->current_path[i] == '/') last_slash = &fm->current_path[i];
    }
    
    if(last_slash && last_slash != fm->current_path) {
        *last_slash = 0;
    } else if(!last_slash) {
        string_copy(fm->current_path, "/", 512);
    }
    
    fm_refresh(fm);
}

// ─── Selección ─────────────────────────────────────────────────────────────

FileInfo* fm_get_selected(FileManager *fm)
{
    if(!fm || fm->selected_idx < 0 || fm->selected_idx >= (int)fm->file_count)
        return 0;
    return &fm->files[fm->selected_idx];
}

void fm_select_file(FileManager *fm, int index)
{
    if(!fm) return;
    if(index >= 0 && index < (int)fm->file_count) {
        fm->selected_idx = index;
    } else {
        fm->selected_idx = -1;
    }
}

void fm_move_selection(FileManager *fm, int delta)
{
    if(!fm) return;
    
    int new_idx = fm->selected_idx + delta;
    if(new_idx < 0) new_idx = 0;
    if(new_idx >= (int)fm->file_count) new_idx = fm->file_count - 1;
    
    fm->selected_idx = new_idx;
}

// ─── Modo de vista ────────────────────────────────────────────────────────────

void fm_set_view_mode(FileManager *fm, ViewMode mode)
{
    if(fm) fm->view_mode = mode;
}

// ─── Dibujo ────────────────────────────────────────────────────────────────

void fm_draw_navbar(FileManager *fm, uint32_t *framebuffer, uint32_t fb_width,
                    int x, int y, int w, int h)
{
    if(!fm || !framebuffer) return;
    
    // Fondo gris de barra de navegación
    for(int row = y; row < y + h && row < (int)fb_get_height(); row++) {
        for(int col = x; col < x + w && col < (int)fb_width; col++) {
            if(row >= 0 && col >= 0) {
                framebuffer[row * fb_width + col] = 0xFF404040;
            }
        }
    }
    
    // Mostrar ruta actual
    draw_string_fb(framebuffer, fb_width, fb_get_height(), 
                   x + 5, y + 4, fm->current_path, 0xFFFFFFFF);
}

void fm_draw(FileManager *fm, uint32_t *framebuffer, uint32_t fb_width,
             uint32_t fb_height, int win_x, int win_y, int win_w, int win_h)
{
    if(!fm || !framebuffer) return;
    
    int content_y = win_y + 25;  // Dejar espacio para navbar
    int content_h = win_h - 30;
    int item_height = 16;
    
    // Dibujar fondo blanco
    for(int row = content_y; row < win_y + win_h && row < (int)fb_height; row++) {
        for(int col = win_x; col < win_x + win_w && col < (int)fb_width; col++) {
            if(row >= 0 && col >= 0) {
                framebuffer[row * fb_width + col] = 0xFFF5F5F5;
            }
        }
    }
    
    // Dibujar lista de archivos
    int visible_items = content_h / item_height;
    
    for(int i = 0; i < (int)fm->file_count && i < visible_items; i++) {
        FileInfo *info = &fm->files[i];
        int item_y = content_y + (i * item_height);
        
        // Fondo de selección
        if(i == fm->selected_idx) {
            for(int row = item_y; row < item_y + item_height; row++) {
                for(int col = win_x; col < win_x + win_w; col++) {
                    if(row < (int)fb_height && col < (int)fb_width && col >= 0 && row >= 0) {
                        framebuffer[row * fb_width + col] = 0xFF0078D4;  // Azul Windows
                    }
                }
            }
        }
        
        // Dibujar icono y nombre
        const char *icon = file_get_icon(info->type);
        uint32_t text_color = (i == fm->selected_idx) ? 0xFFFFFFFF : 0xFF000000;
        
        char line[256];
        string_copy(line, icon, 256);
        string_copy(line + 3, " ", 256 - 3);
        string_copy(line + 4, info->name, 256 - 4);
        
        draw_string_fb(framebuffer, fb_width, fb_height,
                       win_x + 5, item_y + 2, line, text_color);
    }
}

