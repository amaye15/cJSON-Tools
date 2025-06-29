#include "../include/json_flattener.h"
#include "../include/json_utils.h"
#include "../include/thread_pool.h"
#include "../include/memory_pool.h"
#include "../include/string_view.h"
#include "../include/compiler_hints.h"
#include "../include/simd_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(push)
#pragma warning(disable: 4996) // Disable unsafe function warnings
#endif

// MSVC-safe string operations
static inline void safe_strncpy(char* dest, const char* src, size_t size) {
#ifdef _MSC_VER
    if (size > 0) {
        strncpy_s(dest, size, src, _TRUNCATE);
    }
#else
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
#endif
}

#define MAX_KEY_LENGTH 2048
#define BATCH_SIZE 1000
#define MIN_OBJECTS_PER_THREAD 25   // Reduced for better parallelization
#define MIN_BATCH_SIZE_FOR_MT 100   // Reduced threshold for multi-threading
#define INITIAL_ARRAY_CAPACITY 64   // Larger initial capacity
#define KEY_BUFFER_SIZE 512         // Pre-allocated key buffer size
#define MEMORY_POOL_SIZE 8192       // Memory pool for small allocations
#define MAX_CACHED_KEYS 256         // Maximum number of keys to cache

// Structure to hold a flattened key-value pair
typedef struct {
    char* key;
    cJSON* value;
} FlattenedPair;

// Array to store flattened key-value pairs with memory pool
typedef struct {
    FlattenedPair* pairs;
    int count;
    int capacity;
    char* memory_pool;      // Pool for small string allocations
    size_t pool_used;       // Bytes used in pool
    size_t pool_size;       // Total pool size
} FlattenedArray;

// Thread data structure
typedef struct {
    cJSON* object;
    cJSON* result;
    pthread_mutex_t* result_mutex;
    char* key_buffer;  // Pre-allocated buffer for key construction
    size_t buffer_size;
} ThreadData;

// Optimized memory allocation from pool with 16-byte alignment
static char* pool_alloc(FlattenedArray* array, size_t size) {
    // Use power-of-2 alignment for better cache performance
    size = (size + 15) & ~15;  // 16-byte alignment instead of 8

    if (array->pool_used + size <= array->pool_size) {
        char* ptr = array->memory_pool + array->pool_used;
        array->pool_used += size;
        return ptr;
    }

    // Allocate larger chunks to reduce fragmentation
    if (size < 1024) {
        return malloc(size);
    } else {
        // For large allocations, use regular malloc (mmap would require platform-specific code)
        return malloc(size);
    }
}

// Initialize the flattened array with optimized memory allocation
void init_flattened_array(FlattenedArray* array, int initial_capacity) {
    // Use larger initial capacity to reduce reallocations
    int capacity = initial_capacity < INITIAL_ARRAY_CAPACITY ? INITIAL_ARRAY_CAPACITY : initial_capacity;

    // Initialize global memory pools if not already done
    if (!g_cjson_node_pool) {
        init_global_pools();
    }

    array->pairs = (FlattenedPair*)malloc(capacity * sizeof(FlattenedPair));
    array->count = 0;
    array->capacity = capacity;

    // Initialize memory pool for small string allocations with optimized size
    // Use the initial_objects parameter we integrated earlier
    size_t pool_size = MEMORY_POOL_SIZE + (initial_capacity * 64); // Estimate 64 bytes per key
    array->memory_pool = (char*)malloc(pool_size);
    array->pool_used = 0;
    array->pool_size = pool_size;
}

