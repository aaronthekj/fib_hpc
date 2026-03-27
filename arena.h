#ifndef ARENA_H
#define ARENA_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include "common.h"

#define CANARY_SIZE 128
#define CANARY_PATTERN 0xDEADBEEFCAFEBABEULL

typedef struct {
    uint8_t* base;
    size_t size;
    size_t offset;
    int allocations;
    size_t* alloc_offsets;
    size_t* alloc_sizes;
    int capacity;
    pthread_mutex_t mtx;
} arena_t;

/* Scoped Arena API (Section 2) */
arena_t* arena_create(size_t size);
void arena_destroy(arena_t* a);
void* arena_alloc(arena_t* a, size_t size);
void arena_reset(arena_t* a);

/* LimbView Allocation */
LimbView arena_alloc_limbs(arena_t* a, size_t count);

/* Safety Heartbeat (Section 5.2) */
void canary_verify(arena_t* a);

#endif
