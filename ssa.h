#ifndef SSA_H
#define SSA_H

#include <stdint.h>
#include <string.h>
#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include "arena.h"

typedef __uint128_t u128;

typedef struct {
    uint64_t* ptr;
    size_t size_limbs;
    uint64_t extra; /* 1 if value is 2^N, else 0 */
} ssa_val_t;

static inline void ssa_add(ssa_val_t* res, ssa_val_t a, ssa_val_t b) {
    size_t n = res->size_limbs;
    if (a.extra && b.extra) {
        memset(res->ptr, 0xFF, n*8); res->ptr[0]=0xFFFFFFFFFFFFFFFEULL; res->extra=0; return;
    }
    if (a.extra || b.extra) {
        ssa_val_t other = a.extra ? b : a;
        uint64_t borrow = 1;
        for(size_t i=0; i<n; i++) {
            if (other.ptr[i] >= borrow) { res->ptr[i] = other.ptr[i]-borrow; borrow=0; }
            else { res->ptr[i] = 0xFFFFFFFFFFFFFFFFULL; borrow=1; }
        }
        res->extra = borrow; if (borrow) memset(res->ptr, 0, n*8);
        return;
    }
    uint64_t carry = 0;
    for(size_t i=0; i<n; i++) {
        u128 sum = (u128)a.ptr[i] + b.ptr[i] + carry;
        res->ptr[i] = (uint64_t)sum; carry = (uint64_t)(sum >> 64);
    }
    if (carry) {
        uint64_t borrow = 1;
        for(size_t i=0; i<n; i++) {
            if (res->ptr[i] >= borrow) { res->ptr[i] -= borrow; borrow=0; break; }
            else { res->ptr[i] = 0xFFFFFFFFFFFFFFFFULL; borrow=1; }
        }
        res->extra = borrow; if (borrow) memset(res->ptr, 0, n*8);
    } else res->extra = 0;
}

static inline void ssa_sub(ssa_val_t* res, ssa_val_t a, ssa_val_t b) {
    size_t n = res->size_limbs;
    if (b.extra) {
        uint64_t carry = 1;
        for(size_t i=0; i<n; i++) {
            u128 sum = (u128)a.ptr[i] + carry;
            res->ptr[i] = (uint64_t)sum; carry = (uint64_t)(sum >> 64);
        }
        res->extra = carry; if (carry) memset(res->ptr, 0, n*8);
        return;
    }
    if (a.extra) {
        int zero=1; for(size_t i=0; i<n; i++) if(b.ptr[i]) zero=0;
        if (zero) { res->extra=1; memset(res->ptr, 0, n*8); return; }
        uint64_t carry = 1;
        for(size_t i=0; i<n; i++) {
            u128 sum = (u128)(~b.ptr[i]) + carry;
            res->ptr[i] = (uint64_t)sum; carry = (uint64_t)(sum >> 64);
        }
        res->extra = 0; return;
    }
    uint64_t borrow = 0;
    for(size_t i=0; i<n; i++) {
        u128 val_b = (u128)b.ptr[i] + (u128)borrow;
        if ((u128)a.ptr[i] >= val_b) {
            res->ptr[i] = (uint64_t)(a.ptr[i] - val_b);
            borrow = 0;
        } else {
            res->ptr[i] = (uint64_t)(a.ptr[i] + ((u128)1 << 64) - val_b);
            borrow = 1;
        }
    }
    if (borrow) {
        uint64_t carry = 1;
        for(size_t i=0; i<n; i++) {
            u128 sum = (u128)res->ptr[i] + (u128)carry;
            res->ptr[i] = (uint64_t)sum; carry = (uint64_t)(sum >> 64);
        }
        res->extra = carry; if (carry) memset(res->ptr, 0, n*8);
    } else res->extra = 0;
}

static inline void ssa_shift_left(ssa_val_t* res, ssa_val_t a, size_t k) {
    size_t lb = k / 64, bb = k % 64, n = res->size_limbs;
    memset(res->ptr, 0, n * sizeof(uint64_t));
    uint64_t carry = 0;
    for (size_t i = 0; i + lb < n; i++) {
        uint64_t v = a.ptr[i];
        res->ptr[i+lb] = (v << bb) | carry;
        carry = bb ? (v >> (64-bb)) : 0;
    }
    res->extra = 0;
}

static inline void ssa_shift_right(ssa_val_t* res, ssa_val_t a, size_t k) {
    size_t lb = k / 64, bb = k % 64, n = res->size_limbs;
    memset(res->ptr, 0, n * sizeof(uint64_t));
    for (size_t i = lb; i < n; i++) {
        uint64_t v = a.ptr[i];
        size_t t = i - lb;
        res->ptr[t] |= (v >> bb);
        if (t > 0 && bb) res->ptr[t-1] |= (v << (64-bb));
    }
    res->extra = 0;
}

