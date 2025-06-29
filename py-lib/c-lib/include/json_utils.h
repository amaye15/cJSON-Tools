#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <cjson/cJSON.h>

// Compiler optimization hints
#ifdef __GNUC__
#define LIKELY(x)       __builtin_expect(!!(x), 1)
#define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#define PURE            __attribute__((pure))
#define CONST           __attribute__((const))
#define HOT             __attribute__((hot))
#define COLD            __attribute__((cold))
#define INLINE          __attribute__((always_inline)) inline
#else
#define LIKELY(x)       (x)
#define UNLIKELY(x)     (x)
#define PURE
#define CONST
#define HOT
#define COLD
#define INLINE          inline
#endif

/**
 * Custom string duplication function (replacement for strdup)
 *
 * @param str The string to duplicate
 * @return A newly allocated copy of the string
 */
char* my_strdup(const char* str) HOT;

/**
 * Reads a JSON file into a string
 *
 * @param filename The name of the file to read
 * @return The file contents as a string (must be freed by caller)
 */
char* read_json_file(const char* filename);

/**
 * Reads JSON from stdin into a string
 *
 * @return The stdin contents as a string (must be freed by caller)
 */
char* read_json_stdin(void);

/**
 * Determines the number of CPU cores available
 *
 * @return The number of CPU cores
 */
int get_num_cores(void) CONST;

/**
 * Determines the optimal number of threads to use
 *
 * @param requested_threads The number of threads requested (0 for auto)
 * @return The optimal number of threads to use
 */
int get_optimal_threads(int requested_threads) PURE;

#endif /* JSON_UTILS_H */