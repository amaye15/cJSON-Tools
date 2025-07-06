#ifndef JSON_TOOLS_H
#define JSON_TOOLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <cjson/cJSON.h>

// =============================================================================
// PLATFORM DETECTION AND COMPATIBILITY
// =============================================================================

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

// Cross-platform threading support
#if defined(__WINDOWS__) && !defined(__MINGW32__) && !defined(__MINGW64__)
    // Native Windows (MSVC) - threading disabled for initial PyPI release
    #ifndef THREADING_DISABLED
    #define THREADING_DISABLED
    #endif
    typedef int pthread_t;
    typedef int pthread_mutex_t;
    typedef int pthread_cond_t;
    #define PTHREAD_MUTEX_INITIALIZER 0
    #define PTHREAD_COND_INITIALIZER 0
#elif defined(__MINGW32__) || defined(__MINGW64__)
    // MinGW has real pthread support, but disable threading for initial release
    #define THREADING_DISABLED
    #include <pthread.h>
#else
    // Unix-like systems with full pthread support
    #include <pthread.h>
#endif

// =============================================================================
// COMPILER HINTS AND OPTIMIZATIONS
// =============================================================================

#ifdef _MSC_VER
// MSVC-specific definitions
#include <intrin.h>
#include <emmintrin.h>

#define FLATTEN
#define NO_INLINE __declspec(noinline)
#define ALWAYS_INLINE __forceinline
#define PURE_FUNC
#define CONST_FUNC
#define HOT_PATH
#define COLD_PATH
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#define ASSUME_ALIGNED(ptr, alignment) __assume((uintptr_t)(ptr) % (alignment) == 0)
#define PREFETCH_READ(ptr) _mm_prefetch((char*)(ptr), _MM_HINT_T0)
#define PREFETCH_WRITE(ptr) _mm_prefetch((char*)(ptr), _MM_HINT_T0)
#define RESTRICT __restrict
#define PACKED
#define ALIGNED(n) __declspec(align(n))

#elif defined(__GNUC__)
// GCC-specific definitions
#define FLATTEN __attribute__((flatten))
#define NO_INLINE __attribute__((noinline))
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define PURE_FUNC __attribute__((pure))
#define CONST_FUNC __attribute__((const))
#define HOT_PATH __attribute__((hot))
#define COLD_PATH __attribute__((cold))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ASSUME_ALIGNED(ptr, alignment) __builtin_assume_aligned(ptr, alignment)
#define PREFETCH_READ(ptr) __builtin_prefetch(ptr, 0, 3)
#define PREFETCH_WRITE(ptr) __builtin_prefetch(ptr, 1, 3)
#define RESTRICT __restrict__
#define PACKED __attribute__((packed))
#define ALIGNED(n) __attribute__((aligned(n)))

#else
// Fallback for other compilers
#define FLATTEN
#define NO_INLINE
#define ALWAYS_INLINE inline
#define PURE_FUNC
#define CONST_FUNC
#define HOT_PATH
#define COLD_PATH
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#define ASSUME_ALIGNED(ptr, alignment) (ptr)
#define PREFETCH_READ(ptr)
#define PREFETCH_WRITE(ptr)
#define RESTRICT
#define PACKED
#define ALIGNED(n)
#endif

// Cache line size constants
#define CACHE_LINE_SIZE 64
#define CACHE_ALIGNED ALIGNED(CACHE_LINE_SIZE)

// Force inline for critical path functions
#define FORCE_INLINE ALWAYS_INLINE

// CPU relaxation for spin loops
ALWAYS_INLINE static void cpu_relax(void) {
#if defined(_MSC_VER)
    #if defined(_M_X64) || defined(_M_IX86)
        _mm_pause();
    #else
        (void)0;
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #ifdef __x86_64__
        __asm__ volatile("pause" ::: "memory");
    #elif defined(__aarch64__)
        __asm__ volatile("yield" ::: "memory");
    #elif defined(__powerpc__)
        __asm__ volatile("or 27,27,27" ::: "memory");
    #else
        __asm__ volatile("" ::: "memory");
    #endif
#else
    (void)0;
#endif
}

// =============================================================================
// MEMORY MANAGEMENT AND POOLS
// =============================================================================

// High-performance slab allocator for fixed-size objects
typedef struct {
    void* memory;
    void* free_list;
    size_t object_size;
    size_t objects_per_slab;
    size_t total_slabs;
    size_t allocated_objects;
    bool use_huge_pages;
} SlabAllocator;

// Global object pools for common allocations
extern SlabAllocator* g_cjson_node_pool;
extern SlabAllocator* g_property_node_pool;
extern SlabAllocator* g_task_pool;

