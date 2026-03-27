#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>

#define MAX_THREADS 32
#define ARENA_CANARY 0xDEADBEEFCAFEBABEULL

typedef struct {
    uint64_t P;
    uint64_t root;
    uint64_t ni;
    uint64_t r2;
} ntt_prime_t;

/* Four Proth Primes for 256-bit precision (~2^256) */
static const ntt_prime_t NTT_PRIMES[4] = {
    {0xFFFFFFFF00000001ULL, 7, 0xFFFFFFFEFFFFFFFFULL, 0xFFFFFFFE00000001ULL},
    {0xFFFFFFD300000001ULL, 3, 0xFFFFFFD2FFFFFFFFULL, 0x1639AFFFFF818ULL},
    {0xFFFFFFB500000001ULL, 3, 0xFFFFFFB4FFFFFFFFULL, 0x66F5CFFFFEA08ULL},
    {0xFFFFFFA300000001ULL, 3, 0xFFFFFFA2FFFFFFFFULL, 0xC454AFFFFDE38ULL}
};

/* Inverse Constants for Garner's Algorithm (Modular Inverses) */
#define INV_0_1 0x28BA2E84745D1747ULL
#define INV_01_2 0xD07D764E2FA00EC6ULL
#define INV_012_3 0xDDF474C2A2649619ULL

#endif
