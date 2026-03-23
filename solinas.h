#pragma once
#include <stdint.h>

// We require exact NTT-friendly primes. 
// For maximum ILP, we select Solinas primes allowing bitwise-only modulo.

// P1 = 2^61 - 1 (Mersenne Prime, ultra-fast reduction)
#define SOLINAS_P1 0x1FFFFFFFFFFFFFFFULL

// Fast modulo reduction for P1 utilizing pure bitwise shifts
static inline uint64_t solinas_reduce_p1(uint64_t x) {
    uint64_t sl = x & SOLINAS_P1;
    uint64_t sh = x >> 61;
    uint64_t r = sl + sh;
    if (r >= SOLINAS_P1) r -= SOLINAS_P1;
    return r;
}

// Full 128-bit reduction for P1
static inline uint64_t solinas_reduce_128_p1(unsigned __int128 x) {
    uint64_t sl = (uint64_t)(x & SOLINAS_P1);
    uint64_t sh = (uint64_t)(x >> 61);
    return solinas_reduce_p1(sl + sh);
}

// Modular multiplication for P1
static inline uint64_t solinas_mul_p1(uint64_t a, uint64_t b) {
    unsigned __int128 prod = (unsigned __int128)a * b;
    return solinas_reduce_128_p1(prod);
}

// Modular addition
static inline uint64_t solinas_add_p1(uint64_t a, uint64_t b) {
    uint64_t r = a + b;
    if (r >= SOLINAS_P1) r -= SOLINAS_P1;
    return r;
}

// Modular subtraction
static inline uint64_t solinas_sub_p1(uint64_t a, uint64_t b) {
    if (a < b) return a + SOLINAS_P1 - b;
    return a - b;
}
