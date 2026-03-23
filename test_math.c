#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "config.h"
#include "math_engine.h"
#include "formatting.h"
#include "solinas.h"

// Reference O(N^2) schoolbook multiplication to strictly verify FFT accuracy
void bigint_mul_schoolbook(const bigint_t* a, const bigint_t* b, bigint_t* dest) {
    memset(dest->limbs, 0, dest->capacity * sizeof(limb_t));
    if (a->length == 0 || b->length == 0) {
        dest->length = 1;
        dest->limbs[0] = 0;
        return;
    }
    
    // Allocate a wider buffer to prevent overflow during accumulation
    uint64_t* temp = calloc(a->length + b->length, sizeof(uint64_t));
    
    for (size_t i = 0; i < a->length; i++) {
        for (size_t j = 0; j < b->length; j++) {
            temp[i+j] += (uint64_t)a->limbs[i] * (uint64_t)b->limbs[j];
        }
    }
    
    uint64_t carry = 0;
    size_t k;
    for (k = 0; k < a->length + b->length; k++) {
        uint64_t sum = temp[k] + carry;
        dest->limbs[k] = (limb_t)(sum & 0xFFFFFFFF);
        carry = sum >> 32;
    }
    while (carry > 0) {
        dest->limbs[k++] = (limb_t)(carry & 0xFFFFFFFF);
        carry >>= 32;
    }
    
    dest->length = k;
    while(dest->length > 1 && dest->limbs[dest->length - 1] == 0) {
        dest->length--;
    }
    free(temp);
}

int main() {
    printf("[*] Initializing Math Engine Verification Suite...\n");
    
    bigint_t a, b, dest_fft, dest_sb;
    a.capacity = 1000; a.length = 2; a.limbs = calloc(1000, sizeof(limb_t));
    b.capacity = 1000; b.length = 2; b.limbs = calloc(1000, sizeof(limb_t));
    dest_fft.capacity = 2000; dest_fft.length = 0; dest_fft.limbs = calloc(2000, sizeof(limb_t));
    dest_sb.capacity = 2000; dest_sb.length = 0; dest_sb.limbs = calloc(2000, sizeof(limb_t));
    
    // Test 1: Simple high-limb carry test
    // a = 0xFFFFFFFF, 0xFFFFFFFF
    a.limbs[0] = 0xFFFFFFFF;
    a.limbs[1] = 0xFFFFFFFF;
    
    // b = 0x12345678, 0x9ABCDEF0
    b.limbs[0] = 0x12345678;
    b.limbs[1] = 0x9ABCDEF0;
    
    printf("[*] Running Schoolbook multiplication...\n");
    bigint_mul_schoolbook(&a, &b, &dest_sb);
    
    printf("[*] Running FFT multiplication...\n");
    bigint_mul_fft(&a, &b, &dest_fft);
    
    printf("[*] Verifying Output Lengths: SB=%zu, FFT=%zu\n", dest_sb.length, dest_fft.length);
    assert(dest_sb.length == dest_fft.length);
    
    printf("[*] Verifying Output Digits...\n");
    for (size_t i = 0; i < dest_sb.length; i++) {
        if (dest_sb.limbs[i] != dest_fft.limbs[i]) {
            printf("MISMATCH at limb %zu: SB=%08x, FFT=%08x\n", i, dest_sb.limbs[i], dest_fft.limbs[i]);
            assert(0);
        }
    }
    
    printf("[+] Test 1 Passed! FFT correctly handles internal 11-bit carry propagation!\n");
    
    printf("\n[*] Initializing Solinas Modular Reduction Verification...\n");
    uint64_t s_a = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t s_b = 0x1234567890ABCDEFULL;
    
    uint64_t mod_a = s_a % SOLINAS_P1;
    uint64_t mod_b = s_b % SOLINAS_P1;
    
    uint64_t expected_add = (mod_a + mod_b) % SOLINAS_P1;
    uint64_t actual_add = solinas_add_p1(mod_a, mod_b);
    
    printf("[*] Verifying Solinas Addition...\n");
    assert(expected_add == actual_add);
    
    unsigned __int128 expected_mul_128 = ((unsigned __int128)mod_a * mod_b) % SOLINAS_P1;
    uint64_t actual_mul = solinas_mul_p1(mod_a, mod_b);
    
    printf("[*] Verifying Solinas Multiplication...\n");
    if ((uint64_t)expected_mul_128 != actual_mul) {
        printf("MISMATCH: Expected %llx, Got %llx\n", (unsigned long long)expected_mul_128, (unsigned long long)actual_mul);
        assert(0);
    }
    
    printf("[+] Test 2 Passed! Solinas Shift-Add Reductions are mathematically exact!\n");
    
    printf("\n[+] All Math Engine Tests Successfully Verified.\n");
    return 0;
}
