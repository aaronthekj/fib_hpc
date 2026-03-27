#include <stdio.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <gmp.h>
#pragma GCC diagnostic pop
#include <stdint.h>
#include <stdlib.h>

/* Phase 2: C Metaprogramming Pipeline
 * This tool pre-computes primitive roots and NTT twiddles using GMP.
 */

int main() {
    FILE* f = fopen("fib_hpc_constants.h", "w");
    if (!f) return 1;

    fprintf(f, "#ifndef FIB_HPC_CONSTANTS_H\n#define FIB_HPC_CONSTANTS_H\n\n");
    fprintf(f, "#include <stdint.h>\n\n");

    /* Define Primes */
    const char* primes[] = {
        "18446744069414584321", // 2^64 - 2^32 + 1 (Solinas)
        "18446743893320957953", // 2^64 - 2^32 * 41 + 1
        "18446743764471939073", // 2^64 - 2^32 * 71 + 1
        "18446743687162527745"  // 2^64 - 2^32 * 89 + 1
    };

    fprintf(f, "typedef struct {\n    uint64_t P;\n    uint64_t root;\n    uint64_t ni;\n    uint64_t r2;\n} ntt_prime_t;\n\n");
    fprintf(f, "static const ntt_prime_t NTT_PRIMES[4] = {\n");

    mpz_t p, g, r, t, one, r2;
    mpz_inits(p, g, r, t, one, r2, NULL);
    mpz_set_ui(one, 1);

    for (int i = 0; i < 4; i++) {
        mpz_set_str(p, primes[i], 10);
        
        /* Find primitive root g such that g^((p-1)/2) != 1 mod p */
        mpz_set_ui(g, 2);
        mpz_sub_ui(t, p, 1);
        mpz_fdiv_q_ui(t, t, 2);
        while (1) {
            mpz_powm(r, g, t, p);
            if (mpz_cmp(r, one) != 0) break;
            mpz_add_ui(g, g, 1);
        }

        /* Calculate ni = -p^-1 mod 2^64 */
        /* r2 = (2^64)^2 mod p */
        mpz_set_ui(t, 1);
        mpz_mul_2exp(t, t, 64);
        mpz_invert(r, p, t);
        mpz_sub(r, t, r);
        uint64_t ni = mpz_get_ui(r);

        mpz_set_ui(t, 1);
        mpz_mul_2exp(t, t, 128);
        mpz_mod(r2, t, p);
        uint64_t r2_val = mpz_get_ui(r2);

        fprintf(f, "    {0x%sULL, %llu, 0x%llxULL, 0x%llxULL}%s\n", 
                mpz_get_str(NULL, 16, p), (unsigned long long)mpz_get_ui(g), (unsigned long long)ni, (unsigned long long)r2_val, (i < 3 ? "," : ""));
    }

    fprintf(f, "};\n\n#endif\n");
    fclose(f);

    mpz_clears(p, g, r, t, one, r2, NULL);
    return 0;
}
