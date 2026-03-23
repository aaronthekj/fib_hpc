#pragma once

#include <stdint.h>
#include <stddef.h>

// Binary Power-of-Two Radix
#define RADIX_BITS 32
typedef uint32_t limb_t;

// Telemetry bounds
#define TIME_LIMIT_MS 1000
#define PREDICTION_BUFFER_MS 200
