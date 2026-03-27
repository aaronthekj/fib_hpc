#include <omp.h>
#include "bigint.h"
#include "arena.h"
#include "error.h"
#include "constants.h"
#include "verify.h"
#include "aal.h"
#include "ssa.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void bigint_mul_ntt_fallback(LimbView* res, ConstLimbView a, ConstLimbView b, arena_t* arena, fib_ctx_t* ctx);
void bigint_mul_scalar(LimbView* res, ConstLimbView a, ConstLimbView b);

LimbView view_create(uint64_t* ptr, size_t len) {
    if (!is_little_endian()) PANIC("HPC Engine requires Little-Endian architecture.");
    return (LimbView){.ptr = ptr, .len = len};
}

static inline void bigint_trim_scoped(LimbView* v) {
    while (v->len > 1 && v->ptr[v->len - 1] == 0) v->len--;
}


void bigint_add_scoped(LimbView* res, ConstLimbView a, ConstLimbView b) {
    uint64_t carry = 0;
    size_t limbs = (a.len > b.len) ? a.len : b.len;
    size_t capacity = res->len;
    size_t final_len = 0;
    for (size_t i = 0; i < limbs || carry; i++) {
        if (i >= capacity) {
            if (carry) {
                LOG_ERROR("BigInt Addition Overflow: i=%zu, capacity=%zu, a.len=%zu, b.len=%zu, carry=%llu", i, capacity, a.len, b.len, (unsigned long long)carry);
                PANIC("BigInt Addition Overflow");
            }
            break;
        }
        uint64_t v_a = (i < a.len) ? a.ptr[i] : 0;
        uint64_t v_b = (i < b.len) ? b.ptr[i] : 0;
        u128 sum = (u128)v_a + v_b + carry;
        res->ptr[i] = (uint64_t)sum;
        carry = (uint64_t)(sum >> 64);
        final_len = i + 1;
    }
    res->len = final_len;
    bigint_trim_scoped(res);
}

void bigint_sub_scoped(LimbView* res, ConstLimbView a, ConstLimbView b) {
    uint64_t borrow = 0;
    size_t capacity = res->len;
    if (capacity < a.len) PANIC("BigInt Subtraction Target Capacity Failure");
    for (size_t i = 0; i < a.len; i++) {
        uint64_t v_a = a.ptr[i];
        uint64_t v_b = (i < b.len) ? b.ptr[i] : 0;
        u128 sub = (u128)v_a - v_b - borrow;
        res->ptr[i] = (uint64_t)sub;
        borrow = (sub >> 64) ? 1 : 0;
    }
    res->len = a.len;
    if (borrow) PANIC("BigInt Subtraction Underflow");
    bigint_trim_scoped(res);
}

uint64_t mod_pow(uint64_t base, uint64_t exp, uint64_t P) {
    uint64_t res = 1; base %= P;
    while (exp > 0) {
        if (exp % 2 == 1) res = (uint64_t)(((u128)res * base) % P);
        base = (uint64_t)(((u128)base * base) % P);
        exp /= 2;
    }
    return res;
}

void ntt_precompute_twiddles(fib_ctx_t* ctx, size_t n) {
    if (ctx->twiddle_size >= n) return;
    for (int m = 0; m < 4; m++) {
        if (ctx->twiddles[m]) free(ctx->twiddles[m]);
        ctx->twiddles[m] = aligned_alloc(64, n * sizeof(uint64_t));
        
        uint64_t P = NTT_PRIMES[m].P;
        uint64_t ni = NTT_PRIMES[m].ni;
        uint64_t r2 = NTT_PRIMES[m].r2;
        uint64_t g = NTT_PRIMES[m].root;
        uint64_t w_len = mod_pow(g, (P - 1) / n, P);
        
        uint64_t curr = 1;
        for (size_t i = 0; i < n; i++) {
            /* Store in Montgomery form: curr * R mod P */
            ctx->twiddles[m][i] = aal_mul_mont(curr, r2, P, ni);
            curr = (uint64_t)(((u128)curr * w_len) % P);
        }
    }
    ctx->twiddle_size = n;
}

