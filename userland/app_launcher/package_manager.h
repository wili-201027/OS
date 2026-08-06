// userland/app_launcher/package_manager.h
// Sistema de gestión de paquetes e instalación

#ifndef PACKAGE_MANAGER_H
#define PACKAGE_MANAGER_H

#include <stdint.h>

// ─── Estados de instalación ──────────────────────────────────────────
typedef enum {
    PKG_STATE_NOT_INSTALLED,
    PKG_STATE_INSTALLED,
    PKG_STATE_INSTALLING,
    PKG_STATE_UNINSTALLING,
    PKG_STATE_UPDATING,
    PKG_STATE_ERROR,
} PackageState;

// ─── Tipos de paquete ────────────────────────────────────────────────
typedef enum {
    PKG_TYPE_APPLICATION,
    PKG_TYPE_LIBRARY,
    PKG_TYPE_THEME,
    PKG_TYPE_PLUGIN,
    PKG_TYPE_DRIVER,
} PackageType;

// ─── Estructura de dependencia ───────────────────────────────────────
typedef struct {
    char     name[64];
    char     version_min[16];
    char     version_max[16];
} Dependency;

// ─── Estructura de manifest ──────────────────────────────────────────
typedef struct {
    char           name[128];              // "Text Editor"
    char           description[256];       // Descripción
    char           version[16];            // "1.0.0"
    char           author[64];             // Desarrollador
    char           package_id[64];         // ID único: "com.gpt-os.text_editor"
    PackageType    type;                   // Tipo de paquete
    
    uint32_t       size_bytes;             // Tamaño sin comprimir
    uint32_t       size_compressed;        // Tamaño comprimido
    
    char          *supported_extensions[16]; // ".txt", ".cs", ".py", etc
    int            extension_count;
    
    Dependency    *dependencies;           // Array de dependencias
    int            dependency_count;
    
    uint32_t       min_memory_kb;          // Memoria mínima requerida
    uint32_t       min_disk_kb;            // Espacio disco mínimo
    
    char           icon_file[128];         // Archivo de icono
    char           entry_point[128];       // Función main: "text_editor_main"
    
    uint32_t       install_date;           // Timestamp instalación
    uint32_t       update_date;            // Timestamp última actualización
    uint8_t        is_system;              // 1 si es parte del sistema
    uint8_t        is_required;            // 1 si no puede desinstalarse
    
} PackageManifest;

// ─── Estructura de paquete instalado ───────────────────────────────
typedef struct {
    PackageManifest manifest;
    char            install_path[512];     // Ruta de instalación
    PackageState    state;
    uint32_t        install_size;          // Tamaño real instalado
    uint8_t         enabled;               // 1 si está habilitado
} InstalledPackage;

// ─── Gestor global de paquetes ──────────────────────────────────────
typedef struct {
    InstalledPackage *packages;
    uint32_t          package_count;
    uint32_t          package_capacity;
    
    char              app_store_path[256]; // Ruta del app store
    char              install_base_path[256]; // Ruta base de instalación
    
    uint32_t          total_disk_used;     // Espacio total usado
    uint32_t          total_memory_limit;  // Límite de memoria para apps
} PackageManager;

// ─── Funciones de Package Manager ───────────────────────────────────

// Crear e inicializar
PackageManager* pm_create(void);

// Destruir
void pm_destroy(PackageManager *pm);

// Instalación
int pm_install(PackageManager *pm, const char *package_path);
int pm_uninstall(PackageManager *pm, const char *package_id);
int pm_update(PackageManager *pm, const char *package_id);

// Búsqueda
InstalledPackage* pm_find_by_id(PackageManager *pm, const char *package_id);
InstalledPackage* pm_find_by_extension(PackageManager *pm, const char *ext);
InstalledPackage** pm_find_by_type(PackageManager *pm, PackageType type);

// Gestión
int pm_enable_package(PackageManager *pm, const char *package_id);
int pm_disable_package(PackageManager *pm, const char *package_id);
int pm_get_info(PackageManager *pm, const char *package_id, PackageManifest *manifest);

// Validación
int pm_check_dependencies(PackageManager *pm, const char *package_id);
int pm_check_space(PackageManager *pm, uint32_t required_kb);
int pm_check_memory(PackageManager *pm, uint32_t required_kb);

// Operaciones de archivo
int pm_extract_package(const char *package_file, const char *destination);
int pm_compress_package(const char *source_dir, const char *output_file);

// Listado
void pm_list_installed(PackageManager *pm);
void pm_list_updateable(PackageManager *pm);

#endif // PACKAGE_MANAGER_H
