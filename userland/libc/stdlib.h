// userland/libc/stdlib.h
// Userland standard library (freestanding environment)

#ifndef USERLAND_STDLIB_H
#define USERLAND_STDLIB_H

#include <stddef.h>
#include <stdint.h>

// Memory allocation
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);

// Exit and control
void exit(int status);
void abort(void);

// Threading and task control
int sys_thread_create(void *entry, void *arg, void *stack_bottom, uint64_t stack_size);
void sys_thread_exit(int exit_code);

// String conversion
static inline int atoi(const char *str) {
    if(!str) return 0;
    int val = 0, neg = 0;
    if(*str == '-') { neg = 1; str++; }
    while(*str && *str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    return neg ? -val : val;
}

static inline long atol(const char *str) {
    return (long)atoi(str);
}

static inline double atof(const char *str) {
    if(!str) return 0.0;
    double val = 0.0, frac = 0.1, neg = 0;
    int exp_val = 0, exp_neg = 0;
    
    // Handle negative sign
    if(*str == '-') { neg = 1; str++; }
    
    // Integer part
    while(*str && *str >= '0' && *str <= '9') {
        val = val * 10.0 + (*str - '0');
        str++;
    }
    
    // Fractional part
    if(*str == '.') {
        str++;
        while(*str && *str >= '0' && *str <= '9') {
            val = val + frac * (*str - '0');
            frac *= 0.1;
            str++;
        }
    }
    
    // Exponent part
    if(*str == 'e' || *str == 'E') {
        str++;
        if(*str == '-') { exp_neg = 1; str++; }
        else if(*str == '+') str++;
        
        while(*str && *str >= '0' && *str <= '9') {
            exp_val = exp_val * 10 + (*str - '0');
            str++;
        }
        
        // Apply exponent
        double mult = 1.0;
        for(int i = 0; i < exp_val; i++) {
            if(exp_neg) mult *= 0.1;
            else mult *= 10.0;
        }
        val *= mult;
    }
    
    return neg ? -val : val;
}

#endif // USERLAND_STDLIB_H
