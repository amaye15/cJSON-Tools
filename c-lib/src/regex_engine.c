#include "../include/regex_engine.h"
#include "../include/common.h"
#include "../include/portable_string.h"
#include <stdlib.h>
#include <string.h>

// Platform-specific regex implementations
#ifdef _WIN32
    // Use Windows regex (std::regex via C wrapper or PCRE2)
    #define USE_PCRE2
#elif defined(__APPLE__)
    // Use macOS optimized regex
    #define USE_POSIX_EXTENDED
#elif defined(__linux__)
    // Use Linux optimized regex (PCRE2 if available, otherwise POSIX)
    #define USE_PCRE2
#else
    // Fallback to POSIX regex
    #define USE_POSIX_EXTENDED
#endif

// Include appropriate headers based on selected implementation
#ifdef USE_PCRE2
    // PCRE2 implementation (highest performance)
    #include <regex.h>  // Fallback to POSIX for now, will upgrade to PCRE2 later
    #define REGEX_IMPL_POSIX
#else
    #include <regex.h>
    #define REGEX_IMPL_POSIX
#endif

// Internal regex structure
struct regex_engine {
    regex_t posix_regex;
    char* pattern;
    regex_flags_t flags;
    regex_pattern_type_t type;
    bool compiled;
    bool optimized;
    
    // Performance optimization fields
    size_t pattern_length;
    bool is_literal;        // Pattern contains no regex metacharacters
    bool starts_with_caret; // Pattern starts with ^
    bool ends_with_dollar;  // Pattern ends with $
};

// Thread-local error message storage
static __thread char error_buffer[512] = {0};

// Performance optimization: Check if pattern is literal (no regex metacharacters)
static bool is_literal_pattern(const char* pattern) {
    if (!pattern) return false;
    
    const char* metacharacters = "^$.*+?[]{}()\\|";
    return strpbrk(pattern, metacharacters) == NULL;
}

// Optimized literal string matching
static bool literal_match(const char* text, const char* pattern, regex_pattern_type_t type) {
    if (!text || !pattern) return false;
    
    size_t text_len = strlen(text);
    size_t pattern_len = strlen(pattern);
    
    switch (type) {
        case REGEX_PATTERN_START_WITH:
            return text_len >= pattern_len && strncmp(text, pattern, pattern_len) == 0;
            
        case REGEX_PATTERN_END_WITH:
            if (text_len < pattern_len) return false;
            return strcmp(text + text_len - pattern_len, pattern) == 0;
            
        case REGEX_PATTERN_EXACT_MATCH:
            return strcmp(text, pattern) == 0;
            
        case REGEX_PATTERN_CONTAINS:
            return strstr(text, pattern) != NULL;
            
        default:
            return false;
    }
}

// Determine pattern type from regex pattern
static regex_pattern_type_t detect_pattern_type(const char* pattern) {
    if (!pattern || strlen(pattern) == 0) return REGEX_PATTERN_CUSTOM;
    
    size_t len = strlen(pattern);
    bool starts_caret = (pattern[0] == '^');
    bool ends_dollar = (len > 0 && pattern[len - 1] == '$');
    
    if (starts_caret && ends_dollar) {
        return REGEX_PATTERN_EXACT_MATCH;
    } else if (starts_caret) {
        return REGEX_PATTERN_START_WITH;
    } else if (ends_dollar) {
        return REGEX_PATTERN_END_WITH;
    } else {
        return REGEX_PATTERN_CONTAINS;
    }
}

// Extract literal pattern from regex pattern
static char* extract_literal_pattern(const char* pattern, regex_pattern_type_t type) {
    if (!pattern) return NULL;
    
    size_t len = strlen(pattern);
    const char* start = pattern;
    const char* end = pattern + len;
    
    // Remove ^ from start
    if (type == REGEX_PATTERN_START_WITH || type == REGEX_PATTERN_EXACT_MATCH) {
        if (*start == '^') start++;
    }
    
    // Remove $ from end
    if (type == REGEX_PATTERN_END_WITH || type == REGEX_PATTERN_EXACT_MATCH) {
        if (end > start && *(end - 1) == '$') end--;
    }
    
    size_t literal_len = end - start;
    char* literal = malloc(literal_len + 1);
    if (!literal) return NULL;
    
    memcpy(literal, start, literal_len);
    literal[literal_len] = '\0';
    
    return literal;
}

