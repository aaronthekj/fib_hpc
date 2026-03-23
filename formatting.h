#pragma once

#include "config.h"

// Define a BigInt as a pointer to an array of limbs and its length
typedef struct {
    limb_t* limbs;
    size_t length;
    size_t capacity;
} bigint_t;

// Pre-computes the large powers of 10 array necessary for Base-10 conversion
void init_power_tables();

// Recursively converts a binary BigInt to a decimal string
char* bigint_to_decimal_string(const bigint_t* num);

// Extremely fast hex string generation fallback
char* bigint_to_hex_string(const bigint_t* num);
