/* kernel/drivers/mal.h - Molecular Abstraction Layer (simulado)
 * Proporciona una interfaz muy simple para pruebas.
 */

#ifndef KERNEL_DRIVERS_MAL_H
#define KERNEL_DRIVERS_MAL_H

typedef struct mal_vector {
    double pos[3];
    double momentum[3];
} mal_vector_t;

int mal_init(void);
int mal_scan(mal_vector_t *out);

#endif /* KERNEL_DRIVERS_MAL_H */
