#ifndef FIB_HPC_CONSTANTS_H
#define FIB_HPC_CONSTANTS_H

#include <stdint.h>

typedef struct {
    uint64_t P;
    uint64_t root;
    uint64_t ni;
    uint64_t r2;
} ntt_prime_t;

static const ntt_prime_t NTT_PRIMES[4] = {
    {0xffffffff00000001ULL, 7, 0xfffffffeffffffffULL, 0xfffffffe00000001ULL},
    {0xffffffd600008001ULL, 2, 0x702a1fd5c0007fffULL, 0xffd720ea3c8f791eULL},
    {0xffffffb800008001ULL, 2, 0xf0481fb7c0007fffULL, 0xffbdb12835e16bc2ULL},
    {0xffffffa600008001ULL, 2, 0x705a1fa5c0007fffULL, 0xffb11e9a302f605eULL}
};

#endif
