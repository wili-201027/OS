// userland/libc/time.h
// Minimal time support for freestanding environment

#ifndef TIME_H
#define TIME_H

#include <stdint.h>
#include <stddef.h>

typedef uint64_t time_t;
typedef uint64_t clock_t;

#define CLOCKS_PER_SEC 1000000

// Placeholder: En ambiente real, estas funciones harían syscalls al kernel
static inline time_t time(time_t *t) {
    // Stub: Retorna contador simple
    static time_t counter = 0;
    if(t) *t = counter;
    return counter++;
}

static inline clock_t clock(void) {
    // Stub: Retorna contador simple
    static clock_t counter = 0;
    return counter++;
}

static inline uint64_t get_time_ms(void) {
    // Obtener tiempo en milisegundos
    static uint64_t counter = 0;
    return counter++;
}

#endif
