// kernel/fs/vfs.c
#include <stdint.h>
#include <stddef.h>
#include "../string.h"

#define MAX_MOUNTS 16
#define MAX_NAME   64
#define MAX_DIRS   32
#define MAX_FILES_PER_DIR 256

typedef struct inode inode_t;
typedef struct file file_t;
typedef struct fs_ops fs_ops_t;

struct inode {
    uint64_t ino;
    uint32_t mode;
    uint64_t size;
    fs_ops_t *ops;
    void *fs_private;
};

struct file {
    inode_t *inode;
    uint64_t pos;
};

struct fs_ops {
    inode_t *(*lookup)(inode_t *, const char *);
    int (*read)(file_t *, void *, uint64_t);
    int (*write)(file_t *, const void *, uint64_t);
};

typedef struct mount {
    char name[MAX_NAME];
    inode_t *root;
} mount_t;

// Directory entry structure (en memoria)
typedef struct {
    char name[256];
    uint32_t inode_num;
    uint32_t size;
    uint8_t type;  // 0=archivo, 1=directorio
    uint32_t timestamp;
} dir_entry_t;

// Simple in-memory directory structure
typedef struct {
    char path[512];
    dir_entry_t entries[MAX_FILES_PER_DIR];
    int entry_count;
} directory_t;

static mount_t mounts[MAX_MOUNTS];
static int mount_count = 0;

static directory_t directories[MAX_DIRS];
static int directory_count = 0;
static uint32_t next_inode = 1;

// Helpers
static int _strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int _strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void _strcpy(char *d, const char *s) {
    while ((*d++ = *s++));
}

int vfs_mount(const char *name, inode_t *root)
{
    if (mount_count >= MAX_MOUNTS) return -1;
    mount_t *m = &mounts[mount_count++];
    for (int i = 0; i < MAX_NAME && name[i]; i++)
        m->name[i] = name[i];
    m->root = root;
    return 0;
}

inode_t *vfs_lookup(const char *path)
{
    if (path[0] != '/') return NULL;
    for (int i = 0; i < mount_count; i++) {
        if (path[1] == mounts[i].name[0])
            return mounts[i].root;
    }
    return NULL;
}

// Buscar directorio por path
static directory_t *_find_dir(const char *path) {
    for (int i = 0; i < directory_count; i++) {
        if (_strcmp(directories[i].path, path) == 0) {
            return &directories[i];
        }
    }
    return NULL;
}

// Crear directorio
static directory_t *_create_dir(const char *path) {
    if (directory_count >= MAX_DIRS) return NULL;
    
    directory_t *d = &directories[directory_count++];
    _strcpy(d->path, path);
    d->entry_count = 0;
    return d;
}

int vfs_readdir(const char *path, void *entries, int max_entries)
{
    if (!path || !entries || max_entries <= 0) return -1;
    
    directory_t *dir = _find_dir(path);
    if (!dir) return 0;  // Directorio vacío o no existe
    
    // Copiar entries
    int count = (dir->entry_count < max_entries) ? dir->entry_count : max_entries;
    for (int i = 0; i < count; i++) {
        ((dir_entry_t *)entries)[i] = dir->entries[i];
    }
    
    return count;
}

int vfs_stat(const char *path, void *stat_buf)
{
    if (!path || !stat_buf) return -1;
    
    // Por ahora, return info básica
    uint32_t *stat_info = (uint32_t *)stat_buf;
    stat_info[0] = next_inode++;  // inode
    stat_info[1] = 0;             // size (simplificado)
    
    return 0;
}

int vfs_mkdir(const char *path)
{
    if (!path) return -1;
    
    directory_t *existing = _find_dir(path);
    if (existing) return -1;  // Ya existe
    
    directory_t *new_dir = _create_dir(path);
    if (!new_dir) return -1;  // No se pudo crear
    
    return 0;
}

int vfs_write_file(const char *path, const void *data, uint64_t size)
{
    if (!path || !data || size == 0) return -1;
    
    // Extraer directorio padre
    char parent[512] = "/";
    int last_slash = 0;
    for (int i = 0; path[i]; i++) {
        if (path[i] == '/') last_slash = i;
    }
    
    // Obtener nombre de archivo
    const char *filename = &path[last_slash + 1];
    if (_strlen(filename) == 0) return -1;
    
    // Crear directorio padre si no existe
    if (last_slash > 0) {
        char parent_path[512];
        for (int i = 0; i < last_slash; i++) {
            parent_path[i] = path[i];
        }
        parent_path[last_slash] = '\0';
        
        directory_t *parent_dir = _find_dir(parent_path);
        if (!parent_dir) {
            parent_dir = _create_dir(parent_path);
            if (!parent_dir) return -1;
        }
    } else {
        // archivo en root
        directory_t *root = _find_dir("/");
        if (!root) {
            root = _create_dir("/");
            if (!root) return -1;
        }
    }
    
    return (int)size;  // Simplified: return bytes written
}

void vfs_init(void)
{
    mount_count = 0;
    directory_count = 0;
    next_inode = 1;
    
    // Crear directorio root
    _create_dir("/");
}
