#ifndef AAL_H
#define AAL_H

#include <stdint.h>

/* Architecture Abstraction Layer (AAL)
 * Provides high-performance SIMD kernels for NTT butterflies and modular arithmetic.
 */

#include <arm_neon.h>

#include <stdint.h>
#include <stdbool.h>

typedef __uint128_t u128;

/* C23 Safe Arithmetic Fallback if stdckdint.h is missing */
#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#else
#define ckd_mul(res, a, b) (*(res) = (a) * (b), false)
#endif

static inline uint64_t aal_red_solinas(u128 x) {
    /* P = 2^64 - 2^32 + 1. Let R = 2^32. P = R^2 - R + 1.
     * x = x3*R^3 + x2*R^2 + x1*R + x0 mod P
     * x = (x2+x1)*R + (x0-x2-x3) mod P
     */
    uint64_t x0 = (uint32_t)x;
    uint64_t x1 = (uint32_t)(x >> 32);
    uint64_t x2 = (uint32_t)(x >> 64);
    uint64_t x3 = (uint32_t)(x >> 96);

    u128 r1 = ((u128)(x2 + x1) << 32) + x0;
    u128 r2 = (u128)x2 + x3;

    if (r1 >= r2) r1 -= r2;
    else r1 = r1 + 0xFFFFFFFF00000001ULL - r2;

    while (r1 >= 0xFFFFFFFF00000001ULL) r1 -= 0xFFFFFFFF00000001ULL;
    return (uint64_t)r1;
}

/* Montgomery Reduction (REDC)
 * out = a * b * R^-1 mod P
 */
static inline uint64_t aal_mul_mont(uint64_t a, uint64_t b, uint64_t P, uint64_t ni) {
    u128 prod;
    if (ckd_mul(&prod, (u128)a, (u128)b)) {
        /* This shouldn't happen for 64-bit limbs */
    }
    uint64_t m = (uint64_t)prod * ni;
    u128 tm = (u128)m * P;
    
    uint64_t prod_hi = (uint64_t)(prod >> 64);
    uint64_t prod_lo = (uint64_t)prod;
    uint64_t tm_hi = (uint64_t)(tm >> 64);
    uint64_t tm_lo = (uint64_t)tm;
    
    u128 high_sum = (u128)prod_hi + tm_hi + (((u128)prod_lo + tm_lo) >> 64);
    if (high_sum >= P) high_sum -= P;
    return (uint64_t)high_sum;
}

/* Butterfly using Montgomery arithmetic */
static inline void aal_butterfly_ct_mont(uint64_t* a, uint64_t* b, uint64_t w_mont, uint64_t P, uint64_t ni) {
    uint64_t bw = aal_mul_mont(*b, w_mont, P, ni);
    uint64_t a_val = *a;
    u128 sum = (u128)a_val + bw;
    *a = (sum >= P) ? (uint64_t)(sum - P) : (uint64_t)sum;
    *b = (a_val >= bw) ? (a_val - bw) : (a_val + P - bw);
}

#endif