void ntt_stable_scoped(LimbView v, int inv, fib_ctx_t* ctx, int m_idx) {
    size_t n = v.len;
    uint64_t P = NTT_PRIMES[m_idx].P;
    uint64_t ni = NTT_PRIMES[m_idx].ni;
    for (size_t i = 1; i < n; i++) {
        size_t j = 0;
        for (size_t temp = i, bits = n >> 1; bits > 0; temp >>= 1, bits >>= 1) {
            j = (j << 1) | (temp & 1);
        }
        if (i < j) { uint64_t t = v.ptr[i]; v.ptr[i] = v.ptr[j]; v.ptr[j] = t; }
    }

    size_t twiddle_step = ctx->twiddle_size / n;
    
    for (size_t len = 2; len <= n; len <<= 1) {
        size_t gap = n / len;
        
        #pragma omp parallel for schedule(static) if(!omp_in_parallel())
        for (size_t i = 0; i < n; i += len) {
            for (size_t j = 0; j < len / 2; j++) {
                size_t twiddle_idx = j * gap * twiddle_step;
                uint64_t w = ctx->twiddles[m_idx][twiddle_idx];
                if (inv && twiddle_idx != 0) {
                    /* w_inv = twiddles[n - twiddle_idx] */
                    w = ctx->twiddles[m_idx][(n - twiddle_idx) * twiddle_step];
                }
                aal_butterfly_ct_mont(&v.ptr[i + j], &v.ptr[i + j + len / 2], w, P, ni);
            }
        }
    }
    
    if (inv) {
        uint64_t n_inv = mod_pow(n, (uint64_t)P - 2, P);
        uint64_t n_inv_mont = aal_mul_mont(n_inv, NTT_PRIMES[m_idx].r2, P, ni);
        for (size_t i = 0; i < n; i++) {
            v.ptr[i] = aal_mul_mont(v.ptr[i], n_inv_mont, P, ni);
        }
    }
}

static void add_256(uint64_t out[4], uint64_t high, uint64_t low, int shift) {
    uint64_t carry = 0;
    u128 s0 = (u128)out[shift] + low;
    out[shift] = (uint64_t)s0;
    carry = (uint64_t)(s0 >> 64);
    if (shift + 1 < 4) {
        u128 s1 = (u128)out[shift+1] + high + carry;
        out[shift+1] = (uint64_t)s1;
        carry = (uint64_t)(s1 >> 64);
    }
    for (int i = shift + 2; i < 4 && carry; i++) {
        u128 s = (u128)out[i] + carry;
        out[i] = (uint64_t)s;
        carry = (uint64_t)(s >> 64);
    }
}

