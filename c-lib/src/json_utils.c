#include "../include/common.h"
#include "../include/json_utils.h"
#include "../include/simd_utils.h"
#include "../include/regex_engine.h"

// Simplified Windows implementation - avoid complex header conflicts
#ifdef __WINDOWS__
    #include <process.h>

    // Simple Windows implementation without advanced features
    static long msvc_sysconf(int name) {
        (void)name; // Suppress unused parameter warning
        return 4; // Default to 4 cores for Windows
    }

    #define sysconf(x) msvc_sysconf(x)
    #define _SC_NPROCESSORS_ONLN 1
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #ifdef __unix__
    #include <sys/mman.h>
    #endif
#endif



// Disable SIMD on Windows builds for initial PyPI release
#ifdef THREADING_DISABLED
#undef __SSE2__
#undef __AVX2__
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifdef __SSE2__
/**
 * SIMD-optimized string duplication for better performance
 */
static char* my_strdup_simd(const char* str) {
    if (UNLIKELY(str == NULL)) return NULL;

    size_t len = strlen_simd(str);
    // Align allocation to 16 bytes for SIMD operations
    size_t aligned_size = (len + 16) & ~15;
    char* new_str;

    // Use posix_memalign for better compatibility
    if (posix_memalign((void**)&new_str, 16, aligned_size) != 0) {
        return NULL;
    }

    if (UNLIKELY(new_str == NULL)) return NULL;

    // Copy 16 bytes at a time using SIMD
    size_t simd_len = len & ~15;
    for (size_t i = 0; i < simd_len; i += 16) {
        __m128i chunk = _mm_loadu_si128((__m128i*)(str + i));
        _mm_store_si128((__m128i*)(new_str + i), chunk);
    }

    // Copy remaining bytes
    memcpy(new_str + simd_len, str + simd_len, len - simd_len + 1);
    return new_str;
}
#endif

/**
 * Optimized string duplication function with branch prediction
 */
char* my_strdup(const char* str) {
    if (UNLIKELY(str == NULL)) return NULL;

    size_t len = strlen_simd(str) + 1;

#ifdef __SSE2__
    // Use SIMD for longer strings
    if (len > 32) {
        return my_strdup_simd(str);
    }
#endif

    char* new_str = (char*)malloc(len);
    if (UNLIKELY(new_str == NULL)) return NULL;

    return (char*)memcpy(new_str, str, len);
}

/**
 * Memory-mapped file reading for large files
 */
static char* read_json_file_mmap(const char* filename) {
    (void)filename; // Suppress unused parameter warning on non-Unix systems
#ifdef __unix__
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return NULL;

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return NULL;
    }

    // Use mmap for files larger than 64KB
    if (st.st_size > 65536) {
        void* mapped = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        if (mapped == MAP_FAILED) return NULL;

        // Copy to heap memory and unmap
        char* buffer = malloc(st.st_size + 1);
        if (buffer) {
            memcpy(buffer, mapped, st.st_size);
            buffer[st.st_size] = '\0';
        }
        munmap(mapped, st.st_size);

        return buffer;
    }

    close(fd);
#elif defined(__WINDOWS__)
    // Disable memory mapping on Windows for now to avoid header conflicts
    (void)filename; // Suppress unused parameter warning
#endif
    return NULL; // Fall back to regular read
}

/**
 * Optimized JSON file reader with better error handling
 */
char* read_json_file(const char* filename) {
    // Try memory mapping for large files first
    char* result = read_json_file_mmap(filename);
    if (result) return result;

    // Fall back to standard FILE* operations for all platforms
    FILE* file = fopen(filename, "rb"); // Use binary mode for better performance
    if (UNLIKELY(!file)) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }

    // Get file size
    if (UNLIKELY(fseek(file, 0, SEEK_END) != 0)) {
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (UNLIKELY(file_size < 0)) {
        fclose(file);
        return NULL;
    }

    if (UNLIKELY(fseek(file, 0, SEEK_SET) != 0)) {
        fclose(file);
        return NULL;
    }

    // Allocate buffer with extra space for null terminator
    char* buffer = (char*)malloc(file_size + 1);
    if (UNLIKELY(!buffer)) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    // Read file into buffer
    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';
    fclose(file);

    return buffer;
}

/**
 * Reads JSON from stdin into a string
 */
char* read_json_stdin(void) {
    char buffer[1024];
    size_t content_size = 1;
    size_t content_used = 0;
    char* content = malloc(content_size);
    
    if (!content) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }
    
    content[0] = '\0';
    
    while (fgets(buffer, sizeof(buffer), stdin)) {
        size_t buffer_len = strlen_simd(buffer);
        while (content_used + buffer_len + 1 > content_size) {
            content_size *= 2;
            content = realloc(content, content_size);
            if (!content) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                return NULL;
            }
        }
        strcat(content, buffer);
        content_used += buffer_len;
    }
    
    return content;
}

/**
 * Determines the number of CPU cores available
 */
int get_num_cores(void) {
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    return (num_cores > 0) ? (int)num_cores : 1;
}

