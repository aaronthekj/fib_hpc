#pragma once
#include "config.h"
#include "formatting.h"

// Zero-Allocation BigInt Arithmetic Primitives for the Fast Doubling Sequence

// Pointer-aliased signatures locked strictly to CPU alignment guidelines
// C = A + B
void bigint_add(const bigint_t* a, const bigint_t* b, bigint_t* dest);

// C = A - B
// REQUIRES: A >= B exactly.
void bigint_sub(const bigint_t* a, const bigint_t* b, bigint_t* dest);

// Highly optimized C = A * 2 (or base shift)
void bigint_shift_left_1(const bigint_t* a, bigint_t* dest);

// Fast BigInt Copy bypassing recursive evaluation
static inline void bigint_copy(bigint_t* dest, const bigint_t* src) {
    dest->length = src->length;
    // Assuming sufficient buffer capacity governed by Ping-Pong architecture limits
    for (size_t i = 0; i < src->length; i++) {
        dest->limbs[i] = src->limbs[i];
    }
}
