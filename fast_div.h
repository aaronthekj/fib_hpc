#pragma once
#include "math_engine.h"

// Computes the inverse of a BigInt D using Newton-Raphson iteration.
// R_{i+1} = R_i * (2 - D * R_i)
// The result is scaled by an appropriate power of 2 (Base-2^32 shift)
void bigint_invert_newton(const bigint_t* d, bigint_t* inv_out);

// Executes $O(N \log N)$ Fast Division: Q = floor(N / D), R = N - Q*D
// Utilizes the pre-calculated Newton inverse of D.
void bigint_fast_div_newton(const bigint_t* n, const bigint_t* d, 
                            const bigint_t* inv_d, bigint_t* q, bigint_t* r);
