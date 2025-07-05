#ifndef CPU_FEATURES_H
#define CPU_FEATURES_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CPU Feature Detection System
 * 
 * This module provides runtime detection of CPU features and capabilities
 * to enable optimal code path selection and graceful fallbacks.
 */

// ============================================================================
// CPU Feature Flags
// ============================================================================

typedef struct {
    // x86/x86_64 features
    unsigned int has_sse2 : 1;
    unsigned int has_sse4_1 : 1;
    unsigned int has_sse4_2 : 1;
    unsigned int has_avx : 1;
    unsigned int has_avx2 : 1;
    unsigned int has_avx512f : 1;
    unsigned int has_popcnt : 1;
    unsigned int has_bmi1 : 1;
    unsigned int has_bmi2 : 1;
    
    // ARM features
    unsigned int has_neon : 1;
    unsigned int has_crc32 : 1;
    unsigned int has_aes : 1;
    unsigned int has_sha1 : 1;
    unsigned int has_sha2 : 1;
    
    // General features
    unsigned int has_64bit : 1;
    unsigned int has_fma : 1;
    unsigned int has_rdtsc : 1;
    
    // Cache information
    unsigned int l1_cache_size;
    unsigned int l2_cache_size;
    unsigned int l3_cache_size;
    unsigned int cache_line_size;
    
    // CPU information
    unsigned int num_cores;
    unsigned int num_logical_cores;
    char vendor_string[16];
    char brand_string[64];
    
    // Reserved for future use
    unsigned int reserved[8];
} cpu_features_t;

// ============================================================================
// Feature Detection Functions
// ============================================================================

/**
 * Initialize CPU feature detection
 * 
 * This function must be called once before using any other CPU feature
 * detection functions. It's thread-safe and can be called multiple times.
 * 
 * @return SUCCESS on success, error code on failure
 */
int cpu_features_init(void);

/**
 * Get CPU features structure
 * 
 * @return Pointer to CPU features structure, or NULL if not initialized
 */
const cpu_features_t* cpu_features_get(void);

/**
 * Check if a specific feature is available
 * 
 * @param feature_name Name of the feature to check (e.g., "sse2", "avx2", "neon")
 * @return 1 if feature is available, 0 otherwise
 */
int cpu_has_feature(const char* feature_name);

/**
 * Get optimal SIMD instruction set for current CPU
 * 
 * @return String describing the best available SIMD instruction set
 */
const char* cpu_get_optimal_simd(void);

/**
 * Get number of CPU cores (physical)
 * 
 * @return Number of physical CPU cores
 */
unsigned int cpu_get_num_cores(void);

/**
 * Get number of logical CPU cores (including hyperthreading)
 * 
 * @return Number of logical CPU cores
 */
unsigned int cpu_get_num_logical_cores(void);

/**
 * Get cache line size
 * 
 * @return Cache line size in bytes, or default value if unknown
 */
unsigned int cpu_get_cache_line_size(void);

/**
 * Print CPU information to stdout
 */
void cpu_features_print_info(void);

// ============================================================================
// Feature-Specific Helper Functions
// ============================================================================

/**
 * Check if SSE2 is available
 */
static inline int cpu_has_sse2(void) {
    const cpu_features_t* features = cpu_features_get();
    return features ? features->has_sse2 : 0;
}

/**
 * Check if AVX2 is available
 */
static inline int cpu_has_avx2(void) {
    const cpu_features_t* features = cpu_features_get();
    return features ? features->has_avx2 : 0;
}

/**
 * Check if NEON is available (ARM)
 */
static inline int cpu_has_neon(void) {
    const cpu_features_t* features = cpu_features_get();
    return features ? features->has_neon : 0;
}

// ============================================================================
// Compile-Time Feature Detection Macros
// ============================================================================

// x86/x86_64 compile-time detection
#if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64)
#define COMPILE_TIME_SSE2 1
#else
#define COMPILE_TIME_SSE2 0
#endif

#if defined(__SSE4_2__)
#define COMPILE_TIME_SSE4_2 1
#else
#define COMPILE_TIME_SSE4_2 0
#endif

#if defined(__AVX__)
#define COMPILE_TIME_AVX 1
#else
#define COMPILE_TIME_AVX 0
#endif

#if defined(__AVX2__)
#define COMPILE_TIME_AVX2 1
#else
#define COMPILE_TIME_AVX2 0
#endif

// ARM compile-time detection
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define COMPILE_TIME_NEON 1
#else
#define COMPILE_TIME_NEON 0
#endif

// ============================================================================
// Safe Feature Usage Macros
// ============================================================================

/**
 * Use a feature only if both compile-time and runtime support is available
 */
#define USE_SSE2() (COMPILE_TIME_SSE2 && cpu_has_sse2())
#define USE_AVX2() (COMPILE_TIME_AVX2 && cpu_has_avx2())
#define USE_NEON() (COMPILE_TIME_NEON && cpu_has_neon())

#ifdef __cplusplus
}
#endif

#endif // CPU_FEATURES_H
