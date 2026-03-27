#include "arena.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <omp.h>

#ifndef MAP_ANONYMOUS
#ifdef MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#else
#define MAP_ANONYMOUS 0x1000 /* Common value for anonymous mapping */
#endif
#endif

arena_t* arena_create(size_t size) {
    arena_t* a = malloc(sizeof(arena_t));
    if (!a) PANIC("Arena Metadata Allocation Failed");

    /* Scoped Arena via mmap for NUMA-aware first-touch */
    a->base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (a->base == MAP_FAILED) {
        perror("mmap");
        free(a);
        PANIC("Arena mmap Failed (%zu bytes)", size);
    }

    a->size = size;
    a->offset = 0;
    a->allocations = 0;
    a->capacity = 1024;
    a->alloc_offsets = malloc((size_t)a->capacity * sizeof(size_t));
    a->alloc_sizes = malloc((size_t)a->capacity * sizeof(size_t));
    if (!a->alloc_offsets || !a->alloc_sizes) PANIC("Arena Metadata Array Allocation Failed");

    pthread_mutex_init(&a->mtx, NULL);
    return a;
}

void arena_destroy(arena_t* a) {
    if (!a) return;
    pthread_mutex_destroy(&a->mtx);
    free(a->alloc_offsets);
    free(a->alloc_sizes);
    munmap(a->base, a->size);
    free(a);
}

void* arena_alloc(arena_t* a, size_t size) {
    pthread_mutex_lock(&a->mtx);
    /* 128-byte Integrity Padding (Canary Zones) */
    size_t total_size = size + 2 * CANARY_SIZE;
    if (a->offset + total_size > a->size) {
        pthread_mutex_unlock(&a->mtx);
        PANIC("Arena Overflow: Requested %zu, Available %zu", total_size, a->size - a->offset);
    }

    if (a->allocations >= a->capacity) {
        a->capacity *= 2;
        a->alloc_offsets = realloc(a->alloc_offsets, (size_t)a->capacity * sizeof(size_t));
        a->alloc_sizes = realloc(a->alloc_sizes, (size_t)a->capacity * sizeof(size_t));
        if (!a->alloc_offsets || !a->alloc_sizes) PANIC("Arena Metadata Reallocation Failed");
    }

    uint8_t* ptr = a->base + a->offset;
    a->alloc_offsets[a->allocations] = a->offset;
    a->alloc_sizes[a->allocations] = size;
    a->offset += total_size;
    a->allocations++;
    pthread_mutex_unlock(&a->mtx);

    /* Write Leading Canary */
    uint64_t* leader = (uint64_t*)ptr;
    for (int i = 0; i < CANARY_SIZE / 8; i++) leader[i] = CANARY_PATTERN;

    void* data = ptr + CANARY_SIZE;
    memset(data, 0, size);

    /* Write Trailing Canary */
    uint64_t* trailer = (uint64_t*)(ptr + CANARY_SIZE + size);
    for (int i = 0; i < CANARY_SIZE / 8; i++) trailer[i] = CANARY_PATTERN;

    return data;
}

void arena_reset(arena_t* a) {
    if (!a) return;
    canary_verify(a);
    a->offset = 0;
    a->allocations = 0;
#ifdef DEBUG
    memset_explicit(a->base, 0xCC, a->size);
#endif
}

LimbView arena_alloc_limbs(arena_t* a, size_t count) {
    LimbView v;
    v.ptr = arena_alloc(a, count * sizeof(uint64_t));
    v.len = count;
    return v;
}

void canary_verify(arena_t* a) {
    for (int i = 0; i < a->allocations; i++) {
        size_t off = a->alloc_offsets[i];
        size_t sz = a->alloc_sizes[i];
        
        uint64_t* leader = (uint64_t*)(a->base + off);
        for (int k = 0; k < CANARY_SIZE / 8; k++) {
            if (leader[k] != CANARY_PATTERN) {
                PANIC("LEADER CANARY CORRUPTION at allocation %d, offset %zu", i, off + (size_t)k*8);
            }
        }

        uint64_t* trailer = (uint64_t*)(a->base + off + CANARY_SIZE + sz);
        for (int k = 0; k < CANARY_SIZE / 8; k++) {
            if (trailer[k] != CANARY_PATTERN) {
                PANIC("TRAILER CANARY CORRUPTION at allocation %d, offset %zu", i, off + CANARY_SIZE + sz + (size_t)k*8);
            }
        }
    }
}