void garner_4p(uint64_t out[4], uint64_t r[4]) {
    memset(out, 0, 4 * sizeof(uint64_t));
    uint64_t v0 = r[0];
    u128 t1 = (r[1] >= v0) ? (r[1] - v0) : (r[1] + NTT_PRIMES[1].P - v0);
    uint64_t v1 = (uint64_t)((t1 * INV_0_1) % NTT_PRIMES[1].P);
    
    u128 t2_red = (r[2] >= v0 % NTT_PRIMES[2].P) ? (r[2] - v0 % NTT_PRIMES[2].P) : (r[2] + NTT_PRIMES[2].P - v0 % NTT_PRIMES[2].P);
    u128 t2_sub = (u128)v1 * (NTT_PRIMES[0].P % NTT_PRIMES[2].P) % NTT_PRIMES[2].P;
    u128 t2 = (t2_red >= t2_sub) ? (t2_red - t2_sub) : (t2_red + NTT_PRIMES[2].P - t2_sub);
    uint64_t v2 = (uint64_t)((t2 * INV_01_2) % NTT_PRIMES[2].P);
    
    u128 p01_mod3 = (u128)NTT_PRIMES[0].P % NTT_PRIMES[3].P * (NTT_PRIMES[1].P % NTT_PRIMES[3].P) % NTT_PRIMES[3].P;
    u128 acc2_red3 = (v0 % NTT_PRIMES[3].P + (u128)v1 % NTT_PRIMES[3].P * (NTT_PRIMES[0].P % NTT_PRIMES[3].P) % NTT_PRIMES[3].P + (u128)v2 % NTT_PRIMES[3].P * p01_mod3 % NTT_PRIMES[3].P) % NTT_PRIMES[3].P;
    u128 t3 = (r[3] >= acc2_red3) ? (r[3] - acc2_red3) : (r[3] + NTT_PRIMES[3].P - acc2_red3);
    uint64_t v3 = (uint64_t)((t3 * INV_012_3) % NTT_PRIMES[3].P);

    /* Assembly: X = v0 + v1*P0 + v2*P0*P1 + v3*P0*P1*P2 */
    out[0] = v0;
    u128 term1 = (u128)v1 * NTT_PRIMES[0].P;
    add_256(out, (uint64_t)(term1 >> 64), (uint64_t)term1, 0);
    
    u128 p01 = (u128)NTT_PRIMES[0].P * NTT_PRIMES[1].P; /* Approx 2^{128} */
    u128 term2_low = (u128)v2 * (uint64_t)p01;
    u128 term2_high = (u128)v2 * (uint64_t)(p01 >> 64);
    add_256(out, (uint64_t)(term2_low >> 64), (uint64_t)term2_low, 0);
    add_256(out, (uint64_t)(term2_high >> 64), (uint64_t)term2_high, 1);
    
    /* v3 * P0*P1*P2. P0*P1 is p01 (128-bit). P2 is ~2^64. */
    /* p012 = p01 * P2 (192-bit) */
    u128 p012_low = (u128)(uint64_t)p01 * NTT_PRIMES[2].P;
    u128 p012_high = (u128)(p01 >> 64) * NTT_PRIMES[2].P + (p012_low >> 64);
    
    /* term3 = v3 * (p012_high, p012_low) */
    u128 t3_l = (u128)v3 * (uint64_t)p012_low;
    u128 t3_m = (u128)v3 * (uint64_t)p012_high;
    u128 t3_h = (u128)v3 * (p012_high >> 64);
    
    add_256(out, (uint64_t)(t3_l >> 64), (uint64_t)t3_l, 0);
    add_256(out, (uint64_t)(t3_m >> 64), (uint64_t)t3_m, 1);
    add_256(out, (uint64_t)(t3_h >> 64), (uint64_t)t3_h, 2);
}

static double get_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

void bigint_mul_scalar(LimbView* res, ConstLimbView a, ConstLimbView b) {
    size_t capacity = res->len;
    memset(res->ptr, 0, capacity * sizeof(uint64_t));
    size_t final_len = 0;
    for (size_t i = 0; i < a.len; i++) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.len; j++) {
            if (i + j >= capacity) break;
            u128 prod = (u128)a.ptr[i] * b.ptr[j] + res->ptr[i + j] + carry;
            res->ptr[i + j] = (uint64_t)prod;
            carry = (uint64_t)(prod >> 64);
            if (i + j + 1 > final_len) final_len = i + j + 1;
        }
        if (i + b.len < capacity && carry) {
            res->ptr[i + b.len] += carry;
            if (i + b.len + 1 > final_len) final_len = i + b.len + 1;
        }
    }
    res->len = final_len;
    bigint_trim_scoped(res);
}

void bigint_mul_scoped(LimbView* res, ConstLimbView a, ConstLimbView b, arena_t* arena, fib_ctx_t* ctx) {
    if (a.len < 1024 || b.len < 1024) {
        bigint_mul_scalar(res, a, b);
    } else {
        bigint_mul_ntt_fallback(res, a, b, arena, ctx);
    }
    bigint_trim_scoped(res);
}

