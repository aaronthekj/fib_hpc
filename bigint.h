#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_EXPECTED_LIMBS 10000000
typedef uint32_t limb_t;

typedef struct {
    uint32_t* limbs;
    size_t length;
    size_t capacity;
} bigint_t;

typedef struct { double real, imag; } complex_t;

void bigint_init(bigint_t* num, size_t capacity);
void bigint_copy(bigint_t* dest, const bigint_t* src);

void bigint_add(const bigint_t* a, const bigint_t* b, bigint_t* dest);
void bigint_sub(const bigint_t* a, const bigint_t* b, bigint_t* dest);
void bigint_shift_left_1(const bigint_t* a, bigint_t* dest);

void bigint_mul_fft(const bigint_t* a, const bigint_t* b, bigint_t* dest);
void bigint_mul_ntt(const bigint_t* a, const bigint_t* b, bigint_t* dest);

void fast_doubling_step(const bigint_t* f_k, const bigint_t* f_k_plus_1, bigint_t* workspace_a, bigint_t* workspace_b);
char* bigint_to_decimal_string(const bigint_t* num);
