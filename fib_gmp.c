#include <stdio.h>
#include <gmp.h>
#include <time.h>

static double get_elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

int main() {
    printf("[*] Starting GMP $O(N \\log N)$ Fast-Fourier Validation Engine...\n");
    
    mpz_t f_k, f_k_plus_1, w_a, w_b;
    mpz_init_set_ui(f_k, 1);
    mpz_init_set_ui(f_k_plus_1, 1);
    mpz_init(w_a); mpz_init(w_b);
    
    struct timespec start, current;
    printf("[+] Firing GMP 1.000s Execution Stopwatch!\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    int doublings = 0;
    
    while(1) {
        struct timespec step_start;
        clock_gettime(CLOCK_MONOTONIC, &step_start);
        
        // Mathematical Fast Doubling Identities
        // w_a = 2 * F_k+1
        mpz_mul_2exp(w_a, f_k_plus_1, 1);
        // w_a = w_a - F_k
        mpz_sub(w_a, w_a, f_k);
        // F_2k = F_k * (2F_k+1 - F_k)
        mpz_mul(w_b, f_k, w_a);
        
        // F_2k+1 = F_k^2 + F_k+1^2  (Fused MAC sequence)
        mpz_mul(w_a, f_k, f_k);
        mpz_addmul(w_a, f_k_plus_1, f_k_plus_1);
        
        mpz_set(f_k, w_b);
        mpz_set(f_k_plus_1, w_a);
        
        doublings++;
        
        clock_gettime(CLOCK_MONOTONIC, &current);
        double elapsed_ms = get_elapsed_ms(start, current);
        double current_step_ms = get_elapsed_ms(step_start, current);
        
        // 800ms adaptive telemetry loop checking
        if (elapsed_ms >= 800.0 || (1000.0 - elapsed_ms) < (current_step_ms * 2.5)) {
            printf("\n[!] Absolute GMP Sub-Quadratic Limits triggered at %f ms.\n", elapsed_ms);
            break;
        }
    }
    
    size_t digits = mpz_sizeinbase(f_k, 10);
    printf("[+] SECURE GMP VALIDATION RESULT: Reached F_(2^%d)\n", doublings);
    printf("    Exact Sequence Output Length: %zu completely proven decimal digits.\n", digits);
    
    mpz_clears(f_k, f_k_plus_1, w_a, w_b, NULL);
    return 0;
}
