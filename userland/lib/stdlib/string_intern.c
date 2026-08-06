// PHASE 3.2: STRING INTERNING - Implementación
// Deduplicación e interning de strings

#include "string_intern.h"
#include "../../libc/stdlib.h"
#include "../../libc/string.h"

StringIntern* intern_create(int initial_capacity) {
    if (initial_capacity <= 0) initial_capacity = 256;
    
    StringIntern *si = malloc(sizeof(StringIntern));
    if (!si) return NULL;
    
    si->strings = malloc(sizeof(const char*) * initial_capacity);
    si->lengths = malloc(sizeof(int) * initial_capacity);
    
    if (!si->strings || !si->lengths) {
        free(si->strings);
        free(si->lengths);
        free(si);
        return NULL;
    }
    
    si->count = 0;
    si->capacity = initial_capacity;
    
    return si;
}

const char* intern_string(StringIntern *si, const char *str) {
    if (!si || !str) return NULL;
    return intern_string_len(si, str, strlen(str));
}

const char* intern_string_len(StringIntern *si, const char *str, int len) {
    if (!si || !str || len < 0) return NULL;
    
    // Búsqueda lineal (típicamente pocas strings)
    for(int i = 0; i < si->count; i++) {
        if(si->lengths[i] == len && strncmp(si->strings[i], str, len) == 0) {
            // ✓ String ya existe - retornar referencia
            return si->strings[i];
        }
    }
    
    // String no encontrada - crear nueva copia
    if(si->count >= si->capacity) {
        // Expandir capacity
        int new_capacity = si->capacity * 2;
        const char **new_strings = realloc(si->strings, sizeof(const char*) * new_capacity);
        int *new_lengths = realloc(si->lengths, sizeof(int) * new_capacity);
        
        if (!new_strings || !new_lengths) {
            return NULL;
        }
        
        si->strings = new_strings;
        si->lengths = new_lengths;
        si->capacity = new_capacity;
    }
    
    // Crear copia de la string
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    
    strncpy(copy, str, len);
    copy[len] = '\0';
    
    si->strings[si->count] = copy;
    si->lengths[si->count] = len;
    si->count++;
    
    return copy;
}

int intern_find(StringIntern *si, const char *str) {
    if (!si || !str) return -1;
    
    int len = strlen(str);
    
    for(int i = 0; i < si->count; i++) {
        if(si->lengths[i] == len && strcmp(si->strings[i], str) == 0) {
            return i;
        }
    }
    
    return -1;
}

void intern_cleanup(StringIntern *si) {
    if (!si) return;
    
    // Liberar todas las strings copiadas
    for(int i = 0; i < si->count; i++) {
        free((void*)si->strings[i]);
    }
    
    free(si->strings);
    free(si->lengths);
    free(si);
}
