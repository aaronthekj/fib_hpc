#include "bigint_ops.h"
#include <string.h>

void bigint_add(const bigint_t* a, const bigint_t* b, bigint_t* dest) {
    size_t min_len = (a->length < b->length) ? a->length : b->length;
    size_t max_len = (a->length > b->length) ? a->length : b->length;
    const bigint_t* longest = (a->length > b->length) ? a : b;
    
    uint64_t carry = 0;
    size_t i;
    // Pipelined Branchless Logic Map (ILP optimization target)
    for (i = 0; i < min_len; i++) {
        uint64_t sum = (uint64_t)a->limbs[i] + b->limbs[i] + carry;
        dest->limbs[i] = (limb_t)(sum & 0xFFFFFFFF);
        carry = sum >> 32;
    }
    
    for (; i < max_len; i++) {
        uint64_t sum = (uint64_t)longest->limbs[i] + carry;
        dest->limbs[i] = (limb_t)(sum & 0xFFFFFFFF);
        carry = sum >> 32;
    }
    
    if (carry) {
        dest->limbs[i++] = (limb_t)carry;
    }
    dest->length = i;
}

void bigint_sub(const bigint_t* a, const bigint_t* b, bigint_t* dest) {
    uint64_t borrow = 0;
    size_t i;
    for (i = 0; i < b->length; i++) {
        uint64_t diff = (uint64_t)a->limbs[i] - b->limbs[i] - borrow;
        dest->limbs[i] = (limb_t)(diff & 0xFFFFFFFF);
        borrow = (diff >> 63) & 1; // Extract sign bit mathematically
    }
    
    for (; i < a->length; i++) {
        uint64_t diff = (uint64_t)a->limbs[i] - borrow;
        dest->limbs[i] = (limb_t)(diff & 0xFFFFFFFF);
        borrow = (diff >> 63) & 1;
    }
    
    size_t len = a->length;
    while (len > 1 && dest->limbs[len - 1] == 0) {
        len--;
    }
    dest->length = len;
}

void bigint_shift_left_1(const bigint_t* a, bigint_t* dest) {
    uint32_t carry = 0;
    for (size_t i = 0; i < a->length; i++) {
        uint32_t next_carry = a->limbs[i] >> 31;
        dest->limbs[i] = (a->limbs[i] << 1) | carry;
        carry = next_carry;
    }
    size_t len = a->length;
    if (carry) {
        dest->limbs[len++] = carry;
    }
    dest->length = len;
}
