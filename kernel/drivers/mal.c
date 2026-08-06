/* kernel/drivers/mal.c - Implementación simulada de MAL
 * Atención: no controla hardware real.
 */

#include "mal.h"
#include "../string.h"

int mal_init(void) {
    /* Inicialización simulada */
    return 0;
}

int mal_scan(mal_vector_t *out) {
    if (!out) return -1;
    /* Rellenar con valores simulados deterministas */
    out->pos[0] = 0.123; out->pos[1] = 0.456; out->pos[2] = 0.789;
    out->momentum[0] = 0.001; out->momentum[1] = -0.002; out->momentum[2] = 0.0005;
    return 0;
}
