// userland/app_launcher/package_manager.c
// Implementación del sistema de gestión de paquetes

#include "package_manager.h"
#include "../libc/stdlib.h"
#include "../libc/string.h"
#include "../libc/stdio.h"

// ─── Forward declarations ──────────────────────────────────────────
extern uint64_t scheduler_get_ticks(void);

static void pm_parse_json_string(char *out, size_t maxlen, const char *buffer, const char *key)
{
    if(!out || maxlen == 0 || !buffer || !key) return;

    const char *key_pos = strstr(buffer, key);
    if(!key_pos) {
        out[0] = '\0';
        return;
    }

    const char *colon = strchr(key_pos, ':');
    if(!colon) {
        out[0] = '\0';
        return;
    }

    const char *quote = strchr(colon, '"');
    if(!quote) {
        out[0] = '\0';
        return;
    }

    quote++;
    const char *end = strchr(quote, '"');
    if(!end) {
        out[0] = '\0';
        return;
    }

    size_t len = end - quote;
    if(len >= maxlen) {
        len = maxlen - 1;
    }

    memcpy(out, quote, len);
    out[len] = '\0';
}

// ─── Syscalls declarations ─────────────────────────────────────────
extern long syscall_read(int fd, void *buf, unsigned long count);
extern long syscall_write(int fd, const void *buf, unsigned long count);
extern long syscall_open(const char *fname, int flags);
extern long syscall_close(int fd);
extern int syscall_unlink(const char *path);
extern int syscall_rmdir(const char *path);
extern int syscall_stat(const char *path, void *stat);
extern int syscall_mkdir(const char *path, int mode);
extern int syscall_opendir(const char *path);
extern int syscall_readdir(int fd, void *dirent);
extern int syscall_closedir(int fd);
extern uint32_t syscall_get_free_memory(void);
extern uint64_t syscall_get_free_disk(void);

// ─── Helper functions ──────────────────────────────────────────────

// Cargar paquetes instalados desde almacenamiento
static int pm_load_installed_packages(PackageManager *pm)
{
    if(!pm) return -1;
    
    // Crear directorio base si no existe
    syscall_mkdir(pm->install_base_path, 0755);
    
    // Cargar desde /apps/installed/packages.db
    char db_path[512];
    sprintf(db_path, "%s/packages.db", pm->install_base_path);
    
    // Abrir archivo de base de datos
    int fd = syscall_open(db_path, 0);  // O_RDONLY = 0
    if(fd < 0) {
        // Archivo no existe, base datos vacía
        pm->package_count = 0;
        pm->total_disk_used = 0;
        return 0;
    }
    
    // Leer encabezado: cuenta de paquetes (uint32_t) + total_disk_used (uint32_t)
    uint32_t header[2];
    if(syscall_read(fd, header, sizeof(header)) != sizeof(header)) {
        syscall_close(fd);
        return -1;
    }
    
    pm->package_count = header[0];
    pm->total_disk_used = header[1];
    
    // Verificar límite
    if(pm->package_count > pm->package_capacity) {
        pm->package_count = pm->package_capacity;
    }
    
    // Leer cada paquete instalado
    for(uint32_t i = 0; i < pm->package_count; i++) {
        InstalledPackage *pkg = &pm->packages[i];
        
        // Estructura simplificada: lo mínimo necesario
        struct {
            char package_id[64];
            char name[128];
            char version[16];
            uint32_t install_size;
            uint32_t install_date;
            uint8_t enabled;
        } stored;
        
        if(syscall_read(fd, &stored, sizeof(stored)) != sizeof(stored)) {
            break;
        }
        
        // Recuperar manifest
        strcpy(pkg->manifest.package_id, stored.package_id);
        strcpy(pkg->manifest.name, stored.name);
        strcpy(pkg->manifest.version, stored.version);
        pkg->manifest.install_date = stored.install_date;
        pkg->install_size = stored.install_size;
        pkg->enabled = stored.enabled;
        pkg->state = PKG_STATE_INSTALLED;
        
        // Reconstruir path
        sprintf(pkg->install_path, "%s/%s", pm->install_base_path, stored.package_id);
    }
    
    syscall_close(fd);
    return 0;
}

