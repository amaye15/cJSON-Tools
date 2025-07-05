#ifndef REGEX_ENGINE_H
#define REGEX_ENGINE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// High-performance cross-platform regex engine
// Automatically selects the best regex implementation for each platform

// Forward declaration for regex handle
typedef struct regex_engine regex_engine_t;

// Regex compilation flags
typedef enum {
    REGEX_FLAG_NONE = 0,
    REGEX_FLAG_CASE_INSENSITIVE = 1,
    REGEX_FLAG_MULTILINE = 2,
    REGEX_FLAG_DOTALL = 4,
    REGEX_FLAG_OPTIMIZE = 8  // Enable aggressive optimizations
} regex_flags_t;

// Match result structure
typedef struct {
    const char* start;  // Start of match
    const char* end;    // End of match
    size_t length;      // Length of match
    bool found;         // Whether a match was found
} regex_match_t;

// Replacement result structure
typedef struct {
    char* result;       // Resulting string (caller must free)
    size_t length;      // Length of result
    int replacements;   // Number of replacements made
    bool success;       // Whether operation succeeded
} regex_replace_result_t;

// Core regex functions
regex_engine_t* regex_compile(const char* pattern, regex_flags_t flags);
void regex_free(regex_engine_t* regex);

// Matching functions
bool regex_test(regex_engine_t* regex, const char* text);
regex_match_t regex_search(regex_engine_t* regex, const char* text);

// Replacement functions
regex_replace_result_t regex_replace_all(regex_engine_t* regex, const char* text, const char* replacement);
regex_replace_result_t regex_replace_first(regex_engine_t* regex, const char* text, const char* replacement);

// Utility functions
bool regex_is_valid_pattern(const char* pattern);
const char* regex_get_error_message(void);

// Performance optimization: Pre-compiled common patterns
typedef enum {
    REGEX_PATTERN_START_WITH,    // ^pattern
    REGEX_PATTERN_END_WITH,      // pattern$
    REGEX_PATTERN_EXACT_MATCH,   // ^pattern$
    REGEX_PATTERN_CONTAINS,      // pattern
    REGEX_PATTERN_CUSTOM         // User-defined pattern
} regex_pattern_type_t;

// Fast path for common patterns
regex_engine_t* regex_compile_optimized(const char* pattern, regex_pattern_type_t type);

// Batch operations for high performance
typedef struct {
    regex_engine_t** regexes;
    const char** replacements;
    size_t count;
} regex_batch_t;

regex_batch_t* regex_batch_create(size_t capacity);
void regex_batch_add(regex_batch_t* batch, regex_engine_t* regex, const char* replacement);
regex_replace_result_t regex_batch_replace(regex_batch_t* batch, const char* text);
void regex_batch_free(regex_batch_t* batch);

#ifdef __cplusplus
}
#endif

#endif // REGEX_ENGINE_H
