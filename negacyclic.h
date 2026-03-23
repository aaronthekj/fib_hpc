#pragma once
#include "config.h"
#include "formatting.h"
#include "solinas.h"

// Forward NTT Transform within the Negacyclic Ring Z/(2^n + 1)Z
// Input array 'a' is modified in-place using branchless Solinas shifts.
void ntt_negacyclic_forward(uint64_t* a, size_t n);

// Inverse NTT Transform within the Negacyclic Ring
// Input array 'a' is modified in-place and divided by N^{-1} mod P
void ntt_negacyclic_inverse(uint64_t* a, size_t n);

// Sub-quadratic Schönhage-Strassen Multiplier Entry Point
// Computes dest = A * B analytically in the mathematically pure Modulo Ring
// Bypasses Double-Precision floating point completely to prevent Mantissa scale issues.
void bigint_mul_ntt(const bigint_t* a, const bigint_t* b, bigint_t* dest);