// Convenience macros
#define POOL_ALLOC(pool) slab_alloc(pool)
#define POOL_FREE(pool, ptr) slab_free(pool, ptr)

// Memory pool functions
SlabAllocator* slab_allocator_create(size_t object_size, size_t initial_objects);
void* slab_alloc(SlabAllocator* allocator);
void slab_free(SlabAllocator* allocator, void* ptr);
void slab_allocator_destroy(SlabAllocator* allocator);
void init_global_pools(void);
void cleanup_global_pools(void);

// =============================================================================
// STRING VIEW (ZERO-COPY STRING OPERATIONS)
// =============================================================================

// Zero-copy string view for avoiding allocations
typedef struct {
    const char* data;
    size_t length;
    uint32_t hash;  // Cached hash value
} StringView;

// String view functions
ALWAYS_INLINE static StringView make_string_view(const char* str, size_t len) {
    StringView sv = {str, len, 0};
    return sv;
}

ALWAYS_INLINE static StringView make_string_view_cstr(const char* str) {
    return make_string_view(str, strlen(str));
}

// Fast hash function using FNV-1a algorithm
ALWAYS_INLINE static uint32_t string_view_hash(const StringView* sv) {
    if (sv->hash == 0 && sv->length > 0) {
        uint32_t hash = 2166136261u;  // FNV offset basis
        for (size_t i = 0; i < sv->length; i++) {
            hash ^= (uint8_t)sv->data[i];
            hash *= 16777619u;  // FNV prime
        }
        ((StringView*)sv)->hash = hash ? hash : 1;  // Avoid 0
    }
    return sv->hash;
}

// String view utility functions
int string_view_equals(const StringView* a, const StringView* b);
int string_view_equals_cstr(const StringView* sv, const char* str);
int string_view_starts_with(const StringView* sv, const char* prefix);
int string_view_ends_with(const StringView* sv, const char* suffix);
const char* string_view_find_char(const StringView* sv, char c);
StringView string_view_substr(const StringView* sv, size_t start, size_t len);
char* string_view_to_cstr(const StringView* sv);

// =============================================================================
// SIMD OPTIMIZATIONS
// =============================================================================

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

// =============================================================================
// THREAD POOL AND TASK MANAGEMENT
// =============================================================================

/**
 * Task structure for thread pool
 */
typedef struct Task {
    void (*function)(void*);  // Function to execute
    void* argument;           // Argument to pass to the function
    struct Task* next;        // Next task in the queue
} Task;

/**
 * Thread pool structure
 */
typedef struct {
    pthread_t* threads;           // Array of worker threads
    Task* task_queue;             // Queue of tasks to be executed
    Task* task_queue_tail;        // Tail of the task queue for faster enqueuing
    int num_threads;              // Number of threads in the pool
    int active_threads;           // Number of currently active threads
    bool shutdown;                // Flag to indicate shutdown
    pthread_mutex_t queue_mutex;  // Mutex to protect the task queue
    pthread_cond_t queue_cond;    // Condition variable for task queue
    pthread_cond_t idle_cond;     // Condition variable for idle threads
} ThreadPool;

// Thread pool functions
ThreadPool* thread_pool_create(int num_threads);
int thread_pool_add_task(ThreadPool* pool, void (*function)(void*), void* argument);
void thread_pool_destroy(ThreadPool* pool);
void thread_pool_wait(ThreadPool* pool);
size_t thread_pool_get_queue_size(ThreadPool* pool);
int thread_pool_get_thread_count(ThreadPool* pool);

// Global task queue functions
size_t get_task_queue_size(void);

// =============================================================================
// JSON UTILITIES
// =============================================================================

/**
 * Custom string duplication function (replacement for strdup)
 */
char* my_strdup(const char* str) HOT_PATH;

/**
 * Reads a JSON file into a string
 */
char* read_json_file(const char* filename);

/**
 * Reads JSON from stdin into a string
 */
char* read_json_stdin(void);

/**
 * Determines the number of CPU cores available
 */
int get_num_cores(void) CONST_FUNC;

/**
 * Determines the optimal number of threads to use
 */
int get_optimal_threads(int requested_threads) PURE_FUNC;

/**
 * Removes all keys that have empty string values from a JSON object
 */
cJSON* remove_empty_strings(const cJSON* json) HOT_PATH;

/**
 * Removes all keys that have null values from a JSON object
 */
cJSON* remove_nulls(const cJSON* json) HOT_PATH;

/**
 * Replaces JSON keys that match a regex pattern with a replacement string
 */