static inline void ssa_mul_2k(ssa_val_t* res, ssa_val_t a, size_t k) {
    size_t Nb = res->size_limbs * 64; k %= (2 * Nb);
    if (!k) { if (res->ptr != a.ptr) { memcpy(res->ptr, a.ptr, res->size_limbs*8); res->extra = a.extra; } return; }
    if (k >= Nb) {
        uint64_t* t = (uint64_t*)alloca(res->size_limbs*8); ssa_val_t tv = {t, res->size_limbs, 0};
        ssa_mul_2k(&tv, a, k - Nb);
        uint64_t* z = (uint64_t*)alloca(res->size_limbs*8); memset(z, 0, res->size_limbs*8);
        ssa_val_t zv = {z, res->size_limbs, 0}; ssa_sub(res, zv, tv);
        return;
    }
    if (a.extra) {
        uint64_t* t = (uint64_t*)alloca(res->size_limbs*8); ssa_val_t tv = {t, res->size_limbs, 0};
        uint64_t* one = (uint64_t*)alloca(res->size_limbs*8); memset(one, 0, res->size_limbs*8); one[0]=1;
        ssa_val_t ov = {one, res->size_limbs, 0};
        ssa_shift_left(&tv, ov, k);
        uint64_t* z = (uint64_t*)alloca(res->size_limbs*8); memset(z, 0, res->size_limbs*8);
        ssa_val_t zv = {z, res->size_limbs, 0}; ssa_sub(res, zv, tv);
        return;
    }
    uint64_t* lp = (uint64_t*)alloca(res->size_limbs*8); uint64_t* hp = (uint64_t*)alloca(res->size_limbs*8);
    ssa_val_t lv = {lp, res->size_limbs, 0}, hv = {hp, res->size_limbs, 0};
    ssa_shift_left(&lv, a, k); ssa_shift_right(&hv, a, Nb - k);
    ssa_sub(res, lv, hv);
}

static inline void ssa_bit_reverse(ssa_val_t* d, size_t K) {
    for (size_t i = 0, j = 0; i < K; i++) {
        if (i < j) {
            ssa_val_t tmp = d[i];
            d[i] = d[j];
            d[j] = tmp;
        }
        size_t m = K >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }
}

static inline void ssa_fft_partial(ssa_val_t* d, size_t K, size_t Nb, size_t start_len, int inv) {
    if (K <= 1 || start_len < 2) return;
    size_t nrl = d[0].size_limbs;

    /* Iterative DIF FFT starting from start_len */
    for (size_t len = start_len; len >= 2; len >>= 1) {
        size_t h = len / 2;
        size_t step = (2 * Nb) / len;
        
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < K; j += len) {
            /* Use a local buffer on stack. Since j loop is parallel, 
             * we need to be careful. But better to use malloc here 
             * or pass a pre-allocated buffer. 
             * For now, use a tight scope for tp if compiler supports.
             * Better: use malloc/free inside j loop to avoid stack growth.
             */
            uint64_t* tp = (uint64_t*)malloc(nrl * 8);
            ssa_val_t t = {tp, nrl, 0};
            for (size_t i = 0; i < h; i++) {
                size_t p = j + i;
                size_t q = j + i + h;
                ssa_val_t u = d[p], v = d[q];
                ssa_add(&t, u, v);
                ssa_sub(&d[q], u, v);
                ssa_mul_2k(&d[q], d[q], inv ? (2*Nb - i*step) : (i*step));
                memcpy(d[p].ptr, t.ptr, nrl*8); d[p].extra = t.extra;
            }
            free(tp);
        }
    }
}

static inline void ssa_fft(ssa_val_t* d, size_t K, size_t Nb, int inv) {
    ssa_fft_partial(d, K, Nb, K, inv);
    ssa_bit_reverse(d, K);
}

static inline void ssa_scale(ssa_val_t* d, size_t K, size_t Nb) {
    size_t k = 0; while((1ULL << k) < K) k++;
    for(size_t i=0; i<K; i++) ssa_mul_2k(&d[i], d[i], 2*Nb - k);
}

/* Base Case: Multiply in ring. Since Nb is large, use recursive call or simpler.
 * For now, placeholder or linkage to bigint_mul.
 */
static inline void ssa_mul_ring_buf(ssa_val_t* res, ssa_val_t a, ssa_val_t b, uint64_t* prod) {
    if (a.extra) {
        uint64_t* zero = (uint64_t*)alloca(b.size_limbs * 8); memset(zero, 0, b.size_limbs * 8);
        ssa_sub(res, (ssa_val_t){zero, b.size_limbs, 0}, b); return;
    }
    if (b.extra) {
        uint64_t* zero = (uint64_t*)alloca(a.size_limbs * 8); memset(zero, 0, a.size_limbs * 8);
        ssa_sub(res, (ssa_val_t){zero, a.size_limbs, 0}, a); return;
    }
    
    size_t n = a.size_limbs;
    memset(prod, 0, 2 * n * 8);
    
    /* Tiled Schoolbook Multiplication (L1-Cache Optimized) - Disabled for debug */
    const size_t TILE = n;
    for (size_t ii = 0; ii < n; ii += TILE) {
        size_t i_end = (ii + TILE > n) ? n : ii + TILE;
        for (size_t jj = 0; jj < n; jj += TILE) {
            size_t j_end = (jj + TILE > n) ? n : jj + TILE;
            
            for (size_t i = ii; i < i_end; i++) {
                uint64_t ai = a.ptr[i];
                if (!ai) continue;
                uint64_t carry = 0;
                for (size_t j = jj; j < j_end; j++) {
                    u128 m = (u128)ai * b.ptr[j] + prod[i+j] + carry;
                    prod[i+j] = (uint64_t)m;
                    carry = (uint64_t)(m >> 64);
                }
                /* Propagate carry to the end of the product buffer */
                for (size_t k = i + j_end; carry && k < 2 * n; k++) {
                    u128 m = (u128)prod[k] + carry;
                    prod[k] = (uint64_t)m;
                    carry = (uint64_t)(m >> 64);
                }
            }
        }
    }
    
    ssa_val_t lo = {prod, n, 0};
    ssa_val_t hi = {prod + n, n, 0};
    ssa_sub(res, lo, hi);
}

static inline void ssa_mul_ring(ssa_val_t* res, ssa_val_t a, ssa_val_t b, arena_t* q) {
    size_t n = a.size_limbs;
    uint64_t* prod = (uint64_t*)arena_alloc(q, 2 * n * 8);
    ssa_mul_ring_buf(res, a, b, prod);
}

#endif
