# Antigravity Fibonacci Project: Architectural & Engineering Standards

These rules must be followed at all times during code generation and refactoring:

## 1. Memory & Resource Management
* **Zero Dynamic Allocation**: No `malloc`, `calloc`, or `free` calls are permitted within the 1.000-second execution window. All memory must be pre-allocated into Temporal Ping-Pong Buffers during the initialization phase.
* **Alignment**: All BigInt arrays and scratchpads must be 64-byte aligned using `aligned_alloc` to ensure compatibility with cache lines and AVX-512 instructions.
* **Static Bounds**: Every loop must have a provable constant upper bound to ensure deterministic timing.

## 2. High-Performance C Patterns
* **Pointer Aliasing**: All function signatures for mathematical kernels must use the `restrict` keyword to enable aggressive compiler auto-vectorization.
* **Branchless Arithmetic**: Carry propagation and modular reductions must use bitwise masking and shifts (e.g., `sum >> 32`) instead of if/else blocks to prevent branch misprediction.
* **Software Pipelining**: Manual loop unrolling and instruction interleaving (loading iteration $i+1$ during calculation of $i$) must be prioritized for the NTT and addition kernels.

## 3. Algorithmic Integrity
* **$O(n \log n)$ Formatting**: Under no circumstances should $O(n^2)$ decimal formatting be used. The Recursive Divide & Conquer base conversion is the only permitted method for decimal output.
* **Telemetry First**: The Adaptive Step-Sizing logic (800ms check) must wrap the core doubling loop to guarantee a valid $F_N$ is returned before the 1.000s wall.

## 4. Software Excellence (SWE)
* **Assertion Density**: Maintain a high density of `assert()` statements for range and logic validation during development (to be disabled only in the final `-DNDEBUG` release build).
* **Separation of Concerns**: Keep the Mathematical Engine (NTT/BigInt) strictly decoupled from the Recurrence Logic (Fast Doubling) and the Formatting Engine.
* **Performance Regression Testing**: Every major change must be accompanied by a cycle-count benchmark using `clock_gettime(CLOCK_MONOTONIC, ...)`.

## 5. Compiler Strategy
* **Mandatory Flags**: The project must always be compatible with `-O3 -march=native -flto -fopenmp`.
* **Inlining**: Use `inline __attribute__((always_inline))` for all limb-level mathematical operations to eliminate call-stack overhead.
