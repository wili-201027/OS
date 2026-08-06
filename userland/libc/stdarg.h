// userland/libc/stdarg.h
// Standard argument handling for variadic functions

#ifndef USERLAND_STDARG_H
#define USERLAND_STDARG_H

// Compiler-provided va_list macros (GCC built-ins)
typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif // USERLAND_STDARG_H
