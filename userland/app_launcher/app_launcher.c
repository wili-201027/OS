// userland/app_launcher/app_launcher.c
// Implementación del sistema de lanzamiento de aplicaciones

#include "app_launcher.h"
#include "../libc/stdlib.h"
#include "../libc/string.h"

// ─── Funciones de utilidad ──────────────────────────────────────────────────

static int string_ends_with(const char *str, const char *suffix)
{
    if(!str || !suffix) return 0;
    int slen = strlen(str);
    int rlen = strlen(suffix);
    if(rlen > slen) return 0;
    return strcmp(str + slen - rlen, suffix) == 0;
}

// ─── Crear registro ─────────────────────────────────────────────────────────

AppRegistry* app_registry_create(void)
{
    AppRegistry *reg = (AppRegistry*)malloc(sizeof(AppRegistry));
    if(!reg) return 0;
    
    reg->app_count = 0;
    reg->association_count = 0;
    
    reg->apps = (AppInfo*)malloc(sizeof(AppInfo) * 32);
    if(!reg->apps) {
        free(reg);
        return 0;
    }
    
    reg->associations = (FileAssociation*)malloc(sizeof(FileAssociation) * 64);
    if(!reg->associations) {
        free(reg->apps);
        free(reg);
        return 0;
    }
    
    // ─── Registrar aplicaciones por defecto ──────────────────────────────────
    
    // Text Editor
    AppInfo text_editor = {
        .type = APP_TYPE_TEXT_EDITOR,
        .name = "Text Editor",
        .path = "/bin/text_editor",
        .description = "Editor de texto simple",
        .icon = "[T]",
        .supported_extensions = {".txt", ".c", ".cpp", ".h", ".py", ".js", ".json", ".html", ".css"},
        .extension_count = 9,
    };
    app_registry_register(reg, &text_editor);
    
    // Image Viewer
    AppInfo image_viewer = {
        .type = APP_TYPE_IMAGE_VIEWER,
        .name = "Image Viewer",
        .path = "/bin/image_viewer",
        .description = "Visor de imágenes",
        .icon = "[I]",
        .supported_extensions = {".bmp", ".jpg", ".jpeg", ".png", ".gif", ".ico"},
        .extension_count = 6,
    };
    app_registry_register(reg, &image_viewer);
    
    // Media Player
    AppInfo media_player = {
        .type = APP_TYPE_MEDIA_PLAYER,
        .name = "Media Player",
        .path = "/bin/media_player",
        .description = "Reproductor de audio y video",
        .icon = "[♪]",
        .supported_extensions = {".mp3", ".wav", ".ogg", ".mp4", ".avi", ".mkv"},
        .extension_count = 6,
    };
    app_registry_register(reg, &media_player);
    
    // Web Browser
    AppInfo web_browser = {
        .type = APP_TYPE_WEB_BROWSER,
        .name = "Web Browser",
        .path = "/bin/web_browser",
        .description = "Navegador web simple",
        .icon = "[W]",
        .supported_extensions = {".html", ".htm", ".php", ".xml"},
        .extension_count = 4,
    };
    app_registry_register(reg, &web_browser);
    
    // ─── Crear asociaciones de archivo ─────────────────────────────────────
    
    // Archivos de texto
    FileAssociation txt_assoc = {
        .extension = ".txt",
        .default_app = APP_TYPE_TEXT_EDITOR,
        .app_count = 0,
    };
    // Agregar a reg->associations
    if(reg->association_count < 64) {
        memcpy(&reg->associations[reg->association_count], &txt_assoc, 
               sizeof(FileAssociation));
        reg->association_count++;
    }
    
    // Imágenes
    FileAssociation img_assoc = {
        .extension = ".jpg",
        .default_app = APP_TYPE_IMAGE_VIEWER,
        .app_count = 0,
    };
    // Agregar a reg->associations
    if(reg->association_count < 64) {
        memcpy(&reg->associations[reg->association_count], &img_assoc, 
               sizeof(FileAssociation));
        reg->association_count++;
    }
    
    return reg;
}

// ─── Destruir registro ───────────────────────────────────────────────────────

void app_registry_destroy(AppRegistry *reg)
{
    if(!reg) return;
    if(reg->apps) free(reg->apps);
    if(reg->associations) free(reg->associations);
    free(reg);
}

// ─── Registrar aplicación ───────────────────────────────────────────────────

void app_registry_register(AppRegistry *reg, AppInfo *app)
{
    if(!reg || !app || reg->app_count >= 32) return;
    
    memcpy(&reg->apps[reg->app_count], app, sizeof(AppInfo));
    reg->app_count++;
}

// ─── Obtener aplicación por tipo ──────────────────────────────────────────

AppInfo* app_registry_get_by_type(AppRegistry *reg, AppType type)
{
    if(!reg) return 0;
    
    for(uint32_t i = 0; i < reg->app_count; i++) {
        if(reg->apps[i].type == type) {
            return &reg->apps[i];
        }
    }
    
    return 0;
}

// ─── Obtener aplicación por extensión ────────────────────────────────────

AppInfo* app_registry_get_by_extension(AppRegistry *reg, const char *ext)
{
    if(!reg || !ext) return 0;
    
    // Buscar basándose en extensión (buscar en todas las apps)
    for(uint32_t i = 0; i < reg->app_count; i++) {
        AppInfo *app = &reg->apps[i];
        
        for(int j = 0; j < app->extension_count; j++) {
            if(strcmp(app->supported_extensions[j], ext) == 0) {
                return app;
            }
        }
    }
    
    // Fallback: Retornar text editor si no se encuentra
    return app_registry_get_by_type(reg, APP_TYPE_TEXT_EDITOR);
}

// ─── Lanzar aplicación ──────────────────────────────────────────────────────

int app_launch(AppInfo *app, const char *filename)
{
    if(!app || !filename) return -1;
    
    // Syscall declarations
    extern int syscall_fork(void);
    extern int syscall_execve(const char *path, char *const argv[], char *const envp[]);
    extern int syscall_wait(int pid, int *status);
    
    // Fork: crear nuevo proceso
    int pid = syscall_fork();
    
    if(pid < 0) {
        // Error en fork
        return -1;
    }
    
    if(pid == 0) {
        // Proceso hijo: ejecutar la aplicación
        char *argv[3];
        argv[0] = (char*)app->path;
        argv[1] = (char*)filename;
        argv[2] = 0;  // NULL terminator
        
        char *envp[] = {0};  // Environment vacío
        
        // Execve: reemplazar imagen del proceso
        int ret = syscall_execve(app->path, argv, envp);
        
        // Si execve retorna, hubo error
        return ret;
    } else {
        // Proceso padre: esperar a que termine el hijo (opcional)
        // int status;
        // syscall_wait(pid, &status);
        // return status;
        
        // O retorna inmediatamente (lanzamiento asincrónico)
        return 0;
    }
}

// ─── Lanzar por extensión ───────────────────────────────────────────────────

int app_launch_by_extension(AppRegistry *reg, const char *filename)
{
    if(!reg || !filename) return -1;
    
    // Obtener extensión del archivo
    const char *dot = 0;
    for(int i = 0; filename[i]; i++) {
        if(filename[i] == '.') dot = &filename[i];
    }
    
    if(!dot) return -1;
    
    // Obtener app por extensión
    AppInfo *app = app_registry_get_by_extension(reg, dot);
    if(!app) return -1;
    
    // Lanzar app
    return app_launch(app, filename);
}
