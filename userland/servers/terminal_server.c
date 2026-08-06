// userland/servers/terminal_server.c
// Simple terminal emulator using window APIs via syscalls

#include <stdint.h>
#include <stddef.h>

extern void sys_yield(void);

// Syscall wrappers for window operations
extern uint64_t syscall0(uint64_t n);
extern uint64_t syscall2(uint64_t n, uint64_t a, uint64_t b);

#define SYS_WINDOW_CLEAR      19
#define SYS_WINDOW_WRITE      20
#define SYS_WINDOW_FILL_RECT  21

#define RGB(r,g,b) (0xFF000000 | ((uint32_t)(r)<<16) | ((uint32_t)(g)<<8) | (b))


void terminal_server_start(void)
{
    // For now, just yield to prevent blocking
    // Terminal functionality requires kernel-space window manager access
    // which should be called from kernel init instead
    
    while (1) {
        sys_yield();
    }
}
