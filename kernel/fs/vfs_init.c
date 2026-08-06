// kernel/fs/vfs_init.c
// VFS initialization and mount setup

#include "vfs.h"

// Declare VFS init function from vfs.c
extern void vfs_init(void);

// Initialize VFS subsystem
void fs_init(void)
{
    // Initialize the VFS core
    vfs_init();
    
    // Create standard directories
    vfs_mkdir("/dev");
    vfs_mkdir("/proc");
    vfs_mkdir("/tmp");
    vfs_mkdir("/home");
    vfs_mkdir("/var");
    vfs_mkdir("/var/log");
    vfs_mkdir("/usr");
    vfs_mkdir("/usr/bin");
    vfs_mkdir("/usr/lib");
    vfs_mkdir("/etc");
    vfs_mkdir("/boot");
}