// Ultra-optimized key construction with computed goto for better branch prediction
static inline void build_key_ultra_optimized(char* buffer, size_t buffer_size,
                                            const char* prefix, const char* suffix,
                                            int is_array_index, int index) {
#ifdef __GNUC__
    // Use computed goto for better branch prediction (GCC extension)
    static void* dispatch_table[] = {
        &&no_prefix_not_array,
        &&no_prefix_array,
        &&prefix_not_array,
        &&prefix_array
    };

    int dispatch_index = (prefix ? 2 : 0) + (is_array_index ? 1 : 0);
    goto *dispatch_table[dispatch_index];

no_prefix_not_array:
    safe_strncpy(buffer, suffix, buffer_size);
    return;

no_prefix_array:
    snprintf(buffer, buffer_size, "[%d]", index);
    return;

prefix_not_array: {
    size_t prefix_len = strlen_simd(prefix);
    size_t suffix_len = strlen_simd(suffix);
    if (prefix_len + suffix_len + 2 < buffer_size) {
        memcpy(buffer, prefix, prefix_len);
        buffer[prefix_len] = '.';
        memcpy(buffer + prefix_len + 1, suffix, suffix_len + 1);
    } else {
        snprintf(buffer, buffer_size, "%s.%s", prefix, suffix);
    }
    return;
}

prefix_array:
    snprintf(buffer, buffer_size, "%.*s[%d]", (int)strlen_simd(prefix), prefix, index);
    return;
#else
    // Fallback for non-GCC compilers - use simple implementation
    if (!prefix) {
        if (is_array_index) {
            snprintf(buffer, buffer_size, "[%d]", index);
        } else {
            safe_strncpy(buffer, suffix, buffer_size);
        }
        return;
    }

    if (is_array_index) {
        snprintf(buffer, buffer_size, "%s[%d]", prefix, index);
    } else {
        snprintf(buffer, buffer_size, "%s.%s", prefix, suffix);
    }
#endif
}

// Optimized key construction with better performance
static inline void build_key_optimized(char* buffer, size_t buffer_size,
                                     const char* prefix, const char* suffix,
                                     int is_array_index, int index) {
    if (!prefix) {
        if (is_array_index) {
            snprintf(buffer, buffer_size, "[%d]", index);
        } else {
            safe_strncpy(buffer, suffix, buffer_size);
        }
        return;
    }

    size_t prefix_len = strlen_simd(prefix);
    if (is_array_index) {
        // Use faster integer formatting
        if (prefix_len == 0) {
            snprintf(buffer, buffer_size, "[%d]", index);
        } else {
            snprintf(buffer, buffer_size, "%.*s[%d]", (int)prefix_len, prefix, index);
        }
    } else {
        // Use memcpy for better performance than strcat
        size_t suffix_len = strlen_simd(suffix);
        if (prefix_len == 0) {
            // No prefix, just copy the suffix
            if (suffix_len + 1 < buffer_size) {
                memcpy(buffer, suffix, suffix_len + 1);
            }
        } else {
            // Has prefix, add dot separator
            if (prefix_len + suffix_len + 2 < buffer_size) {
                memcpy(buffer, prefix, prefix_len);
                buffer[prefix_len] = '.';
                memcpy(buffer + prefix_len + 1, suffix, suffix_len + 1);
            }
        }
    }
}

// Cache-optimized string duplication using memory pool
HOT_PATH static char* pool_strdup(FlattenedArray* array, const char* str) {
    if (UNLIKELY(!str)) return NULL;

    size_t len = strlen_simd(str) + 1;
    char* new_str;

    // Use pool for small strings (most common case)
    if (LIKELY(len <= 128)) {
        new_str = pool_alloc(array, len);
        if (LIKELY(new_str != NULL)) {
            // Use optimized memory copy
            memcpy(new_str, str, len);
            return new_str;
        }
    }

    // Fall back to global pool or regular allocation
    if (g_cjson_node_pool && len <= 256) {
        new_str = POOL_ALLOC(g_cjson_node_pool);
        if (new_str) {
            memcpy(new_str, str, len);
            return new_str;
        }
    }

    return my_strdup(str);
}