void bigint_mul_ntt_fallback(LimbView* res, ConstLimbView a, ConstLimbView b, arena_t* arena, fib_ctx_t* ctx) {
    memset(res->ptr, 0, res->len * sizeof(uint64_t));
    if (a.len == 0 || b.len == 0) return;
    if (a.len + b.len > 10000) {
        bigint_mul_ssa(res, a, b, arena);
        return;
    }
    size_t n = 1;
    while (n < a.len + b.len) n <<= 1;
    if (n < 8) n = 8;
    
    ntt_precompute_twiddles(ctx, n);
    
    LimbView residues[4];
    double t_ntt_total = 0;
    double t_conv_total = 0;

    #pragma omp parallel for schedule(static, 1) reduction(+:t_ntt_total,t_conv_total)
    for (int m = 0; m < 4; m++) {
        uint64_t P = NTT_PRIMES[m].P;
        uint64_t ni = NTT_PRIMES[m].ni;
        uint64_t r2 = NTT_PRIMES[m].r2;

        residues[m] = arena_alloc_limbs(arena, n);
        LimbView t_a = arena_alloc_limbs(arena, n);
        LimbView t_b = arena_alloc_limbs(arena, n);
        
        double tc1_s = get_ms();
        for (size_t i = 0; i < n; i++) {
            uint64_t val_a = (i < a.len) ? a.ptr[i] % P : 0;
            uint64_t val_b = (i < b.len) ? b.ptr[i] % P : 0;
            t_a.ptr[i] = aal_mul_mont(val_a, r2, P, ni);
            t_b.ptr[i] = aal_mul_mont(val_b, r2, P, ni);
        }
        t_conv_total += (get_ms() - tc1_s);
        
        double tn1_s = get_ms();
        ntt_stable_scoped(t_a, 0, ctx, (int)m);
        ntt_stable_scoped(t_b, 0, ctx, (int)m);
        
        for (size_t i = 0; i < n; i++) {
            t_a.ptr[i] = aal_mul_mont(t_a.ptr[i], t_b.ptr[i], P, ni);
        }
        
        ntt_stable_scoped(t_a, 1, ctx, m);
        t_ntt_total += (get_ms() - tn1_s);
        
        double tc2_s = get_ms();
        for (size_t i = 0; i < n; i++) {
            t_a.ptr[i] = aal_mul_mont(t_a.ptr[i], 1, P, ni);
        }
        t_conv_total += (get_ms() - tc2_s);
        
        if (!(ctx->flags & 0x1)) verify_l2_frequency(to_const(t_a), (int)m);
        memcpy(residues[m].ptr, t_a.ptr, n * sizeof(uint64_t));
    }

    double t_garner_s = get_ms();
    int num_threads = omp_get_max_threads();
    uint64_t** local_bufs = malloc((size_t)num_threads * sizeof(uint64_t*));
    for (int i = 0; i < num_threads; i++) {
        local_bufs[i] = calloc(n + 4, sizeof(uint64_t));
        if (!local_bufs[i]) PANIC("CRT Local Buffer Allocation Failed");
    }

    #pragma omp parallel
    {
        int tid = (int)omp_get_thread_num();
        if (tid >= num_threads) PANIC("Unexpected OpenMP Thread ID %d", tid);
        uint64_t* my_buf = local_bufs[tid];
        #pragma omp for schedule(static)
        for (size_t i = 0; i < n; i++) {
            uint64_t rr[4] = {residues[0].ptr[i], residues[1].ptr[i], residues[2].ptr[i], residues[3].ptr[i]};
            uint64_t out[4];
            garner_4p(out, rr);
            
            uint64_t c = 0;
            for (int k_idx = 0; k_idx < 4; k_idx++) {
                u128 sum = (u128)my_buf[i + (size_t)k_idx] + out[k_idx] + c;
                my_buf[i + (size_t)k_idx] = (uint64_t)sum;
                c = (uint64_t)(sum >> 64);
            }
            for (size_t k_idx = i + 4; k_idx < n + 4 && c; k_idx++) {
                u128 sum = (u128)my_buf[k_idx] + c;
                my_buf[k_idx] = (uint64_t)sum;
                c = (uint64_t)(sum >> 64);
            }
        }
    }

    uint64_t ripple = 0;
    for (size_t i = 0; i < res->len; i++) {
        u128 col_sum = ripple;
        for (int t = 0; t < num_threads; t++) {
            if (i < n + 4) col_sum += local_bufs[t][i];
        }
        res->ptr[i] = (uint64_t)col_sum;
        ripple = (uint64_t)(col_sum >> 64);
    }
    
    double t_garner_e = get_ms();
    if (ctx->flags & 0x2) {
        printf("[PROFILE] Step: %zu bits x %zu bits -> n=%zu. Garnering Took %.2f ms\n", a.len*64, b.len*64, n, t_garner_e - t_garner_s);
    }
    
    for (int t = 0; t < num_threads; t++) free(local_bufs[t]);
    free(local_bufs);
    
    // Theoretical max product length: a.len + b.len
    if (res->len < a.len + b.len) {
        fprintf(stderr, "[ERROR] res->len (%zu) is smaller than required (a.len=%zu + b.len=%zu)\n", res->len, a.len, b.len);
        PANIC("BigInt Multiplication Target Capacity Failure");
    }
    bigint_trim_scoped(res);
}
int fib_race(fib_ctx_t* ctx, unsigned long long N, bigint_t* res, fib_stats_t* stats) {
    (void)res; (void)stats;
    size_t cap = (N/32) + 16384;
    
    arena_t* arena_scratch = arena_create(16384ULL * 1024 * 1024);
    arena_t* arena_ping = arena_create(4096ULL * 1024 * 1024);
    arena_t* arena_pong = arena_create(4096ULL * 1024 * 1024);
    
    LimbView f_n = arena_alloc_limbs(arena_ping, 1);
    f_n.ptr[0] = 0; f_n.len = 1;
    LimbView f_n1 = arena_alloc_limbs(arena_ping, 1);
    f_n1.ptr[0] = 1; f_n1.len = 1;

    size_t k = (N == 0) ? 0 : 64 - (size_t)__builtin_clzll(N);
    for (int i_idx = (int)k - 1; i_idx >= 0; i_idx--) {
        arena_t* next_state = (i_idx % 2 == 0) ? arena_pong : arena_ping;
        arena_t* step_scratch = arena_create(1024ULL * 1024 * 1024);
        step_scratch->offset = 0;
        
        LimbView f_2n = arena_alloc_limbs(step_scratch, cap);
        LimbView f_2n1 = arena_alloc_limbs(step_scratch, cap);
        LimbView inner = arena_alloc_limbs(step_scratch, cap);
        LimbView two_f_n1 = arena_alloc_limbs(step_scratch, cap);
        
        bigint_add_scoped(&two_f_n1, to_const(f_n1), to_const(f_n1));
        bigint_sub_scoped(&inner, to_const(two_f_n1), to_const(f_n));
        
        /* Thresholding Dispatch implicitly handled in bigint_mul_scoped */
        bigint_mul_scoped(&f_2n, to_const(f_n), to_const(inner), step_scratch, ctx);
        
        LimbView f_n1_sq = arena_alloc_limbs(step_scratch, cap);
        LimbView f_n_sq = arena_alloc_limbs(step_scratch, cap);
        bigint_mul_scoped(&f_n1_sq, to_const(f_n1), to_const(f_n1), step_scratch, ctx);
        bigint_mul_scoped(&f_n_sq, to_const(f_n), to_const(f_n), step_scratch, ctx);
        
        bigint_add_scoped(&f_2n1, to_const(f_n1_sq), to_const(f_n_sq));
        
        /* L1 Lucas Check using the values that generated f_2n */
        if (!(ctx->flags & 0x1)) verify_l1_lucas(to_const(f_n), to_const(f_n1), to_const(f_2n));

        unsigned int bit = (unsigned int)((N >> i_idx) & 1);
        if (bit) {
            /* F(2n+1) and F(2n+2) = F(2n) + F(2n+1) */
            LimbView next_f_n = arena_alloc_limbs(next_state, f_2n1.len);
            for(size_t j=0; j<f_2n1.len; j++) next_f_n.ptr[j] = f_2n1.ptr[j]; 
            
            LimbView next_f_n1 = arena_alloc_limbs(next_state, cap);
            bigint_add_scoped(&next_f_n1, to_const(f_2n), to_const(f_2n1));
            f_n = next_f_n; f_n1 = next_f_n1;
        } else {
            LimbView next_f_n = arena_alloc_limbs(next_state, f_2n.len);
            for(size_t j=0; j<f_2n.len; j++) next_f_n.ptr[j] = f_2n.ptr[j];
            
            LimbView next_f_n1 = arena_alloc_limbs(next_state, f_2n1.len);
            for(size_t j=0; j<f_2n1.len; j++) next_f_n1.ptr[j] = f_2n1.ptr[j];
            f_n = next_f_n; f_n1 = next_f_n1;
        }

        arena_destroy(step_scratch);
    }
    arena_destroy(arena_scratch);
    arena_destroy(arena_ping);
    arena_destroy(arena_pong);
    return 0;
}

