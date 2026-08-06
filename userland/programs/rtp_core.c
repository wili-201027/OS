/*
 * rtp_core.c - Simulación del Return to the Past (rtp_core.ext)
 */

#include <stdio.h>
#include "../libc/stdlib.h"
#include "../libc/time.h"

static void log_line(const char *s) {
    uint64_t t = get_time_ms();
    printf("[%llu ms] %s\n", (unsigned long long)t, s);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    log_line("[INIT] Aislamiento de registros de memoria: QUBITS_CHIPS_0_to_4");
    log_line("[READY] Registros anclados térmicamente en Helio Superfluido.");
    log_line("[EXEC] Inyectando pulso de taquiones a través de la red de Torres Activas...");
    log_line("[WARN] Entropía planetaria invirtiéndose. Tiempo local: -24.00h");
    log_line("[SUCCESS] Línea temporal reajustada.");
    return 0;
}
