// userland/libc/window.h
// Window manager APIs for userland applications

#ifndef _WINDOW_H
#define _WINDOW_H

#include <stdint.h>
#include <stddef.h>

// Syscall numbers
#define SYS_WINDOW_CLEAR      19
#define SYS_WINDOW_WRITE      20
#define SYS_WINDOW_FILL_RECT  21

// Syscall helpers (inline asm)
static inline uint64_t syscall1(uint64_t n, uint64_t a) {
    uint64_t ret;
    asm volatile("syscall":"=a"(ret):"a"(n),"D"(a));
    return ret;
}

static inline uint64_t syscall2(uint64_t n, uint64_t a, uint64_t b) {
    uint64_t ret;
    asm volatile("syscall":"=a"(ret):"a"(n),"D"(a),"S"(b));
    return ret;
}

static inline uint64_t syscall4(uint64_t n, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    uint64_t ret;
    asm volatile("syscall":"=a"(ret):"a"(n),"D"(a),"S"(b),"d"(c),"r"(8)(d));
    return ret;
}

// Window opacity type
typedef int window_id_t;

// Color helpers
#define RGB(r,g,b) (0xFF000000 | ((uint32_t)(r)<<16) | ((uint32_t)(g)<<8) | (b))
#define ARGB(a,r,g,b) (((uint32_t)(a)<<24) | ((uint32_t)(r)<<16) | ((uint32_t)(g)<<8) | (b))

// Window struct (opaque to userland)
typedef void* window_t;

// Public window APIs (inline wrappers)
static inline void window_clear(window_id_t wid, uint32_t color) {
    syscall2(SYS_WINDOW_CLEAR, (uint64_t)wid, (uint64_t)color);
}

static inline void window_fill_rect(window_id_t wid, int x, int y, int w, int h, uint32_t color) {
    // r9=color, r8=h, rdx=w, rsi=y, rdi=x, rax=syscall
    uint64_t ret;
    asm volatile("syscall"
                 : "=a" (ret)
                 : "a" (SYS_WINDOW_FILL_RECT),
                   "D" (wid),
                   "S" (x),
                   "d" (y),
                   "r" (8) (h),
                   "r" (9) (color)
                 : "rcx", "r11");
}

static inline void window_write(window_id_t wid, int x, int y, const char *text, uint32_t color) {
    // This requires 5 args, use inline asm carefully
    uint64_t ret;
    asm volatile("mov %5, %%r10; syscall"
                 : "=a" (ret)
                 : "a" (SYS_WINDOW_WRITE),
                   "D" (wid),
                   "S" (x),
                   "d" (y),
                   "r" (text),
                   "r" (color)
                 : "rcx", "r11", "r10");
}

// Common colors
#define COLOR_BLACK      RGB(0, 0, 0)
#define COLOR_WHITE      RGB(255, 255, 255)
#define COLOR_RED        RGB(255, 0, 0)
#define COLOR_GREEN      RGB(0, 255, 0)
#define COLOR_BLUE       RGB(0, 0, 255)
#define COLOR_CYAN       RGB(0, 255, 255)
#define COLOR_MAGENTA    RGB(255, 0, 255)
#define COLOR_YELLOW     RGB(255, 255, 0)
#define COLOR_DARK_BLUE  RGB(26, 10, 46)
#define COLOR_GLASS_BG   RGB(40, 50, 90)

#endif // _WINDOW_H