static void* worker_loop(void* arg) {
    fib_ctx_t* ctx = (fib_ctx_t*)arg;
    while (1) {
        pthread_mutex_lock(&ctx->lock);
        while (ctx->q_count == 0 && !ctx->shut) pthread_cond_wait(&ctx->cond, &ctx->lock);
        if (ctx->shut && ctx->q_count == 0) { pthread_mutex_unlock(&ctx->lock); break; }
        fib_task_t task = ctx->queue[ctx->q_head];
        ctx->q_head = (ctx->q_head + 1) % ctx->q_cap;
        ctx->q_count--;
        ctx->running_count++;
        pthread_mutex_unlock(&ctx->lock);
        
        task.func(task.arg);
        
        pthread_mutex_lock(&ctx->lock);
        ctx->running_count--;
        if (ctx->q_count == 0 && ctx->running_count == 0) pthread_cond_broadcast(&ctx->idle);
        pthread_mutex_unlock(&ctx->lock);
    }
    return NULL;
}

fib_ctx_t* fib_ctx_create(size_t n_max) {
    fib_ctx_t* ctx = calloc(1, sizeof(fib_ctx_t));
    ctx->num_workers = n_max;
    ctx->q_cap = 1024;
    ctx->queue = malloc(sizeof(fib_task_t) * ctx->q_cap);
    ctx->workers = malloc(sizeof(pthread_t) * n_max);
    ctx->running_count = 0;
    pthread_mutex_init(&ctx->lock, NULL);
    pthread_cond_init(&ctx->cond, NULL);
    pthread_cond_init(&ctx->idle, NULL);
    pthread_mutex_init(&ctx->tel.mtx, NULL);
    ctx->twiddle_size = 0;
    for (int i = 0; i < 4; i++) ctx->twiddles[i] = NULL;
    ctx->flags = 0;
    for (size_t i = 0; i < n_max; i++) pthread_create(&ctx->workers[i], NULL, worker_loop, ctx);
    return ctx;
}

