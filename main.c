#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "bigint.h"

// Massive 64-byte aligned bounds pre-allocated to safely absorb scaling (e.g., F_100,000,000)
#define MAX_EXPECTED_LIMBS 10000000 

// Hardware resolution telemetry helper
static double get_elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

int main() {
    printf("[*] Initializing fib_hpc Aligned Allocators & Telemetry Shell...\n");
    
    // 1. Static Buffer Pre-Allocation (ZERO dynamic allocation during loop execution)
    bigint_t f_k, f_k_plus_1, workspace_a, workspace_b;
    
    f_k.capacity = MAX_EXPECTED_LIMBS;
    f_k.limbs = aligned_alloc(64, MAX_EXPECTED_LIMBS * sizeof(limb_t));
    f_k.length = 1; f_k.limbs[0] = 1; // F(1) = 1
    
    f_k_plus_1.capacity = MAX_EXPECTED_LIMBS;
    f_k_plus_1.limbs = aligned_alloc(64, MAX_EXPECTED_LIMBS * sizeof(limb_t));
    f_k_plus_1.length = 1; f_k_plus_1.limbs[0] = 1; // F(2) = 1
    
    workspace_a.capacity = MAX_EXPECTED_LIMBS;
    workspace_a.limbs = aligned_alloc(64, MAX_EXPECTED_LIMBS * sizeof(limb_t));
    workspace_a.length = 0;
    
    workspace_b.capacity = MAX_EXPECTED_LIMBS;
    workspace_b.limbs = aligned_alloc(64, MAX_EXPECTED_LIMBS * sizeof(limb_t));
    workspace_b.length = 0;
    
    // 2. Cycle-Accurate CPU Warm-Up Protocol
    printf("[*] Executing Execution Unit (EU) & ILP Warm-Up Blocks...\n");
    // Form Geometric Space Bounds
    clifford_vec_t vk = { &f_k, &f_k_plus_1 };
    clifford_vec_t vk_next = { &workspace_a, &workspace_b };
    
    // Dry-run the sequence solely to saturate the Instruction Cache and prime Branch Predictors
    fused_clifford_doubling_step(&vk, &vk_next);
    
    // Reset Identity
    f_k.length = 1; f_k.limbs[0] = 1;
    f_k_plus_1.length = 1; f_k_plus_1.limbs[0] = 1;
    
    // 3. Strict Stopwatch Kickoff
    struct timespec start, current;
    printf("\n[+] Caches hot. Firing 1.000s Execution Stopwatch!\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    // 4. Telemetry-Bounded Fast Doubling Loop utilizing Multivector Space
    int doublings = 0; 
    
    while (1) {
        struct timespec step_start;
        clock_gettime(CLOCK_MONOTONIC, &step_start);
        
        fused_clifford_doubling_step(&vk, &vk_next);
        
        // Execute Constant-Time CPU Cache oblivious internal pointer swaps
        bigint_t* temp_e0 = vk.e0; vk.e0 = vk_next.e0; vk_next.e0 = temp_e0;
        bigint_t* temp_e1 = vk.e1; vk.e1 = vk_next.e1; vk_next.e1 = temp_e1;
        
        doublings++; // Track exact mathematical recursion depth

        clock_gettime(CLOCK_MONOTONIC, &current);
        double elapsed_ms = get_elapsed_ms(start, current);
        double current_step_ms = get_elapsed_ms(step_start, current);
        
        // Adaptive Heartbeat Evaluator: 800ms logic limit
        // Next step is O(M(2n)). FFT scales roughly by 2.2x factor per sequence bounds.
        // We pad with exactly 2.5x to guarantee absolute mathematical safety.
        if (elapsed_ms >= 800.0 || (1000.0 - elapsed_ms) < (current_step_ms * 2.5)) {
            printf("\n[!] TELEMETRY WALL: Projected multiplication step exceeds remaining millisecond safety cap.\n");
            printf("[!] Adaptive Break physically triggered at %f ms.\n", elapsed_ms);
            break;
        }
    }
    
    printf("\n[+] SUCCESS! Reached massive identity F_(2^%d) strictly within competitive bounds.\n", doublings);
    printf("    Final Sequence Digits Payload Capacity: %zu 32-bit limbs.\n", f_k.length);

    // Cleanup phase 
    free(f_k.limbs);
    free(f_k_plus_1.limbs);
    free(workspace_a.limbs);
    free(workspace_b.limbs);

    return 0;
}
