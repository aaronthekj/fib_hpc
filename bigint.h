#ifndef BIGINT_H
#define BIGINT_H

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include "common.h"
#include "constants.h"
#include "arena.h"

typedef struct {
    [[deprecated("Use LimbView + Scoped Arena instead")]] uint32_t *limbs; 
    size_t length;
    size_t capacity;
} bigint_t;

/* ... task types ... */
typedef enum {
    TASK_NONE = 0,
    TASK_NTT_STOCKHAM,
    TASK_POINTWISE_MUL
} task_type_t;

typedef struct {
    task_type_t type;
    uint64_t *a;
    const uint64_t *b;
    const uint64_t *c;
    size_t n;
    int inv;
    int mod_idx;
} task_t;

typedef struct {
    void (*func)(void*);
    void* arg;
} fib_task_t;

typedef struct {
    char messages[1024][256];
    size_t head, tail;
    pthread_mutex_t mtx;
} fib_telemetry_t;

typedef struct {
    pthread_t* workers;
    size_t num_workers;
    fib_task_t* queue;
    size_t q_head, q_tail, q_count, q_cap;
    size_t running_count;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    pthread_cond_t idle;
    int shut;
    uint64_t* twiddles[4];
    size_t twiddle_size;
    uint32_t flags;
    fib_telemetry_t tel;
} fib_ctx_t;

/* Phase 1 Operations */
[[nodiscard]] static inline bool is_little_endian(void) {
    uint32_t x = 1;
    return *(uint8_t*)&x == 1;
}

LimbView view_create(uint64_t* ptr, size_t len);
void garner_4p(uint64_t out[4], uint64_t r[4]);
void fib_ctx_dispatch(fib_ctx_t* ctx, void (*f)(void*), void* arg);
void fib_ctx_wait(fib_ctx_t* ctx);
void ntt_precompute_twiddles(fib_ctx_t* ctx, size_t n);
void ntt_stable_scoped(LimbView v, int inv, fib_ctx_t* ctx, int m_idx);
uint64_t mod_pow(uint64_t base, uint64_t exp, uint64_t mod);
void bigint_mul_scalar(LimbView* res, ConstLimbView a, ConstLimbView b);

/* Scoped Arena Operations */
void bigint_add_scoped(LimbView* res, ConstLimbView a, ConstLimbView b);
void bigint_sub_scoped(LimbView* res, ConstLimbView a, ConstLimbView b);
void bigint_mul_scoped(LimbView* res, ConstLimbView a, ConstLimbView b, arena_t* arena, fib_ctx_t* ctx);
void bigint_mul_ssa(LimbView* res, ConstLimbView a, ConstLimbView b, arena_t* q);

/* Lifecycle */
fib_ctx_t* fib_ctx_create(size_t n_max);
void fib_ctx_destroy(fib_ctx_t* ctx);

/* Benchmarking */
typedef struct {
    double ntt_time;
    double ladder_time;
    double verify_time;
} fib_stats_t;

int fib_race(fib_ctx_t* ctx, unsigned long long N, bigint_t* res, fib_stats_t* stats);

#endif
