#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#define PANIC(msg, ...) do { \
    fprintf(stderr, "[PANIC] [%s:%d] Thread %p: " msg "\n", __FILE__, __LINE__, (void*)pthread_self(), ##__VA_ARGS__); \
    abort(); \
} while(0)

static inline void* xmalloc(size_t size) {
    void* p = malloc(size);
    if (__builtin_expect(!p, 0)) PANIC("malloc(%zu) failed: %s", size, strerror(errno));
    return p;
}

static inline void* xaligned_alloc(size_t alignment, size_t size) {
    void* p = aligned_alloc(alignment, size);
    if (__builtin_expect(!p, 0)) PANIC("aligned_alloc(%zu, %zu) failed: %s", alignment, size, strerror(errno));
    return p;
}

static inline void xpthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    int r = pthread_create(thread, attr, start_routine, arg);
    if (__builtin_expect(r != 0, 0)) PANIC("pthread_create failed: %s", strerror(r));
}

#endif
