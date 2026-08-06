// userland/libc/string.c
// String and memory functions for userland freestanding environment

#include <stddef.h>
#include <stdint.h>

// ─── String length ──────────────────────────────────────────────────────────
size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

// ─── String copy ────────────────────────────────────────────────────────────
char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    return dest;
}

// ─── String concatenation ───────────────────────────────────────────────────
char *strcat(char *dest, const char *src)
{
    char *d = dest;
    while (*d) d++;  // Find end
    while ((*d++ = *src++));
    return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (*d) d++;  // Find end
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    *d = '\0';
    return dest;
}

// ─── String comparison ──────────────────────────────────────────────────────
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n > 0 && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return (unsigned char)*s1 - (unsigned char)*s2;
}

// ─── String search ──────────────────────────────────────────────────────────
char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == 0) return (char *)s;  // Null terminator
    return NULL;
}

char *strrchr(const char *s, int c)
{
    char *result = NULL;
    while (*s) {
        if (*s == (char)c) result = (char *)s;
        s++;
    }
    if (c == 0) return (char *)s;
    return result;
}

char *strstr(const char *haystack, const char *needle)
{
    if (!*needle) return (char *)haystack;
    
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        
        if (!*n) return (char *)haystack;
        haystack++;
    }
    
    return NULL;
}

// ─── Memory operations ──────────────────────────────────────────────────────
void *memcpy(void *dest, const void *src, size_t n)
{
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    
    if (d < s) {
        // Forward copy
        while (n--) *d++ = *s++;
    } else if (d > s) {
        // Backward copy (to avoid overlap)
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    uint8_t *p = (uint8_t *)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;
    
    while (n--) {
        if (*a != *b) return (int)*a - (int)*b;
        a++;
        b++;
    }
    
    return 0;
}