/**
 * Optimized thread count determination with better scaling
 */
int get_optimal_threads(int requested_threads) {
    if (LIKELY(requested_threads > 0)) {
        // Cap at reasonable maximum to prevent resource exhaustion
        return requested_threads > 64 ? 64 : requested_threads;
    }

    int num_cores = get_num_cores();

    // Improved heuristic for better performance scaling:
    // - For 1-2 cores: use all cores
    // - For 3-4 cores: use cores-1 (leave one for system)
    // - For 5-8 cores: use 75% of cores
    // - For >8 cores: use 50% + 2 (better for large systems)

    if (num_cores <= 2) {
        return num_cores;
    } else if (num_cores <= 4) {
        return num_cores - 1;
    } else if (num_cores <= 8) {
        return (num_cores * 3) / 4; // 75% of cores
    } else {
        return (num_cores / 2) + 2;
    }
}

/**
 * Helper function to recursively process JSON objects/arrays for filtering
 */
static cJSON* filter_json_recursive(const cJSON* json, int remove_empty_strings, int remove_nulls) {
    if (UNLIKELY(json == NULL)) return NULL;

    if (cJSON_IsObject(json)) {
        cJSON* new_obj = cJSON_CreateObject();
        if (UNLIKELY(!new_obj)) return NULL;

        const cJSON* child = json->child;
        while (child) {
            int should_skip = 0;

            // Check if we should skip this key-value pair
            if (remove_empty_strings && cJSON_IsString(child) &&
                child->valuestring && strlen_simd(child->valuestring) == 0) {
                should_skip = 1;
            }

            if (remove_nulls && cJSON_IsNull(child)) {
                should_skip = 1;
            }

            if (!should_skip) {
                // Recursively process the value
                cJSON* filtered_value = filter_json_recursive(child, remove_empty_strings, remove_nulls);
                if (filtered_value) {
                    cJSON_AddItemToObject(new_obj, child->string, filtered_value);
                }
            }

            child = child->next;
        }

        return new_obj;
    } else if (cJSON_IsArray(json)) {
        cJSON* new_array = cJSON_CreateArray();
        if (UNLIKELY(!new_array)) return NULL;

        const cJSON* child = json->child;
        while (child) {
            int should_skip = 0;

            // Check if we should skip this array element
            if (remove_empty_strings && cJSON_IsString(child) &&
                child->valuestring && strlen_simd(child->valuestring) == 0) {
                should_skip = 1;
            }

            if (remove_nulls && cJSON_IsNull(child)) {
                should_skip = 1;
            }

            if (!should_skip) {
                // Recursively process the array element
                cJSON* filtered_value = filter_json_recursive(child, remove_empty_strings, remove_nulls);
                if (filtered_value) {
                    cJSON_AddItemToArray(new_array, filtered_value);
                }
            }

            child = child->next;
        }

        return new_array;
    } else {
        // For primitive values, create a copy
        return cJSON_Duplicate(json, 1);
    }
}

/**
 * Removes all keys that have empty string values from a JSON object
 */
cJSON* remove_empty_strings(const cJSON* json) {
    if (UNLIKELY(json == NULL)) return NULL;
    return filter_json_recursive(json, 1, 0);
}

/**
 * Removes all keys that have null values from a JSON object
 */
cJSON* remove_nulls(const cJSON* json) {
    if (UNLIKELY(json == NULL)) return NULL;
    return filter_json_recursive(json, 0, 1);
}

/**
 * Helper function to recursively process JSON objects/arrays for key replacement
 */
static cJSON* replace_keys_recursive(const cJSON* json, const char* pattern, const char* replacement) {
    if (UNLIKELY(json == NULL)) return NULL;

#ifdef __WINDOWS__
    // Windows fallback: simple string replacement (no regex support)
    // For Windows builds, we'll implement a simple string match instead of regex
    // This is a limitation but ensures cross-platform compatibility
    (void)pattern; // Suppress unused parameter warning
    (void)replacement;
    // For now, just return a copy - Windows regex support can be added later
    return cJSON_Duplicate(json, 1);
#else
    // HIGH-PERFORMANCE: Use optimized regex engine for Unix-like systems
    regex_engine_t* regex = regex_compile(pattern, REGEX_FLAG_OPTIMIZE);
    if (!regex) {
        // Invalid regex pattern, return a copy of the original
        return cJSON_Duplicate(json, 1);
    }

    if (cJSON_IsObject(json)) {
        cJSON* new_obj = cJSON_CreateObject();
        if (UNLIKELY(!new_obj)) {
            regex_free(regex);
            return NULL;
        }

        const cJSON* child = json->child;
        while (child) {
            const char* key = child->string;
            char* new_key = NULL;

            if (key) {
                // Test if the key matches the regex pattern and perform replacement
                if (regex_test(regex, key)) {
                    // Key matches pattern, perform regex replacement
                    regex_replace_result_t replace_result = regex_replace_first(regex, key, replacement);
                    if (replace_result.success && replace_result.result) {
                        new_key = replace_result.result;  // Transfer ownership
                    } else {
                        new_key = my_strdup(key);  // Fallback to original
                    }
                } else {
                    // Key doesn't match, keep original
                    new_key = my_strdup(key);
                }
            }

            if (new_key) {
                // Recursively process the value
                cJSON* processed_value = replace_keys_recursive(child, pattern, replacement);
                if (processed_value) {
                    cJSON_AddItemToObject(new_obj, new_key, processed_value);
                }
                free(new_key);
            }

            child = child->next;
        }

        regex_free(regex);
        return new_obj;
    } else if (cJSON_IsArray(json)) {
        cJSON* new_array = cJSON_CreateArray();
        if (UNLIKELY(!new_array)) {
            regex_free(regex);
            return NULL;
        }

        const cJSON* child = json->child;
        while (child) {
            // Recursively process array elements
            cJSON* processed_value = replace_keys_recursive(child, pattern, replacement);
            if (processed_value) {
                cJSON_AddItemToArray(new_array, processed_value);
            }
            child = child->next;
        }

        regex_free(regex);
        return new_array;
    } else {
        // For primitive values, create a copy
        regex_free(regex);
        return cJSON_Duplicate(json, 1);
    }
#endif /* !__WINDOWS__ */
}