// Leer manifest.json de un paquete (formato simplificado)
static int pm_read_manifest(const char *package_path, PackageManifest *manifest)
{
    if(!package_path || !manifest) return -1;
    
    char manifest_path[512];
    sprintf(manifest_path, "%s/manifest.json", package_path);
    
    memset(manifest, 0, sizeof(PackageManifest));
    
    // Intentar leer manifest.json
    int fd = syscall_open(manifest_path, 0);  // O_RDONLY = 0
    if(fd < 0) {
        // Si no existe, usar valores por defecto basados en path
        const char *last_slash = strrchr(package_path, '/');
        if(last_slash) {
            strcpy(manifest->package_id, last_slash + 1);
        }
        manifest->type = PKG_TYPE_APPLICATION;
        manifest->size_bytes = 1024 * 1024;  // 1MB por defecto
        manifest->min_memory_kb = 4096;
        manifest->min_disk_kb = 512;
        return 0;
    }
    
    // Leer JSON simplificado: línea por línea
    char buffer[4096];
    long bytes_read = syscall_read(fd, buffer, sizeof(buffer) - 1);
    if(bytes_read > 0) {
        buffer[bytes_read] = 0;
        
        // Parser JSON ultra simplificado (búsqueda de strings)
        char *p = buffer;
        
        pm_parse_json_string(manifest->name, sizeof(manifest->name), buffer, "\"name\"");
    pm_parse_json_string(manifest->package_id, sizeof(manifest->package_id), buffer, "\"package_id\"");
    pm_parse_json_string(manifest->version, sizeof(manifest->version), buffer, "\"version\"");
    pm_parse_json_string(manifest->description, sizeof(manifest->description), buffer, "\"description\"");
    pm_parse_json_string(manifest->author, sizeof(manifest->author), buffer, "\"author\"");

    // Buscar "size_bytes": número (parseo sencillo sin sscanf)
    p = strstr(buffer, "\"size_bytes\"");
    if(p) {
        char *colon = strchr(p, ':');
        if(colon) {
            colon++; // avanzar después de ':'
            while(*colon == ' ' || *colon == '\t') colon++;
            manifest->size_bytes = (uint32_t)atoi(colon);
        }
    }
    }
    
    // Llenar defaults si no se encontraron todos
    if(!manifest->package_id[0]) {
        const char *last_slash = strrchr(package_path, '/');
        if(last_slash) {
            strncpy(manifest->package_id, last_slash + 1,
                    sizeof(manifest->package_id) - 1);
            manifest->package_id[sizeof(manifest->package_id) - 1] = '\0';
        }
    }
    if(!manifest->name[0]) {
        strncpy(manifest->name, manifest->package_id,
                sizeof(manifest->name) - 1);
        manifest->name[sizeof(manifest->name) - 1] = '\0';
    }
    if(!manifest->version[0]) {
        strncpy(manifest->version, "1.0.0", sizeof(manifest->version) - 1);
        manifest->version[sizeof(manifest->version) - 1] = '\0';
    }
    if(manifest->size_bytes == 0) {
        manifest->size_bytes = 1024 * 1024;
    }
    
    manifest->type = PKG_TYPE_APPLICATION;
    manifest->min_memory_kb = 4096;
    manifest->min_disk_kb = 512;
    
    syscall_close(fd);
    return 0;
}

// Guardar lista de paquetes a persistencia
static int pm_save_package_list(PackageManager *pm)
{
    if(!pm) return -1;
    
    char db_path[512];
    sprintf(db_path, "%s/packages.db", pm->install_base_path);
    
    // Crear/sobrescribir archivo: flags O_WRONLY|O_CREAT|O_TRUNC = 0x41
    int fd = syscall_open(db_path, 0x41);
    if(fd < 0) return -1;
    
    // Escribir encabezado
    uint32_t header[2] = { pm->package_count, pm->total_disk_used };
    if(syscall_write(fd, header, sizeof(header)) != sizeof(header)) {
        syscall_close(fd);
        return -1;
    }
    
    // Escribir cada paquete
    for(uint32_t i = 0; i < pm->package_count; i++) {
        InstalledPackage *pkg = &pm->packages[i];
        
        struct {
            char package_id[64];
            char name[128];
            char version[16];
            uint32_t install_size;
            uint32_t install_date;
            uint8_t enabled;
        } stored;
        
        strcpy(stored.package_id, pkg->manifest.package_id);
        strcpy(stored.name, pkg->manifest.name);
        strcpy(stored.version, pkg->manifest.version);
        stored.install_size = pkg->install_size;
        stored.install_date = pkg->manifest.install_date;
        stored.enabled = pkg->enabled;
        
        if(syscall_write(fd, &stored, sizeof(stored)) != sizeof(stored)) {
            syscall_close(fd);
            return -1;
        }
    }
    
    syscall_close(fd);
    return 0;
}

