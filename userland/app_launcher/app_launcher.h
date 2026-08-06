// userland/app_launcher/app_launcher.h
// Sistema de lanzamiento de aplicaciones con asociaciones de archivos

#ifndef APP_LAUNCHER_H
#define APP_LAUNCHER_H

#include <stdint.h>

// ─── Tipos de aplicación ────────────────────────────────────────────────────
typedef enum {
    APP_TYPE_UNKNOWN,
    APP_TYPE_TEXT_EDITOR,
    APP_TYPE_IMAGE_VIEWER,
    APP_TYPE_MEDIA_PLAYER,
    APP_TYPE_WEB_BROWSER,
    APP_TYPE_FILE_MANAGER,
    APP_TYPE_TERMINAL,
    APP_TYPE_SYSTEM_INFO,
} AppType;

// ─── Información de aplicación ──────────────────────────────────────────────
typedef struct {
    AppType     type;
    const char *name;
    const char *path;          // Ruta a ejecutable
    const char *description;
    const char *icon;
    const char *supported_extensions[16];  // Extensiones que soporta
    int         extension_count;
} AppInfo;

// ─── Asociaciones de archivos ───────────────────────────────────────────────
typedef struct {
    char   extension[32];       // ".txt", ".jpg", etc.
    AppType default_app;        // App por defecto
    AppInfo *available_apps;    // Apps alternativas que pueden abrir el archivo
    int     app_count;
} FileAssociation;

// ─── Registro de aplicaciones ────────────────────────────────────────────────
typedef struct {
    AppInfo          *apps;
    uint32_t          app_count;
    FileAssociation  *associations;
    uint32_t          association_count;
} AppRegistry;

// ─── Funciones ──────────────────────────────────────────────────────────────

// Crear registro de aplicaciones
AppRegistry* app_registry_create(void);

// Destruir registro
void app_registry_destroy(AppRegistry *reg);

// Registrar una aplicación
void app_registry_register(AppRegistry *reg, AppInfo *app);

// Obtener aplicación por tipo
AppInfo* app_registry_get_by_type(AppRegistry *reg, AppType type);

// Obtener aplicación por defecto para una extensión
AppInfo* app_registry_get_by_extension(AppRegistry *reg, const char *ext);

// Lanzar aplicación con archivo
int app_launch(AppInfo *app, const char *filename);

// Lanzar aplicación por extensión de archivo
int app_launch_by_extension(AppRegistry *reg, const char *filename);

#endif // APP_LAUNCHER_H
