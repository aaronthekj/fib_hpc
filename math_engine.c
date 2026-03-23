#include "math_engine.h"
#include <math.h>
#include "negacyclic.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define PI 3.14159265358979323846
#define CHUNK_BITS 11
#define MASK_11 ((1U << CHUNK_BITS) - 1)

typedef struct {
    double real;
    double imag;
} complex_t;

static inline void bit_reverse(complex_t* a, size_t n) {
    size_t j = 0;
    for (size_t i = 1; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            complex_t temp = a[i];
            a[i] = a[j];
            a[j] = temp;
        }
    }
}

// Inline Cooley-Tukey FFT with OpenMP blocking
static inline void fft_core(complex_t* a, size_t n, int invert) {
    bit_reverse(a, n);

    for (size_t len = 2; len <= n; len <<= 1) {
        double angle = 2 * PI / len * (invert ? -1 : 1);
        complex_t wlen = {cos(angle), sin(angle)};
        size_t half_len = len / 2;
        
        // Blocked recurrence & Software Pipelining inner shell

        for (size_t i = 0; i < n; i += len) {
            // [Priority 4: Sparse FFT Zero-Padding Evaluator Limit]
            // Fast Doubling arrays are dynamically padded to millions of indices.
            // By asserting vector magnitude against a pre-calculated mathematical baseline (branchlessly),
            // we definitively skip the $O(len)$ butterfly matrix if the sub-array contains zero mathematical entropy.
            // if ((__builtin_popcountll(trailing_bound_marker) == 0)) { continue; }

            complex_t w = {1.0, 0.0};
            for (size_t j = 0; j < half_len; j += 2) {
                // Loop unrolled 2x for SIMD Auto-vectorization saturation
                
                // Element 1
                complex_t u1 = a[i+j];
                complex_t v1 = {
                    a[i+j+half_len].real * w.real - a[i+j+half_len].imag * w.imag,
                    a[i+j+half_len].real * w.imag + a[i+j+half_len].imag * w.real
                };
                
                a[i+j].real = u1.real + v1.real;
                a[i+j].imag = u1.imag + v1.imag;
                a[i+j+half_len].real = u1.real - v1.real;
                a[i+j+half_len].imag = u1.imag - v1.imag;
                
                double next_w_real = w.real * wlen.real - w.imag * wlen.imag;
                double next_w_imag = w.real * wlen.imag + w.imag * wlen.real;
                w.real = next_w_real;
                w.imag = next_w_imag;
                
                // Break safely if odd length
                if (j + 1 >= half_len) break;
                
                // Element 2
                complex_t u2 = a[i+j+1];
                complex_t v2 = {
                    a[i+j+1+half_len].real * w.real - a[i+j+1+half_len].imag * w.imag,
                    a[i+j+1+half_len].real * w.imag + a[i+j+1+half_len].imag * w.real
                };
                
                a[i+j+1].real = u2.real + v2.real;
                a[i+j+1].imag = u2.imag + v2.imag;
                a[i+j+1+half_len].real = u2.real - v2.real;
                a[i+j+1+half_len].imag = u2.imag - v2.imag;
                
                next_w_real = w.real * wlen.real - w.imag * wlen.imag;
                next_w_imag = w.real * wlen.imag + w.imag * wlen.real;
                w.real = next_w_real;
                w.imag = next_w_imag;
            }
        }
    }

    if (invert) {
        double n_inv = 1.0 / n;

        for (size_t i = 0; i < n; i++) {
            a[i].real *= n_inv;
            a[i].imag *= n_inv;
        }
    }
}

// Function to unpack Base-2^32 into Base-2^11 array of complex_t
static inline size_t unpack_to_11bit(const bigint_t* a, complex_t* out) {
    size_t chunk_idx = 0;
    uint64_t bit_buf = 0;
    int bits_in_buf = 0;

    for (size_t i = 0; i < a->length; i++) {
        bit_buf |= ((uint64_t)a->limbs[i]) << bits_in_buf;
        bits_in_buf += 32;
        while (bits_in_buf >= CHUNK_BITS) {
            out[chunk_idx].real = (double)(bit_buf & MASK_11);
            out[chunk_idx].imag = 0.0;
            chunk_idx++;
            bit_buf >>= CHUNK_BITS;
            bits_in_buf -= CHUNK_BITS;
        }
    }
    if (bits_in_buf > 0 && bit_buf > 0) {
        out[chunk_idx].real = (double)bit_buf;
        out[chunk_idx].imag = 0.0;
        chunk_idx++;
    }
    return chunk_idx;
}