regex_engine_t* regex_compile(const char* pattern, regex_flags_t flags) {
    if (!pattern) {
        snprintf(error_buffer, sizeof(error_buffer), "Pattern cannot be NULL");
        return NULL;
    }
    
    regex_engine_t* regex = calloc(1, sizeof(regex_engine_t));
    if (!regex) {
        snprintf(error_buffer, sizeof(error_buffer), "Memory allocation failed");
        return NULL;
    }
    
    regex->pattern = portable_strdup(pattern);
    if (!regex->pattern) {
        free(regex);
        snprintf(error_buffer, sizeof(error_buffer), "Memory allocation failed");
        return NULL;
    }
    
    regex->flags = flags;
    regex->pattern_length = strlen(pattern);
    regex->type = detect_pattern_type(pattern);
    
    // Check if this is a literal pattern that can be optimized
    char* literal_pattern = extract_literal_pattern(pattern, regex->type);
    if (literal_pattern && is_literal_pattern(literal_pattern)) {
        regex->is_literal = true;
        regex->optimized = true;
        free(regex->pattern);
        regex->pattern = literal_pattern;
        regex->pattern_length = strlen(literal_pattern);  // Update length to match new pattern
    } else {
        free(literal_pattern);
        regex->is_literal = false;
        
        // Compile POSIX regex for complex patterns
        int regex_flags = REG_EXTENDED;
        if (flags & REGEX_FLAG_CASE_INSENSITIVE) {
            regex_flags |= REG_ICASE;
        }
        
        int result = regcomp(&regex->posix_regex, pattern, regex_flags);
        if (result != 0) {
            char error_msg[256];
            regerror(result, &regex->posix_regex, error_msg, sizeof(error_msg));
            snprintf(error_buffer, sizeof(error_buffer), "Regex compilation failed: %s", error_msg);
            free(regex->pattern);
            free(regex);
            return NULL;
        }
        
        regex->compiled = true;
    }
    
    return regex;
}

regex_engine_t* regex_compile_optimized(const char* pattern, regex_pattern_type_t type) {
    regex_engine_t* regex = regex_compile(pattern, REGEX_FLAG_OPTIMIZE);
    if (regex) {
        regex->type = type;
    }
    return regex;
}

void regex_free(regex_engine_t* regex) {
    if (!regex) return;
    
    if (regex->compiled) {
        regfree(&regex->posix_regex);
    }
    
    free(regex->pattern);
    free(regex);
}

bool regex_test(regex_engine_t* regex, const char* text) {
    if (!regex || !text) return false;
    
    // Use optimized literal matching if possible
    if (regex->is_literal && regex->optimized) {
        return literal_match(text, regex->pattern, regex->type);
    }
    
    // Fall back to POSIX regex
    if (regex->compiled) {
        return regexec(&regex->posix_regex, text, 0, NULL, 0) == 0;
    }
    
    return false;
}

regex_match_t regex_search(regex_engine_t* regex, const char* text) {
    regex_match_t result = {0};
    
    if (!regex || !text) {
        return result;
    }
    
    // Use optimized literal matching if possible
    if (regex->is_literal && regex->optimized) {
        if (literal_match(text, regex->pattern, regex->type)) {
            result.found = true;
            result.start = text;
            result.end = text + strlen(text);
            result.length = result.end - result.start;
        }
        return result;
    }
    
    // Fall back to POSIX regex
    if (regex->compiled) {
        regmatch_t match;
        if (regexec(&regex->posix_regex, text, 1, &match, 0) == 0) {
            result.found = true;
            result.start = text + match.rm_so;
            result.end = text + match.rm_eo;
            result.length = match.rm_eo - match.rm_so;
        }
    }
    
    return result;
}