// Cache-friendly pair addition with prefetching
HOT_PATH void add_pair(FlattenedArray* array, const char* key, cJSON* value) {
    // Resize if needed with better growth strategy
    if (UNLIKELY(array->count >= array->capacity)) {
        // Grow by 1.5x instead of 2x to reduce memory waste
        int new_capacity = array->capacity + (array->capacity >> 1);
        FlattenedPair* new_pairs = (FlattenedPair*)realloc(array->pairs, new_capacity * sizeof(FlattenedPair));
        if (UNLIKELY(!new_pairs)) {
            return; // Handle allocation failure gracefully
        }
        array->pairs = new_pairs;
        array->capacity = new_capacity;

        // Prefetch the new memory region
        PREFETCH_WRITE(&array->pairs[array->count]);
    }

    // Add the new pair with optimized string allocation
    FlattenedPair* pair = &array->pairs[array->count];
    pair->key = pool_strdup(array, key);
    pair->value = value;
    array->count++;

    // Prefetch next pair location for better cache performance
    if (LIKELY(array->count < array->capacity)) {
        PREFETCH_WRITE(&array->pairs[array->count]);
    }
}

// Free the flattened array with memory pool cleanup
void free_flattened_array(FlattenedArray* array) {
    // Only free keys that were allocated outside the pool
    for (int i = 0; i < array->count; i++) {
        char* key = array->pairs[i].key;
        if (key && (key < array->memory_pool || key >= array->memory_pool + array->pool_size)) {
            free(key);
        }
    }
    free(array->pairs);
    free(array->memory_pool);
    array->count = 0;
    array->capacity = 0;
    array->pool_used = 0;
    array->pool_size = 0;
}

// Optimized recursive flattening with better string handling
void flatten_json_recursive(cJSON* json, const char* prefix, FlattenedArray* result) {
    if (!json) return;

    char key_buffer[MAX_KEY_LENGTH];

    if (json->type == cJSON_Object) {
        cJSON* child = json->child;
        while (child) {
            build_key_optimized(key_buffer, sizeof(key_buffer), prefix, child->string, 0, 0);
            flatten_json_recursive(child, key_buffer, result);
            child = child->next;
        }
    } else if (json->type == cJSON_Array) {
        cJSON* child = json->child;
        int i = 0;
        while (child) {
            build_key_optimized(key_buffer, sizeof(key_buffer), prefix, NULL, 1, i);
            flatten_json_recursive(child, key_buffer, result);
            child = child->next;
            i++;
        }
    } else {
        add_pair(result, prefix, json);
    }
}

// Create a new flattened JSON object from the flattened array
cJSON* create_flattened_json(FlattenedArray* flattened_array) {
    cJSON* result = cJSON_CreateObject();
    
    for (int i = 0; i < flattened_array->count; i++) {
        FlattenedPair pair = flattened_array->pairs[i];
        
        // Add the appropriate type of value to the result
        switch (pair.value->type) {
            case cJSON_False:
                cJSON_AddFalseToObject(result, pair.key);
                break;
            case cJSON_True:
                cJSON_AddTrueToObject(result, pair.key);
                break;
            case cJSON_NULL:
                cJSON_AddNullToObject(result, pair.key);
                break;
            case cJSON_Number:
                if (pair.value->valueint == pair.value->valuedouble) {
                    cJSON_AddNumberToObject(result, pair.key, pair.value->valueint);
                } else {
                    cJSON_AddNumberToObject(result, pair.key, pair.value->valuedouble);
                }
                break;
            case cJSON_String:
                cJSON_AddStringToObject(result, pair.key, pair.value->valuestring);
                break;
            default:
                // This shouldn't happen as we've already flattened objects and arrays
                break;
        }
    }
    
    return result;
}

// Flatten a single JSON object with optimized initial capacity estimation
cJSON* flatten_single_object(cJSON* json) {
    if (!json) return NULL;

    FlattenedArray flattened_array;
    init_flattened_array(&flattened_array, INITIAL_ARRAY_CAPACITY);

    flatten_json_recursive(json, "", &flattened_array);

    cJSON* flattened_json = create_flattened_json(&flattened_array);
    free_flattened_array(&flattened_array);

    return flattened_json;
}

// Thread worker function for batch flattening
void flatten_object_task(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    // Process the object
    cJSON* flattened = flatten_single_object(data->object);
    
    // Store the result
    if (flattened) {
        data->result = flattened;
    }
    
    // Note: We don't free the thread data here anymore
    // It will be freed after collecting the results
}

// Implementation of the public API functions

/**
 * Flattens a single JSON object
 */
cJSON* flatten_json_object(cJSON* json) {
    return flatten_single_object(json);
}

