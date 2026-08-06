// kernel/fs/vfs.h
// Virtual File System APIs

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

// Forward declarations
typedef struct inode inode_t;
typedef struct file file_t;
typedef struct fs_ops fs_ops_t;

// Inode structure (file/directory metadata)
struct inode {
    uint64_t ino;           // Inode number
    uint32_t mode;          // File mode/permissions
    uint64_t size;          // File size
    fs_ops_t *ops;          // Filesystem operations
    void *fs_private;       // Private FS data
};

// File structure (open file descriptor)
struct file {
    inode_t *inode;         // Associated inode
    uint64_t pos;           // Current file position
};

// Filesystem operations
struct fs_ops {
    inode_t *(*lookup)(inode_t *, const char *);      // Lookup inode
    int (*read)(file_t *, void *, uint64_t);          // Read file
    int (*write)(file_t *, const void *, uint64_t);   // Write file
};

// Mount point structure
typedef struct {
    char name[64];          // Mount name
    inode_t *root;          // Root inode
} mount_t;

// VFS API
int vfs_mount(const char *name, inode_t *root);
inode_t *vfs_lookup(const char *path);
int vfs_readdir(const char *path, void *entries, int max_entries);
int vfs_stat(const char *path, void *stat);
int vfs_mkdir(const char *path);
int vfs_write_file(const char *path, const void *data, uint64_t size);

#endif // VFS_H
