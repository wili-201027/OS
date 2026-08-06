// kernel/syscalls/fs_syscalls.h
// Syscalls para manejo del sistema de archivos

#ifndef FS_SYSCALLS_H
#define FS_SYSCALLS_H

#include <stdint.h>

// ─── Códigos de syscall para filesystem ────────────────────────────────────
#define SYSCALL_FS_OPEN       100
#define SYSCALL_FS_CLOSE      101
#define SYSCALL_FS_READ       102
#define SYSCALL_FS_WRITE      103
#define SYSCALL_FS_SEEK       104
#define SYSCALL_FS_LISTDIR    105
#define SYSCALL_FS_STAT       106
#define SYSCALL_FS_MKDIR      107
#define SYSCALL_FS_RMDIR      108
#define SYSCALL_FS_DELETE     109
#define SYSCALL_FS_RENAME     110
#define SYSCALL_FS_GETCWD     111
#define SYSCALL_FS_CHDIR      112

// ─── Estructura de información de archivo ──────────────────────────────────
typedef struct {
    uint32_t inode;          // Número de inodo
    uint32_t size;           // Tamaño en bytes
    uint32_t created;        // Timestamp creación (UNIX)
    uint32_t modified;       // Timestamp modificación
    uint16_t permissions;    // rwxrwxrwx
    uint8_t  type;           // 0=archivo, 1=directorio, 2=symlink
    uint8_t  padding;        // Alineación
} FileStat;

// ─── Estructura de entrada de directorio ──────────────────────────────────
typedef struct {
    char     name[256];
    uint32_t inode;
    uint8_t  type;           // 0=archivo, 1=directorio
    uint8_t  padding[3];
} DirEntry;

// ─── Syscalls para filesystem ──────────────────────────────────────────────

// int sys_open(const char *path, int flags);
// Retorna: file descriptor o -1 en error

// int sys_close(int fd);
// Retorna: 0 en éxito o -1 en error

// int sys_read(int fd, void *buf, uint32_t count);
// Retorna: bytes leídos o -1 en error

// int sys_write(int fd, const void *buf, uint32_t count);
// Retorna: bytes escritos o -1 en error

// int sys_listdir(const char *path, DirEntry *entries, uint32_t max_count);
// Retorna: cantidad de entradas o -1 en error

// int sys_stat(const char *path, FileStat *stat);
// Retorna: 0 en éxito o -1 en error

// int sys_mkdir(const char *path);
// Retorna: 0 en éxito o -1 en error

// int sys_chdir(const char *path);
// Retorna: 0 en éxito o -1 en error

// int sys_getcwd(char *buf, uint32_t size);
// Retorna: longitud de ruta o -1 en error

#endif // FS_SYSCALLS_H
