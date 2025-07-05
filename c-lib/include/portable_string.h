#ifndef PORTABLE_STRING_H
#define PORTABLE_STRING_H

#include "compiler_hints.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Portable string duplication function
 * 
 * This function provides a portable implementation of strdup that works
 * across all platforms and C standards. It automatically selects the
 * best available implementation based on the platform and compiler.
 * 
 * @param str The string to duplicate (must be null-terminated)
 * @return A newly allocated copy of the string, or NULL on failure
 * @note The caller is responsible for freeing the returned string
 */
char* portable_strdup(const char* str) HOT_PATH;

/**
 * Portable string duplication with length limit
 * 
 * @param str The string to duplicate
 * @param max_len Maximum length to copy
 * @return A newly allocated copy of the string, or NULL on failure
 */
char* portable_strndup(const char* str, size_t max_len) HOT_PATH;

/**
 * Safe string copy with bounds checking
 * 
 * @param dest Destination buffer
 * @param src Source string
 * @param dest_size Size of destination buffer
 * @return 0 on success, -1 on error
 */
int portable_strcpy_safe(char* dest, const char* src, size_t dest_size);

/**
 * Safe string concatenation with bounds checking
 * 
 * @param dest Destination buffer
 * @param src Source string to append
 * @param dest_size Size of destination buffer
 * @return 0 on success, -1 on error
 */
int portable_strcat_safe(char* dest, const char* src, size_t dest_size);

/**
 * Platform-specific optimized string length calculation
 * 
 * @param str The string to measure
 * @return Length of the string
 */
size_t portable_strlen(const char* str) PURE_FUNC HOT_PATH;

/**
 * Check if strdup is available natively on this platform
 * 
 * @return 1 if native strdup is available, 0 otherwise
 */
int portable_has_native_strdup(void) CONST_FUNC;

#ifdef __cplusplus
}
#endif

#endif // PORTABLE_STRING_H
