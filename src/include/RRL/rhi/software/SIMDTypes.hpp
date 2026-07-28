// RRL/include/rhi/software/SIMDTypes.hpp
#pragma once


#include <cstdint>


// These macros will be properly defined based on the target architecture
// #include <new>
// #define RRL_SWR_CACHE_LINE_SIZE std::hardware_destructive_interference_size
#define RRL_SWR_CACHE_LINE_SIZE 64

// SSE | AVX    -> 128-bit SIMD (16 bytes)
// AVX2         -> 256-bit SIMD (32 bytes)
#define RRL_SWR_SIMD_BIT_SIZE 128 
// #define SIMD_ALIGNMENT ( SIMD_BIT_SIZE / 8 ) // Minimum required alignment
#define RRL_SWR_SIMD_ALIGNMENT RRL_SWR_CACHE_LINE_SIZE // Cache aligment to avoid false cache sharing and ensuring one cache line fetch
#define RRL_SWR_SIMD_NUM_32BIT_ELMS ( RRL_SWR_SIMD_BIT_SIZE / ( 8 * 4 ) )

// Define this macro to use fixed-point arithmetic:
// #define RRL_SWR_FIXED_POINT


namespace rrl::rhi::software {
    

// --- Floats ------------------------------------------------------
struct alignas(RRL_SWR_SIMD_ALIGNMENT) f32x1 {
    // Size (128-bit): 1 * 4 * 4 = 16 bytes
    float x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) f32x2 {
    // Size (128-bit): 2 * 4 * 4 = 32 bytes
    float x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    float y[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) f32x3 {
    // Size (128-bit): 3 * 4 * 4 + 16 (padding) = 64 bytes
    float x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    float y[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    float z[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) f32x4 {
    // Size (128-bit): 4 * 4 * 4 = 64 bytes
    float x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    float y[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    float z[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    float w[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};


// --- Integers ----------------------------------------------------
struct alignas(RRL_SWR_SIMD_ALIGNMENT) i32x1 {
    // Size (128-bit): 1 * 4 * 4 = 16 bytes
    int32_t x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) i32x2 {
    // Size (128-bit): 2 * 4 * 4 = 32 bytes
    int32_t x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    int32_t y[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) i32x3 {
    // Size (128-bit): 3 * 4 * 4 + 16 (padding) = 64 bytes
    int32_t x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    int32_t y[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    int32_t z[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) i32x4 {
    // Size (128-bit): 4 * 4 * 4 = 64 bytes
    int32_t x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    int32_t y[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    int32_t z[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    int32_t w[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};


// --- Unsigned Integers -------------------------------------------
struct alignas(RRL_SWR_SIMD_ALIGNMENT) u32x1 {
    // Size (128-bit): 1 * 4 * 4 = 16 bytes
    uint32_t x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) u32x2 {
    // Size (128-bit): 2 * 4 * 4 = 32 bytes
    uint32_t x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    uint32_t y[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) u32x3 {
    // Size (128-bit): 3 * 4 * 4 + 16 (padding) = 64 bytes
    uint32_t x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    uint32_t y[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    uint32_t z[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) u32x4 {
    // Size (128-bit): 4 * 4 * 4 = 64 bytes
    uint32_t x[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    uint32_t y[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    uint32_t z[RRL_SWR_SIMD_NUM_32BIT_ELMS];
    uint32_t w[RRL_SWR_SIMD_NUM_32BIT_ELMS];
};


// --- Agnostic Type Names -----------------------------------------
#ifndef RRL_SWR_FIXED_POINT
    // Standard Floating Point
    using scalar_t  = float;
    using swr_vec1  = f32x1;
    using swr_vec2  = f32x2;
    using swr_vec3  = f32x3;
    using swr_vec4  = f32x4;
#else
    // Fixed-Point Arithmetic
    using scalar_t  = int32_t;
    using swr_vec1  = i32x1;
    using swr_vec2  = i32x2; 
    using swr_vec3  = i32x3;
    using swr_vec4  = i32x4;
#endif



} // namespace rrl::rhi::software
