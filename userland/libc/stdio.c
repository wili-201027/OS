// userland/libc/stdio.c
// Standard I/O implementation for userland (freestanding)

#include "stdio.h"
#include "string.h"

// Simple output buffer for printf/sprintf operations
#define PRINTF_BUFFER_SIZE 4096
static char printf_buffer[PRINTF_BUFFER_SIZE];

// Forward declarations
extern void sys_write_log(const char *str);  // To be implemented in kernel

// Helper: convert integer to string
static char *_itoa(int value, char *str, int base)
{
    char *start = str;
    char *ptr = str;
    char *low = str;
    int digit;
    int negative = (value < 0);
    
    if (negative) {
        *str++ = '-';
        value = -value;
    }
    
    if (value == 0) {
        *str++ = '0';
        *str = '\0';
        return start;
    }
    
    while (value > 0) {
        digit = value % base;
        if (digit < 10) {
            *str++ = '0' + digit;
        } else {
            *str++ = 'A' + (digit - 10);
        }
        value /= base;
    }
    *str = '\0';
    
    // Reverse
    low = start;
    if (negative) low++;
    --str;
    while (low < str) {
        char tmp = *low;
        *low++ = *str;
        *str-- = tmp;
    }
    
    return start;
}

// Helper: convert unsigned integer to string
static char *_utoa(unsigned int value, char *str, int base)
{
    char *start = str;
    char *ptr = str;
    int digit;
    
    if (value == 0) {
        *str++ = '0';
        *str = '\0';
        return start;
    }
    
    while (value > 0) {
        digit = value % base;
        if (digit < 10) {
            *str++ = '0' + digit;
        } else {
            *str++ = 'A' + (digit - 10);
        }
        value /= base;
    }
    *str = '\0';
    
    // Reverse
    --str;
    while (start < str) {
        char tmp = *start;
        *start++ = *str;
        *str-- = tmp;
    }
    
    return start;
}

// Core vsnprintf implementation
int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    if (!str || !format || size == 0) return -1;
    
    char *dest = str;
    size_t remaining = size - 1;  // Leave room for null terminator
    
    while (*format && remaining > 0) {
        if (*format == '%') {
            format++;
            
            // Skip flags/width/precision for now (simplified)
            while (*format && (*format == '-' || *format == '+' || 
                               *format == ' ' || *format == '0' || 
                               (*format >= '0' && *format <= '9'))) {
                format++;
            }
            
            // Handle format specifier
            if (*format == 'd' || *format == 'i') {
                int val = va_arg(ap, int);
                char buf[32];
                _itoa(val, buf, 10);
                size_t len = strlen(buf);
                if (len > remaining) len = remaining;
                strncpy(dest, buf, len);
                dest += len;
                remaining -= len;
                format++;
            }
            else if (*format == 'u') {
                unsigned int val = va_arg(ap, unsigned int);
                char buf[32];
                _utoa(val, buf, 10);
                size_t len = strlen(buf);
                if (len > remaining) len = remaining;
                strncpy(dest, buf, len);
                dest += len;
                remaining -= len;
                format++;
            }
            else if (*format == 'x' || *format == 'X') {
                unsigned int val = va_arg(ap, unsigned int);
                char buf[32];
                _utoa(val, buf, 16);
                size_t len = strlen(buf);
                if (len > remaining) len = remaining;
                strncpy(dest, buf, len);
                dest += len;
                remaining -= len;
                format++;
            }
            else if (*format == 's') {
                const char *str = va_arg(ap, const char *);
                if (str) {
                    size_t len = strlen(str);
                    if (len > remaining) len = remaining;
                    strncpy(dest, str, len);
                    dest += len;
                    remaining -= len;
                }
                format++;
            }
            else if (*format == 'c') {
                int ch = va_arg(ap, int);
                *dest++ = (char)ch;
                remaining--;
                format++;
            }
            else if (*format == 'p') {
                unsigned long ptr = (unsigned long)va_arg(ap, void *);
                char buf[32];
                strcpy(buf, "0x");
                _utoa((unsigned int)ptr, buf + 2, 16);
                size_t len = strlen(buf);
                if (len > remaining) len = remaining;
                strncpy(dest, buf, len);
                dest += len;
                remaining -= len;
                format++;
            }
            else if (*format == '%') {
                *dest++ = '%';
                remaining--;
                format++;
            }
        }
        else if (*format == '\\' && *(format + 1) == 'n') {
            *dest++ = '\n';
            remaining--;
            format += 2;
        }
        else {
            *dest++ = *format++;
            remaining--;
        }
    }
    
    *dest = '\0';
    return (dest - str);
}

int vsprintf(char *str, const char *format, va_list ap)
{
    return vsnprintf(str, PRINTF_BUFFER_SIZE, format, ap);
}

int sprintf(char *str, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vsprintf(str, format, ap);
    va_end(ap);
    return result;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vsnprintf(str, size, format, ap);
    va_end(ap);
    return result;
}

int vprintf(const char *format, va_list ap)
{
    int result = vsnprintf(printf_buffer, sizeof(printf_buffer), format, ap);
    if (result > 0) {
        sys_write_log(printf_buffer);
    }
    return result;
}

int printf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vprintf(format, ap);
    va_end(ap);
    return result;
}

int putchar(int c)
{
    char buf[2] = {(char)c, '\0'};
    sys_write_log(buf);
    return c;
}

int puts(const char *s)
{
    if (!s) return -1;
    sys_write_log(s);
    sys_write_log("\n");
    return 0;
}

// Stub: implement sys_write_log in kernel if needed
void sys_write_log(const char *str)
{
    // This would be implemented in kernel to output to console/serial
    // For now, this is a no-op stub
    (void)str;
}
