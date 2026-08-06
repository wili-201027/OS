// kernel/string.h
// Kernel string library (freestanding environment)

#ifndef KERNEL_STRING_H
#define KERNEL_STRING_H

#include <stddef.h>

// Memory operations
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

// String operations
size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

#endif // KERNEL_STRING_H
