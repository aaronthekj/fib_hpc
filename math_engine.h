#pragma once

#include "config.h"
#include "formatting.h"

// Phase 2: Double-Precision Complex FFT Multiplication Engine
// This is the absolute core of the 1-second limit architecture.

// Perform an extremely fast FFT-based multiplication of two BigInts
// Result is stored inside the pre-allocated dest BigInt (zero dynamic alloc)
void bigint_mul_fft(const bigint_t* a, const bigint_t* b, bigint_t* dest);

// In-place Fast Doubling Step
// Calculates F(2k) and F(2k+1) given F(k) and F(k+1)
// Uses the Ping-Pong allocator boundaries A and B
void fast_doubling_step(bigint_t* f_k, bigint_t* f_k_plus_1,
                        bigint_t* workspace_a, bigint_t* workspace_b);

// Fast Division via Newton-Raphson (used exclusively by Formatting D&C)
void bigint_fast_div(const bigint_t* num, const bigint_t* den, 
                     bigint_t* q, bigint_t* r);