regex_replace_result_t regex_replace_first(regex_engine_t* regex, const char* text, const char* replacement) {
    regex_replace_result_t result = {0};

    if (!regex || !text || !replacement) {
        return result;
    }

    // Use optimized literal matching if possible
    if (regex->is_literal && regex->optimized) {
        const char* match_pos = NULL;
        size_t match_len = 0;

        switch (regex->type) {
            case REGEX_PATTERN_START_WITH:
                if (strncmp(text, regex->pattern, regex->pattern_length) == 0) {
                    match_pos = text;
                    match_len = regex->pattern_length;
                }
                break;

            case REGEX_PATTERN_END_WITH:
                {
                    size_t text_len = strlen(text);
                    if (text_len >= regex->pattern_length &&
                        strcmp(text + text_len - regex->pattern_length, regex->pattern) == 0) {
                        match_pos = text + text_len - regex->pattern_length;
                        match_len = regex->pattern_length;
                    }
                }
                break;

            case REGEX_PATTERN_EXACT_MATCH:
                if (strcmp(text, regex->pattern) == 0) {
                    match_pos = text;
                    match_len = strlen(text);
                }
                break;

            case REGEX_PATTERN_CONTAINS:
                match_pos = strstr(text, regex->pattern);
                if (match_pos) {
                    match_len = regex->pattern_length;
                }
                break;

            default:
                break;
        }

        if (match_pos) {
            // Calculate result length
            size_t text_len = strlen(text);
            size_t replacement_len = strlen(replacement);
            size_t prefix_len = match_pos - text;
            size_t suffix_len = text_len - prefix_len - match_len;
            size_t result_len = prefix_len + replacement_len + suffix_len;



            // Allocate result buffer
            result.result = malloc(result_len + 1);
            if (result.result) {
                // Copy prefix
                memcpy(result.result, text, prefix_len);
                // Copy replacement
                memcpy(result.result + prefix_len, replacement, replacement_len);
                // Copy suffix
                memcpy(result.result + prefix_len + replacement_len, match_pos + match_len, suffix_len);
                // Null terminate
                result.result[result_len] = '\0';

                result.length = result_len;
                result.replacements = 1;
                result.success = true;
            }
        } else {
            // No match, return original text
            result.result = portable_strdup(text);
            result.length = strlen(text);
            result.replacements = 0;
            result.success = (result.result != NULL);
        }
    } else if (regex->compiled) {
        // Use POSIX regex for complex patterns
        regmatch_t match;
        if (regexec(&regex->posix_regex, text, 1, &match, 0) == 0) {
            // Calculate result length
            size_t text_len = strlen(text);
            size_t replacement_len = strlen(replacement);
            size_t prefix_len = match.rm_so;
            // size_t match_len = match.rm_eo - match.rm_so;  // Not used in this function
            size_t suffix_len = text_len - match.rm_eo;
            size_t result_len = prefix_len + replacement_len + suffix_len;

            // Allocate result buffer
            result.result = malloc(result_len + 1);
            if (result.result) {
                // Copy prefix
                memcpy(result.result, text, prefix_len);
                // Copy replacement
                memcpy(result.result + prefix_len, replacement, replacement_len);
                // Copy suffix
                memcpy(result.result + prefix_len + replacement_len, text + match.rm_eo, suffix_len);
                // Null terminate
                result.result[result_len] = '\0';

                result.length = result_len;
                result.replacements = 1;
                result.success = true;
            }
        } else {
            // No match, return original text
            result.result = portable_strdup(text);
            result.length = strlen(text);
            result.replacements = 0;
            result.success = (result.result != NULL);
        }
    } else {
        // No valid regex, return original text
        result.result = portable_strdup(text);
        result.length = strlen(text);
        result.replacements = 0;
        result.success = (result.result != NULL);
    }

    return result;
}

regex_replace_result_t regex_replace_all(regex_engine_t* regex, const char* text, const char* replacement) {
    // For now, implement as replace_first since most use cases only need first match
    return regex_replace_first(regex, text, replacement);
}

bool regex_is_valid_pattern(const char* pattern) {
    if (!pattern) return false;
    
    regex_engine_t* test_regex = regex_compile(pattern, REGEX_FLAG_NONE);
    if (test_regex) {
        regex_free(test_regex);
        return true;
    }
    
    return false;
}

const char* regex_get_error_message(void) {
    return error_buffer;
}

// Batch operations
regex_batch_t* regex_batch_create(size_t capacity) {
    regex_batch_t* batch = calloc(1, sizeof(regex_batch_t));
    if (!batch) return NULL;
    
    batch->regexes = calloc(capacity, sizeof(regex_engine_t*));
    batch->replacements = calloc(capacity, sizeof(char*));
    
    if (!batch->regexes || !batch->replacements) {
        free(batch->regexes);
        free(batch->replacements);
        free(batch);
        return NULL;
    }
    
    return batch;
}

void regex_batch_add(regex_batch_t* batch, regex_engine_t* regex, const char* replacement) {
    if (!batch || !regex || !replacement) return;
    
    // For simplicity, assume capacity is sufficient
    batch->regexes[batch->count] = regex;
    batch->replacements[batch->count] = replacement;
    batch->count++;
}

regex_replace_result_t regex_batch_replace(regex_batch_t* batch, const char* text) {
    regex_replace_result_t result = {0};
    
    if (!batch || !text) return result;
    
    // Apply first matching regex
    for (size_t i = 0; i < batch->count; i++) {
        if (regex_test(batch->regexes[i], text)) {
            return regex_replace_first(batch->regexes[i], text, batch->replacements[i]);
        }
    }
    
    // No matches, return original text
    result.result = portable_strdup(text);
    result.length = strlen(text);
    result.replacements = 0;
    result.success = (result.result != NULL);
    
    return result;
}

void regex_batch_free(regex_batch_t* batch) {
    if (!batch) return;
    
    free(batch->regexes);
    free(batch->replacements);
    free(batch);
}
