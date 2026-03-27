#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>

typedef __uint128_t u128;

typedef struct {
    uint64_t *restrict ptr;
    size_t len;
} LimbView;

typedef struct {
    const uint64_t *restrict ptr;
    size_t len;
} ConstLimbView;

static inline ConstLimbView to_const(LimbView v) {
    return (ConstLimbView){.ptr = (const uint64_t *restrict)v.ptr, .len = v.len};
}

#endif
