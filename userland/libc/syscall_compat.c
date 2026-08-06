// Compatibility wrappers and stubs to satisfy legacy userland symbols
#include <stdint.h>

// Arch syscall stubs
extern uint64_t syscall0(uint64_t n);
extern uint64_t syscall1(uint64_t n, uint64_t a1);
extern uint64_t syscall2(uint64_t n, uint64_t a1, uint64_t a2);
extern uint64_t syscall3(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3);

// Common syscall numbers (mirror kernel handler where available)
#define SYS_GET_TICKS   30
#define SYS_FS_OPEN     100
#define SYS_FS_CLOSE    101
#define SYS_FS_READ     102
#define SYS_FS_WRITE    103
#define SYS_FS_SEEK     104

// Provide scheduler_get_ticks for userland callers (wrapper over syscall)
uint64_t scheduler_get_ticks(void) {
    return syscall0(SYS_GET_TICKS);
}

// Filesystem compatibility wrappers used by older userland code
int syscall_open(const char *path, int flags) {
    return (int)syscall2(SYS_FS_OPEN, (uint64_t)path, (uint64_t)flags);
}

int syscall_close(int fd) {
    return (int)syscall1(SYS_FS_CLOSE, (uint64_t)fd);
}

int syscall_read(int fd, void *buf, uint32_t sz) {
    return (int)syscall3(SYS_FS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)sz);
}

int syscall_write(int fd, const void *buf, uint32_t sz) {
    return (int)syscall3(SYS_FS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)sz);
}

int syscall_mkdir(const char *path) {
    // If kernel supports FS_MKDIR via different number, prefer syscall2 mapping above
    return -1;
}

int syscall_unlink(const char *path) { (void)path; return -1; }
int syscall_rmdir(const char *path) { (void)path; return -1; }

// Directory iteration stubs (placeholder implementations)
int syscall_opendir(const char *path) { (void)path; return -1; }
int syscall_readdir(int dh, void *entry) { (void)dh; (void)entry; return -1; }
int syscall_closedir(int dh) { (void)dh; return -1; }

// Process control stubs
int syscall_fork(void) { return -1; }
int syscall_execve(const char *path, char *const argv[], char *const envp[]) { (void)path; (void)argv; (void)envp; return -1; }

// Resource info stubs
uint64_t syscall_get_free_disk(void) { return 0; }
uint32_t syscall_get_free_memory(void) { return 0; }

// Window/compositor drawing stubs used only to satisfy link-time.
void *wm_create_window(int x, int y, int w, int h, const char *title) { (void)x; (void)y; (void)w; (void)h; (void)title; return 0; }
int wm_write(void *win, const char *s, int len) { (void)win; (void)s; (void)len; return 0; }
void wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color) { (void)win; (void)x; (void)y; (void)w; (void)h; (void)color; }
void wm_clear_window(void *win) { (void)win; }

void draw_string_fb(uint32_t *fb, uint32_t fb_w, uint32_t fb_h, int x, int y, const char *s, uint32_t color) {
    (void)fb; (void)fb_w; (void)fb_h; (void)x; (void)y; (void)s; (void)color;
}