/**
 * Replaces JSON keys that match a regex pattern with a replacement string
 */
cJSON* replace_keys(const cJSON* json, const char* pattern, const char* replacement) {
    if (UNLIKELY(json == NULL || pattern == NULL || replacement == NULL)) return NULL;
    return replace_keys_recursive(json, pattern, replacement);
}

/**
 * Helper function to recursively process JSON objects/arrays for value replacement
 */
static cJSON* replace_values_recursive(const cJSON* json, const char* pattern, const char* replacement) {
    if (UNLIKELY(json == NULL)) return NULL;

#ifdef __WINDOWS__
    // Windows fallback: simple string replacement (no regex support)
    // For Windows builds, we'll implement a simple string match instead of regex
    // This is a limitation but ensures cross-platform compatibility
    (void)pattern; // Suppress unused parameter warning
    (void)replacement;
    // For now, just return a copy - Windows regex support can be added later
    return cJSON_Duplicate(json, 1);
#else
    // HIGH-PERFORMANCE: Use optimized regex engine for Unix-like systems
    regex_engine_t* regex = regex_compile(pattern, REGEX_FLAG_OPTIMIZE);
    if (!regex) {
        // Invalid regex pattern, return a copy of the original
        return cJSON_Duplicate(json, 1);
    }

    if (cJSON_IsObject(json)) {
        cJSON* new_obj = cJSON_CreateObject();
        if (UNLIKELY(!new_obj)) {
            regex_free(regex);
            return NULL;
        }

        const cJSON* child = json->child;
        while (child) {
            const char* key = child->string;

            // Recursively process the value
            cJSON* processed_value = replace_values_recursive(child, pattern, replacement);
            if (processed_value && key) {
                cJSON_AddItemToObject(new_obj, key, processed_value);
            }

            child = child->next;
        }

        regex_free(regex);
        return new_obj;
    } else if (cJSON_IsArray(json)) {
        cJSON* new_array = cJSON_CreateArray();
        if (UNLIKELY(!new_array)) {
            regex_free(regex);
            return NULL;
        }

        const cJSON* child = json->child;
        while (child) {
            // Recursively process array elements
            cJSON* processed_value = replace_values_recursive(child, pattern, replacement);
            if (processed_value) {
                cJSON_AddItemToArray(new_array, processed_value);
            }
            child = child->next;
        }

        regex_free(regex);
        return new_array;
    } else if (cJSON_IsString(json)) {
        // This is a string value - check if it matches the pattern
        const char* string_value = cJSON_GetStringValue(json);
        if (string_value) {
            // Test if the string matches the regex pattern and perform replacement
            if (regex_test(regex, string_value)) {
                // String matches pattern, perform regex replacement
                regex_replace_result_t replace_result = regex_replace_first(regex, string_value, replacement);
                if (replace_result.success && replace_result.result) {
                    cJSON* result_json = cJSON_CreateString(replace_result.result);
                    free(replace_result.result);  // Free the replacement result
                    regex_free(regex);
                    return result_json;
                } else {
                    // Fallback to original
                    regex_free(regex);
                    return cJSON_CreateString(string_value);
                }
            } else {
                // String doesn't match, keep original
                regex_free(regex);
                return cJSON_CreateString(string_value);
            }
        } else {
            // Invalid string, return copy
            regex_free(regex);
            return cJSON_Duplicate(json, 1);
        }
    } else {
        // For non-string primitive values (numbers, booleans, null), create a copy
        regex_free(regex);
        return cJSON_Duplicate(json, 1);
    }
#endif /* !__WINDOWS__ */
}

/**
 * Replaces JSON string values that match a regex pattern with a replacement string
 */
cJSON* replace_values(const cJSON* json, const char* pattern, const char* replacement) {
    if (UNLIKELY(json == NULL || pattern == NULL || replacement == NULL)) return NULL;
    return replace_values_recursive(json, pattern, replacement);
}