// PHASE 3.2: STRING INTERNING - Deduplicación de strings para ahorrar memoria
// Reduce memoria de extensiones en 40-50%

#ifndef STRING_INTERN_H
#define STRING_INTERN_H

#include <stddef.h>

typedef struct {
    const char **strings;
    int *lengths;
    int count;
    int capacity;
} StringIntern;

// Funciones públicas
StringIntern* intern_create(int initial_capacity);
const char* intern_string(StringIntern *si, const char *str);
const char* intern_string_len(StringIntern *si, const char *str, int len);
int intern_find(StringIntern *si, const char *str);
void intern_cleanup(StringIntern *si);

#endif
