cmake_minimum_required(VERSION 3.25)
project(fib_hpc C)

# Strict ISO C23 Enforcement
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build Hygiene: -Werror -Wvla -Wshadow -Wconversion
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Werror -Wall -Wextra -Wvla -Wshadow -Wconversion -Wpedantic)
    add_compile_options(-march=native -O3 -flto)
endif()

# OpenMP Support
find_package(OpenMP REQUIRED)
if(OpenMP_C_FOUND)
    link_libraries(OpenMP::OpenMP_C)
endif()

# Hardware Abstraction Layer (HAL) - Quarantined Intrinsics
# Neon is typically enabled by default on Apple Silicon, but we maintain the HAL boundary.

# Phase 2: Metaprogramming Pipeline
find_path(GMP_INCLUDE_DIR NAMES gmp.h PATHS /opt/homebrew/include /usr/local/include)
find_library(GMP_LIBRARY NAMES gmp PATHS /opt/homebrew/lib /usr/local/lib)

add_executable(generate_constants tools/generate_constants.c)
target_include_directories(generate_constants PRIVATE ${GMP_INCLUDE_DIR})
target_link_libraries(generate_constants PRIVATE ${GMP_LIBRARY})

add_custom_command(
    OUTPUT fib_hpc_constants.h
    COMMAND generate_constants
    DEPENDS generate_constants
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Pre-computing NTT primitive roots and twiddles..."
)

add_executable(fib 
    main.c 
    bigint.c 
    verify.c 
    arena.c
    ${CMAKE_CURRENT_BINARY_DIR}/fib_hpc_constants.h
)

target_include_directories(fib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# Atomic Commits Protocol: 
# Every completed feature must pass all tests before moving to the next.
# tests/ folder addition will follow in Phase 3.