// Verificar si paquete ya está instalado
static int pm_already_installed(PackageManager *pm, const char *package_id)
{
    if(!pm || !package_id) return 0;
    return (pm_find_by_id(pm, package_id) != NULL);
}

// Verificar dependencias inversas
static int pm_check_reverse_dependencies(PackageManager *pm, const char *package_id)
{
    if(!pm || !package_id) return 1;  // No reverse deps = OK
    
    // Buscar paquetes que dependen de este
    for(uint32_t i = 0; i < pm->package_count; i++) {
        InstalledPackage *pkg = &pm->packages[i];
        if(!pkg->manifest.dependencies) continue;
        
        for(int j = 0; j < pkg->manifest.dependency_count; j++) {
            if(strcmp(pkg->manifest.dependencies[j].name, package_id) == 0) {
                // Alguien depende de esto
                return 0;  // No se puede desinstalar
            }
        }
    }
    
    return 1;  // OK desinstalar
}

// Eliminar archivo o directorio recursivamente
static int pm_recursive_delete(const char *path)
{
    if(!path) return -1;
    
    // Por ahora, intentar eliminar como archivo
    int result = syscall_unlink(path);
    if(result == 0) return 0;
    
    // Si no funciona, intentar como directorio
    result = syscall_rmdir(path);
    return result;
}

// Extraer paquete TAR simplificado
static int pm_extract_simple(const char *package_file, const char *destination)
{
    if(!package_file || !destination) return -1;
    
    // Crear directorio destino
    syscall_mkdir(destination, 0755);
    
    // Abrir archivo TAR
    int src_fd = syscall_open(package_file, 0);  // O_RDONLY
    if(src_fd < 0) return -1;
    
    // Formato TAR ultra simplificado:
    // [size:4 bytes][name_len:4 bytes][name][data]
    while(1) {
        uint32_t file_size;
        
        // Leer tamaño
        if(syscall_read(src_fd, &file_size, sizeof(file_size)) != sizeof(file_size)) {
            break;  // Fin de archivo
        }
        
        if(file_size == 0) break;  // Marcador de fin
        
        // Leer longitud de nombre
        uint32_t name_len;
        if(syscall_read(src_fd, &name_len, sizeof(name_len)) != sizeof(name_len)) {
            break;
        }
        
        if(name_len > 255) break;  // Nombre muy largo
        
        // Leer nombre
        char filename[256];
        if(syscall_read(src_fd, filename, name_len) != (long)name_len) {
            break;
        }
        filename[name_len] = 0;
        
        // Construir ruta completa
        char full_path[512];
        sprintf(full_path, "%s/%s", destination, filename);
        
        // Crear archivo destino
        int dst_fd = syscall_open(full_path, 0x41);  // O_WRONLY|O_CREAT|O_TRUNC
        if(dst_fd < 0) {
            syscall_close(src_fd);
            return -1;
        }
        
        // Copiar datos
        char buffer[4096];
        uint32_t remaining = file_size;
        while(remaining > 0) {
            uint32_t to_read = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            long bytes = syscall_read(src_fd, buffer, to_read);
            if(bytes <= 0) break;
            
            if(syscall_write(dst_fd, buffer, bytes) != bytes) {
                syscall_close(dst_fd);
                syscall_close(src_fd);
                return -1;
            }
            
            remaining -= bytes;
        }
        
        syscall_close(dst_fd);
    }
    
    syscall_close(src_fd);
    return 0;
}

// Comprimir directorio en TAR
static int pm_compress_simple(const char *source_dir, const char *output_file)
{
    if(!source_dir || !output_file) return -1;
    
    // Abrir archivo TAR para escritura
    int tar_fd = syscall_open(output_file, 0x41);  // O_WRONLY|O_CREAT|O_TRUNC
    if(tar_fd < 0) return -1;
    
    // Crear directorio como archivo TAR simplificado
    // Para cada archivo en source_dir: [size][name_len][name][data]
    
    // Por ahora: versión simplificada que solo empaqueta
    // Los archivos necesitarían enumerarse recursivamente (syscall_opendir/readdir)
    
    // Escribir marcador de fin: tamaño 0
    uint32_t zero = 0;
    syscall_write(tar_fd, &zero, sizeof(zero));
    
    syscall_close(tar_fd);
    return 0;
}

// Buscar paquete por tipo
static InstalledPackage** pm_find_by_type_impl(PackageManager *pm, PackageType type, int *count)
{
    if(!pm || !count) return NULL;
    
    *count = 0;
    
    // Contar paquetes del tipo
    for(uint32_t i = 0; i < pm->package_count; i++) {
        if(pm->packages[i].manifest.type == type) {
            (*count)++;
        }
    }
    
    if(*count == 0) return NULL;
    
    // Asignar array
    InstalledPackage **result = (InstalledPackage**)malloc(
        sizeof(InstalledPackage*) * (*count));
    if(!result) return NULL;
    
    // Llenar array
    int idx = 0;
    for(uint32_t i = 0; i < pm->package_count; i++) {
        if(pm->packages[i].manifest.type == type) {
            result[idx++] = &pm->packages[i];
        }
    }
    
    return result;
}

// ─── Crear manager ──────────────────────────────────────────────────

PackageManager* pm_create(void)
{
    PackageManager *pm = (PackageManager*)malloc(sizeof(PackageManager));
    if(!pm) return 0;
    
    pm->package_count = 0;
    pm->package_capacity = 128;  // Soportar hasta 128 paquetes
    pm->total_disk_used = 0;
    pm->total_memory_limit = 104857600;  // 100 MB por defecto
    
    strcpy(pm->app_store_path, "/apps/store");
    strcpy(pm->install_base_path, "/apps/installed");
    
    pm->packages = (InstalledPackage*)malloc(sizeof(InstalledPackage) * pm->package_capacity);
    if(!pm->packages) {
        free(pm);
        return 0;
    }
    
    // Cargar paquetes instalados
    pm_load_installed_packages(pm);
    
    return pm;
}

// ─── Destruir manager ────────────────────────────────────────────────

void pm_destroy(PackageManager *pm)
{
    if(!pm) return;
    
    if(pm->packages) {
        // Liberar dependencias de cada paquete
        for(uint32_t i = 0; i < pm->package_count; i++) {
            if(pm->packages[i].manifest.dependencies) {
                free(pm->packages[i].manifest.dependencies);
            }

            // `supported_extensions` es un array fijo de punteros; liberar
            // cada string individual si fue asignada dinámicamente.
            for(int j = 0; j < pm->packages[i].manifest.extension_count; j++) {
                if(pm->packages[i].manifest.supported_extensions[j]) {
                    free(pm->packages[i].manifest.supported_extensions[j]);
                    pm->packages[i].manifest.supported_extensions[j] = NULL;
                }
            }
        }
        free(pm->packages);
    }
    
    free(pm);
}

// ─── Instalar paquete ───────────────────────────────────────────────

int pm_install(PackageManager *pm, const char *package_path)
{
    if(!pm || !package_path) return -1;
    
    // 1. Extraer manifest.json
    PackageManifest manifest;
    if(pm_read_manifest(package_path, &manifest) != 0) {
        return -1;  // Failed to read manifest
    }
    
    // 2. Verificar que no está instalado
    if(pm_already_installed(pm, manifest.package_id)) {
        return -1;  // Already installed
    }
    
    // 3. Verificar dependencias
    if(!pm_check_dependencies(pm, &manifest)) {
        return -1;  // Missing dependencies
    }
    
    // 4. Verificar espacio disco
    if(!pm_check_space(pm, manifest.size_bytes / 1024)) {
        return -1;  // Not enough space
    }
    
    // 5. Extraer archivos
    char dest_path[512];
    sprintf(dest_path, "%s/%s", pm->install_base_path, manifest.package_id);
    if(pm_extract_simple(package_path, dest_path) != 0) {
        return -1;  // Extraction failed
    }
    
    // 6. Agregar a lista de paquetes instalados
    if(pm->package_count >= pm->package_capacity) {
        // Expandir array
        pm->package_capacity *= 2;
        InstalledPackage *new_packages = (InstalledPackage*)malloc(
            sizeof(InstalledPackage) * pm->package_capacity);
        if(!new_packages) return -1;
        
        memcpy(new_packages, pm->packages, 
               sizeof(InstalledPackage) * pm->package_count);
        free(pm->packages);
        pm->packages = new_packages;
    }
    
    // Copiar manifest
    InstalledPackage *pkg = &pm->packages[pm->package_count];
    memcpy(&pkg->manifest, &manifest, sizeof(PackageManifest));
    strcpy(pkg->install_path, dest_path);
    pkg->state = PKG_STATE_INSTALLED;
    pkg->enabled = 1;
    pkg->install_size = manifest.size_bytes;
    
    pm->package_count++;
    pm->total_disk_used += manifest.size_bytes;
    
    // 7. Guardar estado
    pm_save_package_list(pm);
    
    return 0;  // Success
}

// ─── Desinstalar paquete ────────────────────────────────────────────

int pm_uninstall(PackageManager *pm, const char *package_id)
{
    if(!pm || !package_id) return -1;
    
    // 1. Encontrar paquete
    InstalledPackage *pkg = pm_find_by_id(pm, package_id);
    if(!pkg) return -1;  // Not found
    
    // 2. Verificar si es requerido
    if(pkg->manifest.is_required) {
        return -1;  // Cannot uninstall
    }
    
    // 3. Verificar dependencias inversa (qué depende de esto)
    if(!pm_check_reverse_dependencies(pm, package_id)) {
        return -1;  // Other packages depend on this
    }
    
    // 4. Eliminar archivos
    pm_recursive_delete(pkg->install_path);
    
    // 5. Actualizar lista
    uint32_t disk_freed = pkg->install_size;
    
    // Mover último elemento al lugar del eliminado
    uint32_t idx = pkg - pm->packages;
    if(idx < pm->package_count - 1) {
        memcpy(pkg, &pm->packages[pm->package_count - 1], 
               sizeof(InstalledPackage));
    }
    pm->package_count--;
    pm->total_disk_used -= disk_freed;
    
    // 6. Guardar estado
    pm_save_package_list(pm);
    
    return 0;  // Success
}

// ─── Actualizar paquete ─────────────────────────────────────────────

int pm_update(PackageManager *pm, const char *package_id)
{
    if(!pm || !package_id) return -1;
    
    // 1. Encontrar paquete
    InstalledPackage *pkg = pm_find_by_id(pm, package_id);
    if(!pkg) return -1;
    
    // 2-6. Por ahora: stub simplificado
    // En el futuro: implementar descarga de servidor de actualizaciones
    // - Conexión a update server
    // - Descarga de nueva versión
    // - Instalación actualización
    // - Backup de datos usuario
    // - Limpieza de versión antigua
    
    pkg->manifest.update_date = scheduler_get_ticks() / 18;  // Timestamp actual
    pm_save_package_list(pm);
    
    return 0;
}

// ─── Buscar por ID ──────────────────────────────────────────────────

InstalledPackage* pm_find_by_id(PackageManager *pm, const char *package_id)
{
    if(!pm || !package_id) return 0;
    
    for(uint32_t i = 0; i < pm->package_count; i++) {
        if(strcmp(pm->packages[i].manifest.package_id, package_id) == 0) {
            return &pm->packages[i];
        }
    }
    
    return 0;
}

// ─── Buscar por extensión ───────────────────────────────────────────

InstalledPackage* pm_find_by_extension(PackageManager *pm, const char *ext)
{
    if(!pm || !ext) return 0;
    
    for(uint32_t i = 0; i < pm->package_count; i++) {
        InstalledPackage *pkg = &pm->packages[i];
        
        for(int j = 0; j < pkg->manifest.extension_count; j++) {
            if(strcmp(pkg->manifest.supported_extensions[j], ext) == 0) {
                return pkg;
            }
        }
    }
    
    return 0;
}

// ─── Verificar dependencias ──────────────────────────────────────────

int pm_check_dependencies(PackageManager *pm, const PackageManifest *manifest)
{
    if(!pm || !manifest) return 0;
    if(manifest->dependency_count <= 0) return 1;
    if(!manifest->dependencies) return 0;
    
    // Verificar cada dependencia declarada en el manifest
    for(int i = 0; i < manifest->dependency_count; i++) {
        Dependency *dep = &manifest->dependencies[i];
        if(!dep->name[0]) {
            return 0;
        }
        if(!pm_find_by_id(pm, dep->name)) {
            // Dependencia no encontrada
            return 0;
        }
    }
    
    return 1;  // Todas las dependencias OK
}

// ─── Verificar espacio ──────────────────────────────────────────────

int pm_check_space(PackageManager *pm, uint32_t required_kb)
{
    if(!pm) return 0;
    
    // Obtener espacio disponible en disco usando syscall
    uint64_t free_disk_kb = syscall_get_free_disk() / 1024;
    
    // Necesitamos espacio para el paquete + margen de seguridad (10%)
    uint32_t required_with_margin = required_kb + (required_kb / 10);
    
    return (free_disk_kb > required_with_margin) ? 1 : 0;
}

// ─── Verificar memoria ──────────────────────────────────────────────

int pm_check_memory(PackageManager *pm, uint32_t required_kb)
{
    if(!pm) return 0;
    
    // Obtener memoria disponible usando syscall
    uint32_t free_memory_kb = syscall_get_free_memory();
    
    // Verificar si hay suficiente memoria libre
    // Considerar que ejecutar la app también consume memoria
    uint32_t required_with_margin = required_kb + (required_kb / 5);  // 20% margen
    
    return (free_memory_kb > required_with_margin) ? 1 : 0;
}

// ─── Listar paquetes instalados ─────────────────────────────────────

void pm_list_installed(PackageManager *pm)
{
    if(!pm) return;
    
    printf("=== Installed Packages ===\n");
    printf("%-30s %-12s %-50s\n", "Package", "Version", "Description");
    printf("─────────────────────────────────────────────────────────────────────────────────\n");
    
    for(uint32_t i = 0; i < pm->package_count; i++) {
        InstalledPackage *pkg = &pm->packages[i];
        printf("%-30s %-12s %-50s\n",
               pkg->manifest.name,
               pkg->manifest.version,
               pkg->manifest.description);
    }
    
    printf("\nTotal: %u packages, %u KB used\n", 
           pm->package_count, pm->total_disk_used / 1024);
}

// Stubs para funciones de archivo
int pm_extract_package(const char *package_file, const char *destination) {
    // Implementar descompresión de ZIP/TAR
    return pm_extract_simple(package_file, destination);
}

int pm_compress_package(const char *source_dir, const char *output_file) {
    // Implementar compresión ZIP/TAR
    return pm_compress_simple(source_dir, output_file);
}

int pm_enable_package(PackageManager *pm, const char *package_id) {
    InstalledPackage *pkg = pm_find_by_id(pm, package_id);
    if(pkg) pkg->enabled = 1;
    return (pkg != 0) ? 0 : -1;
}

int pm_disable_package(PackageManager *pm, const char *package_id) {
    InstalledPackage *pkg = pm_find_by_id(pm, package_id);
    if(pkg) pkg->enabled = 0;
    return (pkg != 0) ? 0 : -1;
}

void pm_list_updateable(PackageManager *pm) {
    if(!pm) return;
    
    printf("=== Available Updates ===\n");
    printf("%-30s %-12s %-12s\n", "Package", "Current", "Available");
    printf("───────────────────────────────────────────────────────────\n");
    
    // Conectar con servidor de actualizaciones en /apps/updates/index.db
    int fd = syscall_open("/apps/updates/index.db", 0);
    if(fd < 0) {
        printf("No update server available\n");
        return;
    }
    
    // Leer índice de actualizaciones (formato simplificado)
    // [num_updates:4][update entries...]
    // where each entry: [package_id:64][available_version:16]
    uint32_t num_updates = 0;
    if(syscall_read(fd, &num_updates, sizeof(num_updates)) != sizeof(num_updates)) {
        syscall_close(fd);
        return;
    }
    
    for(uint32_t i = 0; i < num_updates && i < 100; i++) {
        struct {
            char package_id[64];
            char available_version[16];
        } update_entry;
        
        if(syscall_read(fd, &update_entry, sizeof(update_entry)) != sizeof(update_entry)) {
            break;
        }
        
        // Buscar paquete local
        InstalledPackage *local_pkg = pm_find_by_id(pm, update_entry.package_id);
        if(!local_pkg) continue;
        
        // Comparar versiones (simple: string comparison)
        if(strcmp(local_pkg->manifest.version, update_entry.available_version) < 0) {
            printf("%-30s %-12s %-12s\n",
                   local_pkg->manifest.name,
                   local_pkg->manifest.version,
                   update_entry.available_version);
        }
    }
    
    syscall_close(fd);
}

InstalledPackage** pm_find_by_type(PackageManager *pm, PackageType type) {
    // Implementar búsqueda por tipo
    int count = 0;
    return pm_find_by_type_impl(pm, type, &count);
}

int pm_get_info(PackageManager *pm, const char *package_id, PackageManifest *manifest) {
    InstalledPackage *pkg = pm_find_by_id(pm, package_id);
    if(!pkg || !manifest) return -1;
    memcpy(manifest, &pkg->manifest, sizeof(PackageManifest));
    return 0;
}
