// userland/libc/stdint.h
// Standard integer types for freestanding environment

#ifndef USERLAND_STDINT_H
#define USERLAND_STDINT_H

// Signed integer types
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

// Unsigned integer types
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// Fast integer types
typedef int8_t int_fast8_t;
typedef int16_t int_fast16_t;
typedef int32_t int_fast32_t;
typedef int64_t int_fast64_t;

typedef uint8_t uint_fast8_t;
typedef uint16_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

// Pointer integer types
typedef int64_t intptr_t;
typedef uint64_t uintptr_t;

// Greatest-width integer types
typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

// Limits
#define INT8_MAX 127
#define INT8_MIN (-128)
#define UINT8_MAX 255

#define INT16_MAX 32767
#define INT16_MIN (-32768)
#define UINT16_MAX 65535

#define INT32_MAX 2147483647
#define INT32_MIN (-2147483648)
#define UINT32_MAX 4294967295U

#define INT64_MAX 9223372036854775807LL
#define INT64_MIN (-9223372036854775807LL - 1)
#define UINT64_MAX 18446744073709551615ULL

#endif // USERLAND_STDINT_H
