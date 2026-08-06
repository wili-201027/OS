// userland/lib/ai/ai_utils.c
#include "ai_utils.h"
#include "ai_core.h"
#include "../../libc/stdlib.h"
#include "../../libc/string.h"

static uint32_t ai_global_seed = 1;

float ai_rand_float(void)
{
    ai_global_seed = (ai_global_seed * 1664525u + 1013904223u) & 0x7fffffff;
    return (float)ai_global_seed / 2147483647.0f;
}

int ai_seed_rng(uint32_t seed)
{
    if(seed == 0) seed = 1;
    ai_global_seed = seed;
    return 0;
}

void ai_scale_parameters(float *params, uint32_t count, float scale)
{
    if(!params || count == 0) return;
    for(uint32_t i = 0; i < count; i++) {
        params[i] *= scale;
    }
}

void ai_zero_gradients(float *grads, uint32_t count)
{
    if(!grads || count == 0) return;
    for(uint32_t i = 0; i < count; i++) {
        grads[i] = 0.0f;
    }
}
