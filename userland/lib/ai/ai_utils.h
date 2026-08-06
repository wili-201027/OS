// userland/lib/ai/ai_utils.h
#ifndef AI_UTILS_H
#define AI_UTILS_H

#include <stdint.h>

float ai_rand_float(void);
int ai_seed_rng(uint32_t seed);
void ai_scale_parameters(float *params, uint32_t count, float scale);
void ai_zero_gradients(float *grads, uint32_t count);

#endif // AI_UTILS_H
