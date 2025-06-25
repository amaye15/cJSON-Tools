#ifndef SIMD_UTILS_H
#define SIMD_UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SIMD-optimized string length calculation
 * Falls back to standard strlen on non-SIMD platforms
 */
size_t strlen_simd(const char* str);

/**
 * SIMD-optimized whitespace skipping
 */
const char* skip_whitespace_optimized(const char* str, size_t len);

/**
 * SIMD-optimized JSON delimiter finding
 */
const char* find_delimiter_optimized(const char* str, size_t len);

#ifdef __cplusplus
}
#endif

#endif // SIMD_UTILS_H