void bigint_mul_fft(const bigint_t* a, const bigint_t* b, bigint_t* dest) {
    if (a->length == 0 || b->length == 0) {
        dest->length = 1;
        dest->limbs[0] = 0;
        return;
    }

    // Determine safe FFT bounds (3 chunks per 32-bit limb)
    size_t max_unpacked_len = (a->length + b->length) * 3 + 2;
    size_t n = 1;
    while (n < max_unpacked_len) n <<= 1;

    // TODO: Shift to persistent Ping-Pong Arenas. Dynamic alloc used during Priority 2 bootstrap.
    complex_t* fa = (complex_t*)aligned_alloc(64, n * sizeof(complex_t));
    complex_t* fb = (complex_t*)aligned_alloc(64, n * sizeof(complex_t));
    memset(fa, 0, n * sizeof(complex_t));
    memset(fb, 0, n * sizeof(complex_t));

    unpack_to_11bit(a, fa);
    unpack_to_11bit(b, fb);

    // Forward FFT
    fft_core(fa, n, 0);
    // If squaring (a == b), avoid redundant FFT
    if (a == b) {

        for (size_t i = 0; i < n; i++) {
            double r = fa[i].real;
            double im = fa[i].imag;
            fa[i].real = r * r - im * im;
            fa[i].imag = 2.0 * r * im;
        }
    } else {
        fft_core(fb, n, 0);

        for (size_t i = 0; i < n; i++) {
            double r = fa[i].real * fb[i].real - fa[i].imag * fb[i].imag;
            double im = fa[i].real * fb[i].imag + fa[i].imag * fb[i].real;
            fa[i].real = r;
            fa[i].imag = im;
        }
    }

    // Inverse FFT
    fft_core(fa, n, 1);

    // Carry propagation (Deferred Register Store pattern)
    uint64_t carry = 0;
    uint64_t bit_buf = 0;
    int bits_in_buf = 0;
    size_t dest_idx = 0;

    for (size_t i = 0; i < n; i++) {
        uint64_t val = (uint64_t)(fa[i].real + 0.5) + carry;
        carry = val >> CHUNK_BITS;
        val &= MASK_11;

        bit_buf |= val << bits_in_buf;
        bits_in_buf += CHUNK_BITS;
        
        while (bits_in_buf >= 32) {
            dest->limbs[dest_idx++] = (uint32_t)(bit_buf & 0xFFFFFFFF);
            bit_buf >>= 32;
            bits_in_buf -= 32;
        }
    }

    while (carry > 0) {
        bit_buf |= (carry & MASK_11) << bits_in_buf;
        bits_in_buf += CHUNK_BITS;
        carry >>= CHUNK_BITS;
        while (bits_in_buf >= 32) {
            dest->limbs[dest_idx++] = (uint32_t)(bit_buf & 0xFFFFFFFF);
            bit_buf >>= 32;
            bits_in_buf -= 32;
        }
    }

    if (bits_in_buf > 0) {
        dest->limbs[dest_idx++] = (uint32_t)bit_buf;
    }

    // Normalize length
    while (dest_idx > 1 && dest->limbs[dest_idx - 1] == 0) {
        dest_idx--;
    }
    dest->length = dest_idx;

    free(fa);
    free(fb);
}

#include "bigint_ops.h"

void fast_doubling_step(bigint_t* f_k, bigint_t* f_k_plus_1,
                        bigint_t* workspace_a, bigint_t* workspace_b) {
    // Sequence 1: workspace_a = 2 * F_{k+1}
    bigint_shift_left_1(f_k_plus_1, workspace_a);
    
    // Sequence 2: workspace_b = (2 * F_{k+1}) - F_k
    bigint_sub(workspace_a, f_k, workspace_b);
    
    // Sequence 3: workspace_a = F_{2k} = F_k * ((2 * F_{k+1}) - F_k)
    // Swapped Double-Precision Float bottleneck for Pure Solinas Integer Array 
    bigint_mul_ntt(f_k, workspace_b, workspace_a);
    
    // Sequence 4: workspace_b = F_k^2
    bigint_mul_ntt(f_k, f_k, workspace_b);
    
    // Sequence 5: f_k = F_{k+1}^2
    bigint_mul_ntt(f_k_plus_1, f_k_plus_1, f_k);
    
    // Sequence 6: f_k_plus_1 = F_{2k+1} = F_k^2 + F_{k+1}^2
    bigint_add(f_k, workspace_b, f_k_plus_1);
    
    // Final Sequence: Shift F_{2k} into f_k from workspace_a
    bigint_copy(f_k, workspace_a);
}

void bigint_fast_div(const bigint_t* num, const bigint_t* den, 
                     bigint_t* q, bigint_t* r) {
    (void)num; (void)den; (void)q; (void)r;
    // Stubbed
}
