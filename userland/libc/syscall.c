// userland/libc/syscall.c
#include <stdint.h>
#include "../sysroot/sysroot.h"
#include "../ipc/fast_ipc.h"

// Syscall assembly stubs defined in arch code
extern uint64_t syscall0(uint64_t n);
extern uint64_t syscall1(uint64_t n, uint64_t a1);
extern uint64_t syscall2(uint64_t n, uint64_t a1, uint64_t a2);
extern uint64_t syscall3(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3);
extern uint64_t syscall4(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4);

// Syscall numbers
#define SYS_YIELD         1
#define SYS_FB_WIDTH      5
#define SYS_FB_HEIGHT     6
#define SYS_FB_ADDR       7
#define SYS_SLEEP         10
#define SYS_THREAD_CREATE 11
#define SYS_THREAD_EXIT   12
#define SYS_FS_LISTDIR    105
#define SYS_FS_STAT       106
#define SYS_FS_MKDIR      107
#define SYS_FS_CHDIR      112
#define SYS_FS_GETCWD     111
#define SYS_IPC_OPEN      200
#define SYS_IPC_SEND      201
#define SYS_IPC_RECV      202
#define SYS_IPC_CLOSE     203

static sysroot_layout_t s_sysroot;
static int s_sysroot_ready = 0;

void sys_yield() { syscall0(SYS_YIELD); }
void sys_sleep_ms(uint64_t ms) { syscall1(SYS_SLEEP, ms); }
int sys_thread_create(void *entry, void *arg, void *stack_bottom, uint64_t stack_size) {
    return (int)syscall4(SYS_THREAD_CREATE,
                         (uint64_t)entry,
                         (uint64_t)arg,
                         (uint64_t)stack_bottom,
                         stack_size);
}
void sys_thread_exit(int exit_code) {
    syscall1(SYS_THREAD_EXIT, (uint64_t)exit_code);
}
uint32_t fb_get_width() { return (uint32_t)syscall0(SYS_FB_WIDTH); }
uint32_t fb_get_height() { return (uint32_t)syscall0(SYS_FB_HEIGHT); }
uint64_t fb_get_addr() { return syscall0(SYS_FB_ADDR); }

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

int sys_ipc_open(const char* s, const char* r) { 
    return (int)syscall2(SYS_IPC_OPEN, (uint64_t)s, (uint64_t)r);
}

int sys_ipc_send(int h, const void *m, uint32_t sz) {
    return (int)syscall3(SYS_IPC_SEND, (uint64_t)h, (uint64_t)m, (uint64_t)sz);
}

int sys_ipc_recv(int h, void *m, uint32_t sz) {
    return (int)syscall3(SYS_IPC_RECV, (uint64_t)h, (uint64_t)m, (uint64_t)sz);
}

int sys_ipc_close(int h) {
    return (int)syscall1(SYS_IPC_CLOSE, (uint64_t)h);
}

void fb_init(void* a, uint32_t w, uint32_t h) { (void)a; (void)w; (void)h; }

void sysroot_prepare(void)
{
    if (!s_sysroot_ready) {
        sysroot_init(&s_sysroot);
        s_sysroot_ready = 1;
    }
}

int sysroot_lookup_path(const char *path, char *out, uint32_t out_size)
{
    sysroot_prepare();
    sysroot_entry_t entry;
    if (sysroot_lookup(&s_sysroot, path, &entry) != 0) return -1;
    if (out && out_size > 0) {
        uint32_t len = 0;
        while (len + 1 < out_size && entry.target[len]) { out[len] = entry.target[len]; ++len; }
        out[len] = 0;
    }
    return 0;
}

// ─── Rendering ────────────────────────────────────────────────────────────
void render_frame() { 
    sys_yield();  // Yield after rendering
}

// ─── Input operations ─────────────────────────────────────────────────────
void sys_read_keyboard(void* buf) { 
    if (buf) syscall1(100, (uint64_t)buf);
}

void sys_read_mouse(void* buf) { 
    if (buf) syscall1(101, (uint64_t)buf);
}

// ─── Port-based IPC (deprecated, kept for compatibility) ────────────────
void port_recv(void* a, void* b) { 
    if (a && b) syscall2(202, (uint64_t)a, (uint64_t)b);
}

void port_send(void* a, void* b) { 
    if (a && b) syscall2(201, (uint64_t)a, (uint64_t)b);
}