void fib_ctx_dispatch(fib_ctx_t* ctx, void (*f)(void*), void* arg) {
    pthread_mutex_lock(&ctx->lock);
    ctx->queue[ctx->q_tail] = (fib_task_t){f, arg};
    ctx->q_tail = (ctx->q_tail + 1) % ctx->q_cap;
    ctx->q_count++;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
}

void fib_ctx_wait(fib_ctx_t* ctx) {
    pthread_mutex_lock(&ctx->lock);
    while (ctx->q_count > 0 || ctx->running_count > 0) pthread_cond_wait(&ctx->idle, &ctx->lock);
    pthread_mutex_unlock(&ctx->lock);
}

void fib_ctx_destroy(fib_ctx_t* ctx) {
    if (!ctx) return;
    fib_ctx_wait(ctx);
    pthread_mutex_lock(&ctx->lock);
    ctx->shut = 1;
    pthread_cond_broadcast(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
    for (size_t i = 0; i < ctx->num_workers; i++) pthread_join(ctx->workers[i], NULL);
    free(ctx->workers); free(ctx->queue);
    pthread_mutex_destroy(&ctx->lock);
    pthread_cond_destroy(&ctx->cond);
    pthread_cond_destroy(&ctx->idle);
    free(ctx);
}

void bigint_mul_ssa(LimbView* res, ConstLimbView a, ConstLimbView b, arena_t* q) {
    memset(res->ptr, 0, res->len * 8);
    size_t total_bits = (a.len + b.len) * 64 + 128;
    /* We need K pieces such that the linear convolution (length 2*PieceCount) fits in K.
     * So PieceCount <= K/2.
     * With piece size L, we have PieceCount = total_bits / 2 / L. 
     * Let's pick K such that L is reasonable. */
    size_t K = 512;
    while (K * K < total_bits * 32) K *= 2;
    
    /* Input pieces: at most K/2. */
    size_t input_bits = (a.len > b.len ? a.len : b.len) * 64;
    size_t L = (input_bits + (K/2) - 1) / (K/2);
    if (L < 64) L = 64;
    
    size_t logK = 0; while((1ULL << logK) < K) logK++;
    size_t Nb_min = 2 * L + logK + 64; 
    size_t alignment = K / 2;
    if (alignment < 64) alignment = 64;
    size_t Nb = ((Nb_min + alignment - 1) / alignment) * alignment;
    size_t nrl = Nb / 64;
    
    ssa_val_t* da = arena_alloc(q, K * sizeof(ssa_val_t));
    ssa_val_t* db = arena_alloc(q, K * sizeof(ssa_val_t));
    uint64_t* flat_a = arena_alloc(q, K * nrl * 8);
    uint64_t* flat_b = arena_alloc(q, K * nrl * 8);
    memset(flat_a, 0, K * nrl * 8);
    memset(flat_b, 0, K * nrl * 8);
    
    LOG_INFO("SSA: total_bits=%zu, K=%zu, L=%zu, Nb=%zu, nrl=%zu", total_bits, K, L, Nb, nrl);
    for (size_t i = 0; i < K; i++) {
        da[i].ptr = flat_a + i * nrl; da[i].size_limbs = nrl; da[i].extra = 0;
        db[i].ptr = flat_b + i * nrl; db[i].size_limbs = nrl; db[i].extra = 0;
    }

    /* Fused Extraction & FFT Pass 0 (DIF) */
    size_t h_top = K / 2;
    size_t step_top = (2 * Nb) / K;
    #pragma omp parallel
    {
        uint64_t* tp = (uint64_t*)malloc(nrl * 8);
        ssa_val_t t = {tp, nrl, 0};
        #pragma omp for schedule(dynamic, 128)
        for (size_t i = 0; i < h_top; i++) {
            /* Extract Piece i and i + h_top */
            size_t indices[2] = {i, i + h_top};
            for (int idx = 0; idx < 2; idx++) {
                size_t p_idx = indices[idx];
                size_t bit_start = p_idx * L;
                for (size_t bit_idx = 0; bit_idx < L; bit_idx += 64) {
                    size_t abs_bit = bit_start + bit_idx;
                    size_t limb = abs_bit / 64, off = abs_bit % 64;
                    size_t target_limb = bit_idx / 64;
                    if (limb < a.len) {
                        u128 val = (u128)a.ptr[limb] >> off;
                        if (off > 0 && limb + 1 < a.len) val |= ((u128)a.ptr[limb+1]) << (64 - off);
                        uint64_t mask = (L - bit_idx >= 64) ? ~0ULL : (1ULL << (L - bit_idx)) - 1;
                        da[p_idx].ptr[target_limb] = (uint64_t)val & mask;
                    } else da[p_idx].ptr[target_limb] = 0;
                    if (limb < b.len) {
                        u128 val = (u128)b.ptr[limb] >> off;
                        if (off > 0 && limb + 1 < b.len) val |= ((u128)b.ptr[limb+1]) << (64 - off);
                        uint64_t mask = (L - bit_idx >= 64) ? ~0ULL : (1ULL << (L - bit_idx)) - 1;
                        db[p_idx].ptr[target_limb] = (uint64_t)val & mask;
                    } else db[p_idx].ptr[target_limb] = 0;
                }
            }
            if (i < 2) {
                LOG_TRACE("Piece %zu: [0]=%llu, [1]=%llu", i, (unsigned long long)da[i].ptr[0], (unsigned long long)da[i].ptr[1]);
                LOG_TRACE("Piece %zu: [0]=%llu, [1]=%llu", i + h_top, (unsigned long long)da[i+h_top].ptr[0], (unsigned long long)da[i+h_top].ptr[1]);
            }
            /* Butterfly for A */
            ssa_add(&t, da[i], da[i+h_top]);
            ssa_sub(&da[i+h_top], da[i], da[i+h_top]);
            ssa_mul_2k(&da[i+h_top], da[i+h_top], i * step_top);
            memcpy(da[i].ptr, t.ptr, nrl*8); da[i].extra = t.extra;
            /* Butterfly for B */
            ssa_add(&t, db[i], db[i+h_top]);
            ssa_sub(&db[i+h_top], db[i], db[i+h_top]);
            ssa_mul_2k(&db[i+h_top], db[i+h_top], i * step_top);
            memcpy(db[i].ptr, t.ptr, nrl*8); db[i].extra = t.extra;
        }
        free(tp);
    }

    /* Continue FFT from level K/2 */
    ssa_fft_partial(da, K, Nb, K/2, 0);
    ssa_fft_partial(db, K, Nb, K/2, 0);
    ssa_bit_reverse(da, K);
    ssa_bit_reverse(db, K);
    
    LOG_TRACE("Coeff 0: da[0].ptr[0]=%llu, db[0].ptr[0]=%llu", (unsigned long long)da[0].ptr[0], (unsigned long long)db[0].ptr[0]);
    LOG_TRACE("Coeff 1: da[1].ptr[0]=%llu, db[1].ptr[0]=%llu", (unsigned long long)da[1].ptr[0], (unsigned long long)db[1].ptr[0]);

    uint64_t* flat_m = (uint64_t*)arena_alloc(q, K * 2 * nrl * 8);

    printf("[SSA] Mul Ring Start\n");
    #pragma omp parallel for
    for (size_t i = 0; i < K; i++) {
        ssa_mul_ring_buf(&da[i], da[i], db[i], flat_m + i * 2 * nrl);
    }

    printf("[SSA] FFT Inv Start\n");
    ssa_fft(da, K, Nb, 1);
    ssa_scale(da, K, Nb);
    printf("[SSA] Reconstruct Start\n");

    /* Parallel Reconstruction via Thread-Local Accumulation (O(P*N) memory) */
    #pragma omp parallel
    {
        int tid = (int)omp_get_thread_num();
        int nthreads = (int)omp_get_num_threads();
        size_t chunk = (K + (size_t)nthreads - 1) / (size_t)nthreads;
        size_t start = (size_t)tid * chunk;
        size_t end = (start + chunk > K) ? K : start + chunk;

        /* Private accumulation buffer - must be thread-safe */
        uint64_t* local_ptr = calloc(res->len, 8);
        if (!local_ptr) PANIC("Thread-local buffer allocation failed");
        LimbView local_res = {.ptr = local_ptr, .len = res->len};

        for (size_t i = start; i < end; i++) {
            size_t bit_start = i * L;
            size_t nrl_inner = da[i].size_limbs;
            for (size_t limb_idx = 0; limb_idx < nrl_inner; limb_idx++) {
                size_t abs_bit = bit_start + limb_idx * 64;
                size_t res_limb = abs_bit / 64, res_bit = abs_bit % 64;
                if (res_limb >= local_res.len) break;
                
                u128 val = ((u128)da[i].ptr[limb_idx]) << res_bit;
                /* Non-atomic local addition */
                u128 sum = (u128)local_res.ptr[res_limb] + (uint64_t)val;
                local_res.ptr[res_limb] = (uint64_t)sum;
                uint64_t carry = (uint64_t)(sum >> 64);
                
                if (res_limb + 1 < local_res.len) {
                    u128 sum2 = (u128)local_res.ptr[res_limb+1] + (uint64_t)(val >> 64) + carry;
                    local_res.ptr[res_limb+1] = (uint64_t)sum2;
                    carry = (uint64_t)(sum2 >> 64);
                    for (size_t r = res_limb + 2; carry && r < local_res.len; r++) {
                        u128 s = (u128)local_res.ptr[r] + carry;
                        local_res.ptr[r] = (uint64_t)s;
                        carry = (uint64_t)(s >> 64);
                    }
                }
            }
        }

        /* Merge into global result */
        #pragma omp critical
        {
            uint64_t ripple = 0;
            for (size_t i = 0; i < res->len; i++) {
                u128 s = (u128)res->ptr[i] + local_res.ptr[i] + ripple;
                res->ptr[i] = (uint64_t)s;
                ripple = (uint64_t)(s >> 64);
            }
        }
        free(local_ptr);
    }
}
