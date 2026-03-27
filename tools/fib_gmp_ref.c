#include <stdio.h>
#include <gmp.h>
#include <stdlib.h>

/* Phase 2: Golden Reference Test Vectors
 * This tool utilizes GMP to generate massive, deterministic Fibonacci numbers
 * for use in validating the HPC NTT engine.
 */

void print_fib(unsigned long n) {
    mpz_t fn;
    mpz_init(fn);
    mpz_fib_ui(fn, n);
    
    /* For now, just print the hex to stdout. 
     * In a full implementation, this should generate JSON. */
    char* s = mpz_get_str(NULL, 16, fn);
    printf("F(%lu) = 0x%s\n", n, s);
    free(s);
    mpz_clear(fn);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <n>\n", argv[0]);
        return 1;
    }
    unsigned long n = strtoul(argv[1], NULL, 10);
    print_fib(n);
    return 0;
}