cJSON* replace_keys(const cJSON* json, const char* pattern, const char* replacement) HOT_PATH;

/**
 * Replaces JSON string values that match a regex pattern with a replacement string
 */
cJSON* replace_values(const cJSON* json, const char* pattern, const char* replacement) HOT_PATH;

// =============================================================================
// JSON FLATTENER
// =============================================================================

/**
 * Flattens a single JSON object
 * 
 * @param json The JSON object to flatten
 * @return A new flattened JSON object (must be freed by caller)
 */
cJSON* flatten_json_object(cJSON* json);

/**
 * Flattens a batch of JSON objects (array of objects)
 * 
 * @param json_array The JSON array to flatten
 * @param use_threads Whether to use multi-threading
 * @param num_threads Number of threads to use (0 for auto-detection)
 * @return A new flattened JSON array (must be freed by caller)
 */
cJSON* flatten_json_batch(cJSON* json_array, int use_threads, int num_threads);

/**
 * Flattens a JSON string (auto-detects single object or batch)
 *
 * @param json_string The JSON string to flatten
 * @param use_threads Whether to use multi-threading
 * @param num_threads Number of threads to use (0 for auto-detection)
 * @return A new flattened JSON string (must be freed by caller)
 */
char* flatten_json_string(const char* json_string, int use_threads, int num_threads);

/**
 * Gets flattened paths with their data types from a JSON object
 *
 * @param json The JSON object to analyze
 * @return A new JSON object with paths as keys and data types as values (must be freed by caller)
 */
cJSON* get_flattened_paths_with_types(cJSON* json);

/**
 * Gets flattened paths with their data types from a JSON string
 *
 * @param json_string The JSON string to analyze
 * @return A new JSON string with paths as keys and data types as values (must be freed by caller)
 */
char* get_flattened_paths_with_types_string(const char* json_string);

// =============================================================================
// JSON SCHEMA GENERATOR
// =============================================================================

/**
 * Generates a JSON schema from a single JSON object
 * 
 * @param json The JSON object to analyze
 * @return A new JSON schema object (must be freed by caller)
 */
cJSON* generate_schema_from_object(cJSON* json);

/**
 * Generates a JSON schema from multiple JSON objects
 * 
 * @param json_array The array of JSON objects to analyze
 * @param use_threads Whether to use multi-threading
 * @param num_threads Number of threads to use (0 for auto-detection)
 * @return A new JSON schema object (must be freed by caller)
 */
cJSON* generate_schema_from_batch(cJSON* json_array, int use_threads, int num_threads);

/**
 * Generates a JSON schema from a JSON string (auto-detects single object or batch)
 * 
 * @param json_string The JSON string to analyze
 * @param use_threads Whether to use multi-threading
 * @param num_threads Number of threads to use (0 for auto-detection)
 * @return A new JSON schema string (must be freed by caller)
 */
char* generate_schema_from_string(const char* json_string, int use_threads, int num_threads);

// =============================================================================
// WINDOWS PTHREAD COMPATIBILITY (when threading is disabled)
// =============================================================================

#if defined(THREADING_DISABLED) && defined(__WINDOWS__) && !defined(__MINGW32__) && !defined(__MINGW64__)
// Native Windows (MSVC) pthread function declarations (implemented as no-ops)
int pthread_create(pthread_t* thread, void* attr, void* (*start_routine)(void*), void* arg);
int pthread_join(pthread_t thread, void** retval);
void pthread_exit(void* retval);
int pthread_mutex_init(pthread_mutex_t* mutex, void* attr);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_cond_init(pthread_cond_t* cond, void* attr);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_cond_destroy(pthread_cond_t* cond);
#endif

// =============================================================================
// CONSTANTS AND CONFIGURATION
// =============================================================================

// Performance tuning constants
#define MIN_OBJECTS_PER_THREAD 25   // Reduced for better parallelization
#define MIN_BATCH_SIZE_FOR_MT 100   // Reduced threshold for multi-threading
#define INITIAL_ARRAY_CAPACITY 64   // Larger initial capacity
#define KEY_BUFFER_SIZE 512         // Pre-allocated key buffer size
#define MEMORY_POOL_SIZE 8192       // Memory pool for small allocations
#define MAX_CACHED_KEYS 256         // Maximum number of keys to cache
#define MAX_KEY_LENGTH 2048         // Maximum length for JSON keys
#define BATCH_SIZE 1000             // Default batch processing size
#define MAX_ARRAY_SAMPLE_SIZE 50    // Maximum array items to sample for type inference

#ifdef __cplusplus
}
#endif

#endif /* JSON_TOOLS_H */