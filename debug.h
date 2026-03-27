#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

/* Debug Levels */
#define DEBUG_NONE  0
#define DEBUG_ERROR 1
#define DEBUG_WARN  2
#define DEBUG_INFO  3
#define DEBUG_TRACE 4

#ifndef GLOBAL_DEBUG_LEVEL
#define GLOBAL_DEBUG_LEVEL DEBUG_INFO
#endif

#define LOG_MSG(level, prefix, fmt, ...) do { \
    if (GLOBAL_DEBUG_LEVEL >= (level)) { \
        fprintf(stderr, "[%s] [%s:%d] " fmt "\n", prefix, __FILE__, __LINE__, ##__VA_ARGS__); \
    } \
} while (0)

#define LOG_ERROR(fmt, ...) LOG_MSG(DEBUG_ERROR, "ERROR", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG_MSG(DEBUG_WARN,  "WARN ", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG_MSG(DEBUG_INFO,  "INFO ", fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) LOG_MSG(DEBUG_TRACE, "TRACE", fmt, ##__VA_ARGS__)

/* Advanced Expectations for Unit Tests */
#define EXPECT_EQ(actual, expected) do { \
    if ((actual) != (expected)) { \
        fprintf(stderr, "[FAIL] [%s:%d] Expected %" PRIu64 ", got %" PRIu64 "\n", __FILE__, __LINE__, (uint64_t)(expected), (uint64_t)(actual)); \
        abort(); \
    } \
} while (0)

#define EXPECT_TRUE(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] [%s:%d] Condition failed: %s\n", __FILE__, __LINE__, #cond); \
        abort(); \
    } \
} while (0)

#endif
