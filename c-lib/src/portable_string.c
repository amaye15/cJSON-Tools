#include "../include/portable_string.h"
#include "../include/simd_utils.h"
#include <stdio.h>
#include <errno.h>

// Platform detection for strdup availability
#if defined(_GNU_SOURCE) || defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L
#define HAS_NATIVE_STRDUP 1
#else
#define HAS_NATIVE_STRDUP 0
#endif

// MSVC has _strdup instead of strdup
#ifdef _MSC_VER
#define HAS_NATIVE_STRDUP 1
#define native_strdup _strdup
#elif HAS_NATIVE_STRDUP
#define native_strdup strdup
#endif

/**
 * Internal optimized string duplication implementation
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static char* internal_strdup_optimized(const char* str) {
    if (UNLIKELY(str == NULL)) {
        return NULL;
    }

    // Use optimized strlen if available
    size_t len = portable_strlen(str);
    if (UNLIKELY(len == 0)) {
        // Handle empty string case
        char* result = (char*)malloc(1);
        if (result) {
            result[0] = '\0';
        }
        return result;
    }

    // Allocate memory for string + null terminator
    char* result = (char*)malloc(len + 1);
    if (UNLIKELY(result == NULL)) {
        return NULL;
    }

    // Copy the string including null terminator
    memcpy(result, str, len + 1);
    return result;
}
#pragma GCC diagnostic pop

/**
 * Portable string duplication function
 */
char* portable_strdup(const char* str) {
    if (UNLIKELY(str == NULL)) {
        return NULL;
    }

#if HAS_NATIVE_STRDUP && !defined(FORCE_CUSTOM_STRDUP)
    // Use native implementation when available and not forced to use custom
    return native_strdup(str);
#else
    // Use our optimized implementation
    return internal_strdup_optimized(str);
#endif
}

/**
 * Portable string duplication with length limit
 */
char* portable_strndup(const char* str, size_t max_len) {
    if (UNLIKELY(str == NULL)) {
        return NULL;
    }

    // Find the actual length to copy (up to max_len)
    size_t len = 0;
    while (len < max_len && str[len] != '\0') {
        len++;
    }

    // Allocate memory for string + null terminator
    char* result = (char*)malloc(len + 1);
    if (UNLIKELY(result == NULL)) {
        return NULL;
    }

    // Copy the string and add null terminator
    memcpy(result, str, len);
    result[len] = '\0';
    
    return result;
}

/**
 * Safe string copy with bounds checking
 */
int portable_strcpy_safe(char* dest, const char* src, size_t dest_size) {
    if (UNLIKELY(dest == NULL || src == NULL || dest_size == 0)) {
        return -1;
    }

    size_t src_len = portable_strlen(src);
    if (UNLIKELY(src_len >= dest_size)) {
        // Source string too long for destination
        return -1;
    }

    memcpy(dest, src, src_len + 1);
    return 0;
}

/**
 * Safe string concatenation with bounds checking
 */
int portable_strcat_safe(char* dest, const char* src, size_t dest_size) {
    if (UNLIKELY(dest == NULL || src == NULL || dest_size == 0)) {
        return -1;
    }

    size_t dest_len = portable_strlen(dest);
    size_t src_len = portable_strlen(src);
    
    if (UNLIKELY(dest_len + src_len >= dest_size)) {
        // Combined string too long for destination
        return -1;
    }

    memcpy(dest + dest_len, src, src_len + 1);
    return 0;
}

/**
 * Platform-specific optimized string length calculation
 */
size_t portable_strlen(const char* str) {
    if (UNLIKELY(str == NULL)) {
        return 0;
    }

    // Use SIMD-optimized version if available
    return strlen_simd(str);
}

/**
 * Check if strdup is available natively on this platform
 */
int portable_has_native_strdup(void) {
    return HAS_NATIVE_STRDUP;
}
