// userland/libc/stdio.h
// Userland standard I/O library (freestanding environment - minimal)

#ifndef USERLAND_STDIO_H
#define USERLAND_STDIO_H

#include <stddef.h>
#include <stdarg.h>

// Output functions (simplified for freestanding environment)
int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

// Utility
int putchar(int c);
int puts(const char *s);

#endif // USERLAND_STDIO_H
