/*
 * net_scanner.c - Simulación del Superscan (net_scanner.prg)
 * Escanea flujos (simulados) y detecta picos de "entropía negativa".
 */

#include <stdio.h>
#include "../libc/stdlib.h"
#include "../libc/time.h"

// Simple logger using millisecond timestamp (freestanding environment)
static void log_msg(const char *s) {
    uint64_t t = get_time_ms();
    printf("[%llu ms] %s\n", (unsigned long long)t, s);
}

// Tiny LCG RNG for freestanding environment
static unsigned int _rng_state = 1;
static void my_srand(unsigned int seed) { _rng_state = seed ? seed : 1; }
static int my_rand(void) { _rng_state = _rng_state * 1103515245u + 12345u; return (int)(_rng_state & 0x7fffffff); }
static const int MY_RAND_MAX = 0x7fffffff;

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    log_msg("INIT: net_scanner residente en memoria (simulado)");
    log_msg("LISTEN: Monitorizando frecuencias y enlaces (simulado)");

    my_srand((unsigned)(get_time_ms() & 0xFFFFFFFF));
    for (int i = 0; i < 100; ++i) {
        double sample = my_rand() / (MY_RAND_MAX + 1.0);
        if (sample > 0.995) {
            char buf[128];
            int tower = 1 + (my_rand() % 5);
            snprintf(buf, sizeof(buf), "ALERT: Fluctuación detectada (score=%.5f) - probable puente en Torre %d", sample, tower);
            log_msg(buf);
        }
    }

    log_msg("IDLE: net_scanner entra en modo pasivo (simulado)");
    return 0;
}