/**
 * Flattens a batch of JSON objects (array of objects)
 */
cJSON* flatten_json_batch(cJSON* json_array, int use_threads, int num_threads) {
    if (!json_array || json_array->type != cJSON_Array) {
        return NULL;
    }
    
    int array_size = cJSON_GetArraySize(json_array);
    if (array_size == 0) {
        return cJSON_CreateArray();
    }
    
    // Create result array
    cJSON* result = cJSON_CreateArray();
    
    // Determine if multi-threading should be used
    // Only use multi-threading if:
    // 1. Threading is enabled
    // 2. Array size is large enough to justify threading
    // 3. We have more than one thread available
    int should_use_threads = use_threads && 
                            array_size >= MIN_BATCH_SIZE_FOR_MT && 
                            get_optimal_threads(num_threads) > 1;
    
    // If threading is disabled or not beneficial, process sequentially
    if (!should_use_threads) {
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(json_array, i);
            cJSON* flattened = flatten_single_object(item);
            if (flattened) {
                cJSON_AddItemToArray(result, flattened);
            }
        }
        return result;
    }
    
    // Multi-threaded processing with thread pool
    ThreadPool* pool = thread_pool_create(num_threads);
    if (!pool) {
        // Fall back to sequential processing if thread pool creation fails
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(json_array, i);
            cJSON* flattened = flatten_single_object(item);
            if (flattened) {
                cJSON_AddItemToArray(result, flattened);
            }
        }
        return result;
    }
    
    // Create an array of thread data structures
    ThreadData** thread_data_array = (ThreadData**)calloc(array_size, sizeof(ThreadData*));
    if (!thread_data_array) {
        thread_pool_destroy(pool);
        return result; // Return empty array
    }
    
    // Submit tasks to thread pool
    for (int i = 0; i < array_size; i++) {
        ThreadData* data = (ThreadData*)malloc(sizeof(ThreadData));
        if (!data) continue;
        
        data->object = cJSON_GetArrayItem(json_array, i);
        data->result = NULL;
        
        // Store the thread data for later collection
        thread_data_array[i] = data;
        
        // Add task to thread pool
        if (thread_pool_add_task(pool, flatten_object_task, data) != 0) {
            // If adding task fails, process it directly
            flatten_object_task(data);
        }
    }
    
    // Wait for all tasks to complete
    thread_pool_wait(pool);
    
    // Collect results
    for (int i = 0; i < array_size; i++) {
        if (thread_data_array[i] && thread_data_array[i]->result) {
            cJSON_AddItemToArray(result, thread_data_array[i]->result);
        }
        
        // Free the individual thread data
        if (thread_data_array[i]) {
            free(thread_data_array[i]);
        }
    }
    
    // Free the thread data array
    free(thread_data_array);
    
    // Clean up
    thread_pool_destroy(pool);
    
    return result;
}

/**
 * Flattens a JSON string (auto-detects single object or batch)
 */
char* flatten_json_string(const char* json_string, int use_threads, int num_threads) {
    if (!json_string) return NULL;
    
    cJSON* json = cJSON_Parse(json_string);
    if (!json) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            fprintf(stderr, "Error parsing JSON: %s\n", error_ptr);
        }
        return NULL;
    }
    
    cJSON* flattened = NULL;
    
    if (json->type == cJSON_Array) {
        // Check if array contains only primitives (not objects)
        int array_size = cJSON_GetArraySize(json);
        int has_objects = 0;
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(json, i);
            if (item && (item->type == cJSON_Object || item->type == cJSON_Array)) {
                has_objects = 1;
                break;
            }
        }

        if (has_objects) {
            // Array contains objects/arrays, flatten them
            flattened = flatten_json_batch(json, use_threads, num_threads);
        } else {
            // Array contains only primitives, return as-is
            flattened = cJSON_Duplicate(json, 1);
        }
    } else {
        flattened = flatten_json_object(json);
    }
    
    char* result = NULL;
    if (flattened) {
        result = cJSON_Print(flattened);
        cJSON_Delete(flattened);
    }
    
    cJSON_Delete(json);
    return result;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif