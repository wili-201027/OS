// kernel/syscalls/fs_syscalls.c
// Implementación de syscalls para filesystem

#include "fs_syscalls.h"
#include "../fs/vfs.h"
#include "../string.h"

// ─── Variables globales ─────────────────────────────────────────────────────
static char current_working_directory[512] = "/";

// ─── sys_listdir: Listar contenido de directorio ──────────────────────────

int sys_listdir(const char *path, DirEntry *entries, uint32_t max_count)
{
    if(!path || !entries || max_count == 0) {
        return -1;  // EINVAL
    }
    
    // Usar ruta actual si es relativa
    const char *real_path = path;
    char absolute_path[512];
    if(path[0] != '/') {
        strcpy(absolute_path, current_working_directory);
        if(absolute_path[strlen(absolute_path) - 1] != '/') {
            strcat(absolute_path, "/");
        }
        strcat(absolute_path, path);
        real_path = absolute_path;
    }
    
    // Llamar a VFS para leer directorio
    return vfs_readdir(real_path, entries, max_count);
}

// ─── sys_stat: Obtener información de archivo/directorio ───────────────────

int sys_stat(const char *path, FileStat *stat)
{
    if(!path || !stat) {
        return -1;  // EINVAL
    }
    
    // Usar ruta actual si es relativa
    const char *real_path = path;
    char absolute_path[512];
    if(path[0] != '/') {
        strcpy(absolute_path, current_working_directory);
        if(absolute_path[strlen(absolute_path) - 1] != '/') {
            strcat(absolute_path, "/");
        }
        strcat(absolute_path, path);
        real_path = absolute_path;
    }
    
    // Llenar estructura básica
    stat->inode = 1;     // Simplificado
    stat->size = 0;      // Por ahora
    stat->created = 0;   // Timestamp UNIX
    stat->modified = 0;
    stat->permissions = 0755;  // rwxr-xr-x
    stat->type = 1;      // 1=directorio
    
    return vfs_stat(real_path, (void *)stat);
}

// ─── sys_mkdir: Crear directorio ───────────────────────────────────────────

int sys_mkdir(const char *path)
{
    if(!path) {
        return -1;  // EINVAL
    }
    
    // Usar ruta actual si es relativa
    const char *real_path = path;
    char absolute_path[512];
    if(path[0] != '/') {
        strcpy(absolute_path, current_working_directory);
        if(absolute_path[strlen(absolute_path) - 1] != '/') {
            strcat(absolute_path, "/");
        }
        strcat(absolute_path, path);
        real_path = absolute_path;
    }
    
    return vfs_mkdir(real_path);
}

// ─── sys_chdir: Cambiar directorio ────────────────────────────────────────

int sys_chdir(const char *path)
{
    if(!path) {
        return -1;  // EINVAL
    }
    
    // Validar que es directorio existente (simplificado)
    FileStat stat;
    if(sys_stat(path, &stat) != 0) {
        return -1;  // No existe
    }
    
    if(stat.type != 1) {
        return -1;  // No es directorio
    }
    
    // Copiar a current_working_directory
    strncpy(current_working_directory, path, sizeof(current_working_directory) - 1);
    current_working_directory[sizeof(current_working_directory) - 1] = 0;
    
    return 0;
}

// ─── sys_getcwd: Obtener directorio actual ──────────────────────────────────

int sys_getcwd(char *buf, uint32_t size)
{
    if(!buf || size == 0) {
        return -1;  // EINVAL
    }
    
    int len = strlen(current_working_directory);
    if(len + 1 > (int)size) {
        return -1;  // ENAMETOOLONG
    }
    
    strcpy(buf, current_working_directory);
    return len;
}

// ─── Dispatcher de syscalls ─────────────────────────────────────────────────

long dispatch_fs_syscall(uint32_t syscall_num, uint64_t arg1, uint64_t arg2,
                         uint64_t arg3, uint64_t arg4 __attribute__((unused)))
{
    switch(syscall_num) {
        case SYSCALL_FS_LISTDIR:
            return sys_listdir((const char *)arg1, (DirEntry *)arg2, (uint32_t)arg3);
        
        case SYSCALL_FS_STAT:
            return sys_stat((const char *)arg1, (FileStat *)arg2);
        
        case SYSCALL_FS_MKDIR:
            return sys_mkdir((const char *)arg1);
        
        case SYSCALL_FS_CHDIR:
            return sys_chdir((const char *)arg1);
        
        case SYSCALL_FS_GETCWD:
            return sys_getcwd((char *)arg1, (uint32_t)arg2);
        
        default:
            return -1;  // ENOSYS
    }
}
