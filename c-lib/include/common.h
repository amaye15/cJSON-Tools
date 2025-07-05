#ifndef COMMON_H
#define COMMON_H

/**
 * Common header file for cJSON-Tools C library
 * 
 * This file provides:
 * - Standard feature test macros for POSIX compliance
 * - Common includes used across the project
 * - Platform detection and compatibility macros
 * - Standard type definitions and constants
 * 
 * Include this file first in all C source files to ensure
 * consistent compilation environment.
 */

// ============================================================================
// Feature Test Macros (must be defined before any system includes)
// ============================================================================

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

// macOS specific feature test macros
#ifdef __APPLE__
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#ifndef __DARWIN_C_LEVEL
#define __DARWIN_C_LEVEL 200809L
#endif
// Enable BSD types
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#endif

// For Windows compatibility
#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

// ============================================================================
// Standard C Library Includes
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

// Platform-specific includes
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <regex.h>
#else
#include <windows.h>
#include <io.h>
#endif

// ============================================================================
// Platform Detection
// ============================================================================

// Unified Windows detection
#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

// Compiler detection
#ifdef _MSC_VER
#define COMPILER_MSVC 1
#elif defined(__GNUC__)
#define COMPILER_GCC 1
#elif defined(__clang__)
#define COMPILER_CLANG 1
#else
#define COMPILER_UNKNOWN 1
#endif

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
#define ARCH_X86_64 1
#elif defined(__i386__) || defined(_M_IX86)
#define ARCH_X86_32 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_ARM64 1
#elif defined(__arm__) || defined(_M_ARM)
#define ARCH_ARM32 1
#else
#define ARCH_UNKNOWN 1
#endif

// ============================================================================
// Project-Specific Includes
// ============================================================================

#include "compiler_hints.h"
#include "portable_string.h"

// ============================================================================
// Common Constants and Macros
// ============================================================================

// Buffer sizes
#define SMALL_BUFFER_SIZE 256
#define MEDIUM_BUFFER_SIZE 1024
#define LARGE_BUFFER_SIZE 4096
#define HUGE_BUFFER_SIZE 16384

// Error codes
#define SUCCESS 0
#define ERROR_INVALID_INPUT -1
#define ERROR_MEMORY_ALLOCATION -2
#define ERROR_FILE_IO -3
#define ERROR_JSON_PARSE -4
#define ERROR_REGEX_COMPILE -5

// Memory alignment
#define DEFAULT_ALIGNMENT 16
#define CACHE_LINE_SIZE 64

// ============================================================================
// Utility Macros
// ============================================================================

// Safe array size calculation
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Min/Max macros (avoiding conflicts with system definitions)
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// Safe string operations
#define SAFE_FREE(ptr) do { if (ptr) { free(ptr); (ptr) = NULL; } } while(0)

// Bit manipulation
#define SET_BIT(value, bit) ((value) |= (1U << (bit)))
#define CLEAR_BIT(value, bit) ((value) &= ~(1U << (bit)))
#define TEST_BIT(value, bit) (((value) >> (bit)) & 1U)

// ============================================================================
// Debug and Logging Macros
// ============================================================================

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, "[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) ((void)0)
#endif

#ifdef VERBOSE
#define VERBOSE_PRINT(fmt, ...) fprintf(stderr, "[VERBOSE] " fmt "\n", ##__VA_ARGS__)
#else
#define VERBOSE_PRINT(fmt, ...) ((void)0)
#endif

// Error reporting
#define ERROR_PRINT(fmt, ...) fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define WARNING_PRINT(fmt, ...) fprintf(stderr, "[WARNING] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

// ============================================================================
// Thread Safety Macros
// ============================================================================

#ifdef THREADING_DISABLED
#define THREAD_LOCAL
#define ATOMIC_INT int
#define ATOMIC_LOAD(ptr) (*(ptr))
#define ATOMIC_STORE(ptr, val) (*(ptr) = (val))
#else
#ifdef __STDC_VERSION__
#if __STDC_VERSION__ >= 201112L
#include <stdatomic.h>
#define THREAD_LOCAL _Thread_local
#define ATOMIC_INT atomic_int
#define ATOMIC_LOAD(ptr) atomic_load(ptr)
#define ATOMIC_STORE(ptr, val) atomic_store(ptr, val)
#else
#define THREAD_LOCAL __thread
#define ATOMIC_INT volatile int
#define ATOMIC_LOAD(ptr) (*(ptr))
#define ATOMIC_STORE(ptr, val) (*(ptr) = (val))
#endif
#else
#define THREAD_LOCAL
#define ATOMIC_INT volatile int
#define ATOMIC_LOAD(ptr) (*(ptr))
#define ATOMIC_STORE(ptr, val) (*(ptr) = (val))
#endif
#endif

// ============================================================================
// Version Information
// ============================================================================

#define CJSON_TOOLS_VERSION_MAJOR 1
#define CJSON_TOOLS_VERSION_MINOR 9
#define CJSON_TOOLS_VERSION_PATCH 0
#define CJSON_TOOLS_VERSION_STRING "1.9.0"

// ============================================================================
// Forward Declarations
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Common initialization function
int cjson_tools_init(void);
void cjson_tools_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // COMMON_H
