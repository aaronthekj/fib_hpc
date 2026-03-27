#ifndef VERIFY_H
#define VERIFY_H

#include "bigint.h"

#define M61 ((1ULL << 61) - 1)
#define CANARY 0xDEADBEEFC0DEFACEULL

typedef struct {
    uint64_t f_n;
    uint64_t f_n1;
} shadow_state_t;

void verify_l1_lucas(ConstLimbView f_n, ConstLimbView f_n1, ConstLimbView f_2n);
void verify_l2_frequency(ConstLimbView ntt_data, int m_idx);
void verify_l3_double_prime(ConstLimbView result);
void shadow_step(shadow_state_t* st, int bit);
#endif
