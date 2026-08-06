// userland/libc/syscall.c
// Complete syscall wrapper implementations
#include <stdint.h>

// Syscall assembly defined in arch-specific code
extern uint64_t syscall0(uint64_t n);
extern uint64_t syscall1(uint64_t n, uint64_t arg1);
extern uint64_t syscall2(uint64_t n, uint64_t arg1, uint64_t arg2);
extern uint64_t syscall3(uint64_t n, uint64_t arg1, uint64_t arg2, uint64_t arg3);
extern uint64_t syscall4(uint64_t n, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);

// Syscall numbers
#define SYS_YIELD     1
#define SYS_FB_WIDTH  5
#define SYS_FB_HEIGHT 6
#define SYS_FB_ADDR   7
#define SYS_SLEEP     10

// Filesystem syscalls (100-112)
#define SYS_FS_OPEN       100
#define SYS_FS_CLOSE      101
#define SYS_FS_READ       102
#define SYS_FS_WRITE      103
#define SYS_FS_SEEK       104
#define SYS_FS_LISTDIR    105
#define SYS_FS_STAT       106
#define SYS_FS_MKDIR      107
#define SYS_FS_RMDIR      108
#define SYS_FS_DELETE     109
#define SYS_FS_RENAME     110
#define SYS_FS_GETCWD     111
#define SYS_FS_CHDIR      112

// IPC syscalls (200+)
#define SYS_IPC_OPEN      200
#define SYS_IPC_SEND      201
#define SYS_IPC_RECV      202
#define SYS_IPC_CLOSE     203

// ─── Framebuffer operations ────────────────────────────────────────────────

void sys_yield() { 
    syscall0(SYS_YIELD); 
}

void sys_sleep_ms(uint64_t ms) { 
    syscall1(SYS_SLEEP, ms); 
}

uint32_t fb_get_width() { 
    return (uint32_t)syscall0(SYS_FB_WIDTH); 
}

uint32_t fb_get_height() { 
    return (uint32_t)syscall0(SYS_FB_HEIGHT); 
}

uint64_t fb_get_addr() { 
    return syscall0(SYS_FB_ADDR); 
}

void fb_init(void* a, uint32_t w, uint32_t h) { 
    // Framebuffer base address and dimensions already obtained via syscalls
    (void)a; 
    (void)w; 
    (void)h;
}

// ─── Filesystem operations ────────────────────────────────────────────────

int sys_listdir(const char *path, void *entries, uint32_t max_count) {
    return (int)syscall3(SYS_FS_LISTDIR, (uint64_t)path, (uint64_t)entries, (uint64_t)max_count);
}

int sys_stat(const char *path, void *stat) {
    return (int)syscall2(SYS_FS_STAT, (uint64_t)path, (uint64_t)stat);
}

int sys_mkdir(const char *path) {
    return (int)syscall1(SYS_FS_MKDIR, (uint64_t)path);
}

int sys_chdir(const char *path) {
    return (int)syscall1(SYS_FS_CHDIR, (uint64_t)path);
}

int sys_getcwd(char *buf, uint32_t size) {
    return (int)syscall2(SYS_FS_GETCWD, (uint64_t)buf, (uint64_t)size);
}

// ─── IPC operations ──────────────────────────────────────────────────────

int sys_ipc_open(const char *service, const char *role) { 
    return (int)syscall2(SYS_IPC_OPEN, (uint64_t)service, (uint64_t)role);
}

int sys_ipc_send(int handle, const void *msg, uint32_t size) {
    return (int)syscall3(SYS_IPC_SEND, (uint64_t)handle, (uint64_t)msg, (uint64_t)size);
}

int sys_ipc_recv(int handle, void *msg, uint32_t max_size) {
    return (int)syscall3(SYS_IPC_RECV, (uint64_t)handle, (uint64_t)msg, (uint64_t)max_size);
}

int sys_ipc_close(int handle) {
    return (int)syscall1(SYS_IPC_CLOSE, (uint64_t)handle);
}

// ─── Rendering ───────────────────────────────────────────────────────────

void render_frame() { 
    sys_yield();  // Yield after rendering to allow other processes
}

void sys_read_keyboard(void* buf) { 
    if (buf) syscall1(100, (uint64_t)buf);  // Keyboard input (future)
}

void sys_read_mouse(void* buf) { 
    if (buf) syscall1(101, (uint64_t)buf);  // Mouse input (future)
}

// ─── Port-based IPC (deprecated, kept for compatibility) ──────────────────

void port_recv(void* a, void* b) { 
    if (a && b) syscall2(202, (uint64_t)a, (uint64_t)b);
}

void port_send(void* a, void* b) { 
    if (a && b) syscall2(201, (uint64_t)a, (uint64_t)b);
}
