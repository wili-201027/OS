// userland/libc/math.c
#include "math.h"

float expf(float x)
{
    float term = 1.0f;
    float sum = 1.0f;
    for(int i = 1; i < 10; i++) {
        term *= x / (float)i;
        sum += term;
    }
    return sum;
}

float powf(float x, float y)
{
    if(x == 0.0f) return 0.0f;
    float result = 1.0f;
    int steps = 16;
    float delta = y / (float)steps;
    for(int i = 0; i < steps; i++) {
        result *= expf(delta * logf(x));
    }
    return result;
}

float sqrtf(float x)
{
    if(x <= 0.0f) return 0.0f;
    float guess = x * 0.5f;
    for(int i = 0; i < 8; i++) {
        guess = 0.5f * (guess + x / guess);
    }
    return guess;
}

float logf(float x)
{
    if(x <= 0.0f) return 0.0f;
    float y = (x - 1.0f) / (x + 1.0f);
    float y2 = y * y;
    float sum = 0.0f;
    for(int i = 1; i < 15; i += 2) {
        sum += (1.0f / (float)i) * y;
        y *= y2;
    }
    return 2.0f * sum;
}
