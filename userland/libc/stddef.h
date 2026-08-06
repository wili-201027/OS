// userland/libc/stddef.h
// Minimal standard definitions for freestanding environment

#ifndef USERLAND_STDDEF_H
#define USERLAND_STDDEF_H

// Standard type definitions
typedef long ptrdiff_t;
typedef unsigned long size_t;

#ifndef __cplusplus
typedef int wchar_t;
#endif

#define NULL ((void*)0)
#define offsetof(type, member) ((size_t) &((type *)0)->member)

#endif // USERLAND_STDDEF_H
