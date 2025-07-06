#include "cjson_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <time.h>

// Platform-specific includes
#ifdef __WINDOWS__
    #include <windows.h>
    #include <process.h>
    #ifdef _MSC_VER
        #include <malloc.h>  // For _aligned_malloc and _aligned_free
    #endif
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #ifndef THREADING_DISABLED
        #include <pthread.h>
    #endif
    #ifdef __unix__
        #include <sys/mman.h>
    #endif
    #ifndef __WINDOWS__
        #include <regex.h>
    #endif
#endif

// SIMD includes (conditionally compiled)
#ifndef THREADING_DISABLED
    #ifdef __SSE2__
        #include <emmintrin.h>
    #endif
    #ifdef __AVX2__
        #include <immintrin.h>
    #endif
    #ifdef __aarch64__
        #include <arm_neon.h>
    #endif
#endif

// Apple-specific includes
#ifdef __APPLE__
    #include <sys/sysctl.h>
#endif

// MSVC-safe string operations
#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
    #pragma warning(push)
    #pragma warning(disable: 4996) // Disable unsafe function warnings
#endif

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

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Global object pools
SlabAllocator* g_cjson_node_pool = NULL;
SlabAllocator* g_property_node_pool = NULL;
SlabAllocator* g_task_pool = NULL;

// =============================================================================
// PLATFORM-SPECIFIC SYSTEM CALLS
// =============================================================================

#ifdef __WINDOWS__
// Simplified Windows implementation
static long msvc_sysconf(int name) {
    (void)name; // Suppress unused parameter warning
    return 4; // Default to 4 cores for Windows
}
#define sysconf(x) msvc_sysconf(x)
#define _SC_NPROCESSORS_ONLN 1
#endif

// =============================================================================
// STRING VIEW IMPLEMENTATION
// =============================================================================

int string_view_equals(const StringView* a, const StringView* b) {
    if (a->length != b->length) return 0;
    if (a->data == b->data) return 1;  // Same pointer
    
    // Compare hashes first (fast rejection)
    if (string_view_hash(a) != string_view_hash(b)) return 0;
    
    return memcmp(a->data, b->data, a->length) == 0;
}

int string_view_equals_cstr(const StringView* sv, const char* str) {
    size_t str_len = strlen(str);
    if (sv->length != str_len) return 0;
    return memcmp(sv->data, str, str_len) == 0;
}

int string_view_starts_with(const StringView* sv, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    if (sv->length < prefix_len) return 0;
    return memcmp(sv->data, prefix, prefix_len) == 0;
}

int string_view_ends_with(const StringView* sv, const char* suffix) {
    size_t suffix_len = strlen(suffix);
    if (sv->length < suffix_len) return 0;
    return memcmp(sv->data + sv->length - suffix_len, suffix, suffix_len) == 0;
}

const char* string_view_find_char(const StringView* sv, char c) {
    return (const char*)memchr(sv->data, c, sv->length);
}

StringView string_view_substr(const StringView* sv, size_t start, size_t len) {
    if (start >= sv->length) {
        return make_string_view("", 0);
    }
    
    size_t actual_len = (start + len > sv->length) ? sv->length - start : len;
    return make_string_view(sv->data + start, actual_len);
}

char* string_view_to_cstr(const StringView* sv) {
    char* str = malloc(sv->length + 1);
    if (str) {
        memcpy(str, sv->data, sv->length);
        str[sv->length] = '\0';
    }
    return str;
}

// =============================================================================
// SIMD OPTIMIZATIONS
// =============================================================================

// Runtime CPU feature detection
static int has_avx2 = -1;
static int has_sse2 = -1;
static int has_neon = -1;

static void detect_cpu_features(void) {
    if (has_avx2 == -1) {
#ifdef __x86_64__
        #ifdef __GNUC__
        has_avx2 = __builtin_cpu_supports("avx2") ? 1 : 0;
        has_sse2 = __builtin_cpu_supports("sse2") ? 1 : 0;
        #else
        has_avx2 = 0;
        has_sse2 = 0;
        #endif
#elif defined(__aarch64__)
        has_neon = 1;
        has_avx2 = 0;
        has_sse2 = 0;
#else
        has_avx2 = 0;
        has_sse2 = 0;
        has_neon = 0;
#endif
    }
}

#ifdef __aarch64__
static size_t strlen_simd_neon(const char* str) {
    const uint8x16_t zero = vdupq_n_u8(0);
    size_t len = 0;

    while (1) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*)(str + len));
        uint8x16_t cmp = vceqq_u8(chunk, zero);

        uint64x2_t mask = vreinterpretq_u64_u8(cmp);
        uint64_t low = vgetq_lane_u64(mask, 0);
        uint64_t high = vgetq_lane_u64(mask, 1);

        if (low || high) {
            for (int i = 0; i < 16; i++) {
                if (str[len + i] == 0) return len + i;
            }
        }
        len += 16;

        if (len > 1000000) break; // Safety check
    }

    return strlen(str);
}
#endif

size_t strlen_simd(const char* str) {
    if (!str) return 0;

    detect_cpu_features();

#ifdef __aarch64__
    if (has_neon) {
        return strlen_simd_neon(str);
    }
#else
    (void)has_neon;
#endif

#ifdef __AVX2__
    if (has_avx2) {
        const char* start = str;
        const char* ptr = str;
        
        while (*ptr && (ptr - str) < 32) {
            ptr++;
        }

        if (*ptr == '\0') {
            return ptr - str;
        }

        const __m256i zero = _mm256_setzero_si256();
        size_t len = 0;

        while (1) {
            __m256i chunk = _mm256_loadu_si256((__m256i*)(str + len));
            __m256i cmp = _mm256_cmpeq_epi8(chunk, zero);
            uint32_t mask = _mm256_movemask_epi8(cmp);

            if (mask != 0) {
                return len + __builtin_ctz(mask);
            }
            len += 32;
        }
    }
#endif

#ifdef __SSE2__
    if (has_sse2) {
        const __m128i zero = _mm_setzero_si128();
        size_t len = 0;

        while (1) {
            __m128i chunk = _mm_loadu_si128((__m128i*)(str + len));
            __m128i cmp = _mm_cmpeq_epi8(chunk, zero);
            uint32_t mask = _mm_movemask_epi8(cmp);

            if (mask != 0) {
                return len + __builtin_ctz(mask);
            }
            len += 16;
        }
    }
#endif

    return strlen(str);
}

const char* skip_whitespace_optimized(const char* str, size_t len) {
#ifdef __AVX2__
    if (has_avx2) {
        const __m256i spaces = _mm256_set1_epi8(' ');
        const __m256i tabs = _mm256_set1_epi8('\t');
        const __m256i newlines = _mm256_set1_epi8('\n');
        const __m256i returns = _mm256_set1_epi8('\r');
        
        size_t i = 0;
        for (; i + 32 <= len; i += 32) {
            __m256i chunk = _mm256_loadu_si256((__m256i*)(str + i));
            
            __m256i is_space = _mm256_cmpeq_epi8(chunk, spaces);
            __m256i is_tab = _mm256_cmpeq_epi8(chunk, tabs);
            __m256i is_newline = _mm256_cmpeq_epi8(chunk, newlines);
            __m256i is_return = _mm256_cmpeq_epi8(chunk, returns);
            
            __m256i is_whitespace = _mm256_or_si256(
                _mm256_or_si256(is_space, is_tab),
                _mm256_or_si256(is_newline, is_return)
            );
            
            uint32_t mask = ~_mm256_movemask_epi8(is_whitespace);
            if (mask != 0) {
                return str + i + __builtin_ctz(mask);
            }
        }
        
        for (; i < len; i++) {
            char c = str[i];
            if (LIKELY(c != ' ' && c != '\t' && c != '\n' && c != '\r')) {
                return str + i;
            }
        }
    }
#endif

    // Fallback scalar implementation
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (LIKELY(c != ' ' && c != '\t' && c != '\n' && c != '\r')) {
            return str + i;
        }
    }
    return str + len;
}

const char* find_delimiter_optimized(const char* str, size_t len) {
    // Fallback scalar implementation (SIMD version omitted for brevity)
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (c == '"' || c == ',' || c == ':' || c == '{' || c == '}' || c == '[' || c == ']') {
            return str + i;
        }
    }
    return str + len;
}

// =============================================================================
// MEMORY POOL IMPLEMENTATION
// =============================================================================

// CPU cache line detection
static size_t get_cache_line_size(void) {
    static size_t cache_line_size = 0;
    if (cache_line_size == 0) {
#if defined(__linux__)
        #ifdef _SC_LEVEL1_DCACHE_LINESIZE
        long size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
        cache_line_size = (size > 0) ? (size_t)size : 64;
        #else
        cache_line_size = 64;
        #endif
#elif defined(__WINDOWS__)
        cache_line_size = 64; // Safe default for Windows
#else
        cache_line_size = 64; // Default assumption
#endif
    }
    return cache_line_size;
}

// Aligned allocation compatibility
#ifdef _MSC_VER
static inline void* my_aligned_alloc(size_t alignment, size_t size) {
    return _aligned_malloc(size, alignment);
}
#define aligned_alloc my_aligned_alloc
#elif !defined(__APPLE__) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L)
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
static inline void* my_aligned_alloc(size_t alignment, size_t size) {
    void* ptr;
    if (posix_memalign(&ptr, alignment, size) == 0) {
        return ptr;
    }
    return NULL;
}
#define aligned_alloc my_aligned_alloc
#else
static inline void* my_aligned_alloc(size_t alignment, size_t size) {
    (void)alignment;
    return malloc(size);
}
#define aligned_alloc my_aligned_alloc
#endif
#endif

SlabAllocator* slab_allocator_create(size_t object_size, size_t initial_objects) {
    SlabAllocator* allocator = malloc(sizeof(SlabAllocator));
    if (!allocator) return NULL;

    allocator->object_size = (object_size + 63) & ~63;  // 64-byte alignment

    size_t optimal_slab_size = 4096;
    allocator->objects_per_slab = optimal_slab_size / allocator->object_size;
    if (allocator->objects_per_slab < 1) allocator->objects_per_slab = 1;

    size_t slabs_needed = (initial_objects + allocator->objects_per_slab - 1) / allocator->objects_per_slab;
    if (slabs_needed < 1) slabs_needed = 1;

    size_t slab_size = allocator->object_size * allocator->objects_per_slab * slabs_needed;
    allocator->use_huge_pages = false;
    
#ifdef __unix__
    allocator->memory = mmap(NULL, slab_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (allocator->memory != MAP_FAILED) {
        allocator->use_huge_pages = true;
    } else {
        size_t alignment = get_cache_line_size();
        #if defined(__APPLE__) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
        if (posix_memalign(&allocator->memory, alignment, slab_size) != 0) {
            allocator->memory = NULL;
        }
        #else
        allocator->memory = aligned_alloc(alignment, slab_size);
        #endif
    }
#else
    size_t alignment = get_cache_line_size();
    #if defined(__APPLE__) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
    if (posix_memalign(&allocator->memory, alignment, slab_size) != 0) {
        allocator->memory = NULL;
    }
    #else
    allocator->memory = aligned_alloc(alignment, slab_size);
    #endif
#endif

    if (!allocator->memory) {
        free(allocator);
        return NULL;
    }

    // Initialize free list
    allocator->free_list = allocator->memory;
    for (size_t i = 0; i < allocator->objects_per_slab - 1; i++) {
        void** current = (void**)((char*)allocator->memory + i * allocator->object_size);
        *current = (char*)allocator->memory + (i + 1) * allocator->object_size;
    }
    
    void** last = (void**)((char*)allocator->memory + 
                          (allocator->objects_per_slab - 1) * allocator->object_size);
    *last = NULL;

    allocator->total_slabs = 1;
    allocator->allocated_objects = 0;

    return allocator;
}

void* slab_alloc(SlabAllocator* allocator) {
    if (!allocator) return malloc(64);

#ifndef THREADING_DISABLED
    void* old_head;
    void* new_head;

    do {
        old_head = __atomic_load_n(&allocator->free_list, __ATOMIC_ACQUIRE);
        if (!old_head) {
            return malloc(allocator->object_size);
        }

        new_head = *(void**)old_head;
    } while (!__atomic_compare_exchange_n(&allocator->free_list, &old_head, new_head,
                                         false, __ATOMIC_RELEASE, __ATOMIC_RELAXED));

    __atomic_fetch_add(&allocator->allocated_objects, 1, __ATOMIC_RELAXED);
    return old_head;
#else
    void* old_head = allocator->free_list;
    if (!old_head) {
        return malloc(allocator->object_size);
    }

    allocator->free_list = *(void**)old_head;
    allocator->allocated_objects++;
    return old_head;
#endif
}

void slab_free(SlabAllocator* allocator, void* ptr) {
    if (!allocator || !ptr) {
        free(ptr);
        return;
    }
    
    char* pool_start = (char*)allocator->memory;
    char* pool_end = pool_start + (allocator->object_size * allocator->objects_per_slab);
    
    if (ptr < (void*)pool_start || ptr >= (void*)pool_end) {
        free(ptr);
        return;
    }
    
#ifndef THREADING_DISABLED
    void* old_head;
    do {
        old_head = __atomic_load_n(&allocator->free_list, __ATOMIC_ACQUIRE);
        *(void**)ptr = old_head;
    } while (!__atomic_compare_exchange_n(&allocator->free_list, &old_head, ptr,
                                         false, __ATOMIC_RELEASE, __ATOMIC_RELAXED));

    __atomic_fetch_sub(&allocator->allocated_objects, 1, __ATOMIC_RELAXED);
#else
    *(void**)ptr = allocator->free_list;
    allocator->free_list = ptr;
    allocator->allocated_objects--;
#endif
}

void slab_allocator_destroy(SlabAllocator* allocator) {
    if (!allocator) return;
    
#ifdef __unix__
    if (allocator->use_huge_pages) {
        size_t slab_size = allocator->object_size * allocator->objects_per_slab;
        munmap(allocator->memory, slab_size);
    } else {
        free(allocator->memory);
    }
#elif defined(_MSC_VER)
    _aligned_free(allocator->memory);
#else
    free(allocator->memory);
#endif
    
    free(allocator);
}

void init_global_pools(void) {
    if (g_cjson_node_pool != NULL) {
        return;
    }

    g_cjson_node_pool = slab_allocator_create(256, 1000);
    g_property_node_pool = slab_allocator_create(128, 500);
    g_task_pool = slab_allocator_create(64, 200);
}

void cleanup_global_pools(void) {
    if (g_cjson_node_pool) {
        slab_allocator_destroy(g_cjson_node_pool);
        g_cjson_node_pool = NULL;
    }
    if (g_property_node_pool) {
        slab_allocator_destroy(g_property_node_pool);
        g_property_node_pool = NULL;
    }
    if (g_task_pool) {
        slab_allocator_destroy(g_task_pool);
        g_task_pool = NULL;
    }
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

#ifdef __SSE2__
static char* my_strdup_simd(const char* str) {
    if (UNLIKELY(str == NULL)) return NULL;

    size_t len = strlen_simd(str);
    size_t aligned_size = (len + 16) & ~15;
    char* new_str;

    if (posix_memalign((void**)&new_str, 16, aligned_size) != 0) {
        return NULL;
    }

    if (UNLIKELY(new_str == NULL)) return NULL;

    size_t simd_len = len & ~15;
    for (size_t i = 0; i < simd_len; i += 16) {
        __m128i chunk = _mm_loadu_si128((__m128i*)(str + i));
        _mm_store_si128((__m128i*)(new_str + i), chunk);
    }

    memcpy(new_str + simd_len, str + simd_len, len - simd_len + 1);
    return new_str;
}
#endif

char* my_strdup(const char* str) {
    if (UNLIKELY(str == NULL)) return NULL;

    size_t len = strlen_simd(str) + 1;

#ifdef __SSE2__
    if (len > 32) {
        return my_strdup_simd(str);
    }
#endif

    char* new_str = (char*)malloc(len);
    if (UNLIKELY(new_str == NULL)) return NULL;

    return (char*)memcpy(new_str, str, len);
}

static char* read_json_file_mmap(const char* filename) {
    (void)filename;
#ifdef __unix__
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return NULL;

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return NULL;
    }

    if (st.st_size > 65536) {
        void* mapped = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        if (mapped == MAP_FAILED) return NULL;

        char* buffer = malloc(st.st_size + 1);
        if (buffer) {
            memcpy(buffer, mapped, st.st_size);
            buffer[st.st_size] = '\0';
        }
        munmap(mapped, st.st_size);

        return buffer;
    }

    close(fd);
#endif
    return NULL;
}

char* read_json_file(const char* filename) {
    char* result = read_json_file_mmap(filename);
    if (result) return result;

    FILE* file = fopen(filename, "rb");
    if (UNLIKELY(!file)) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }

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

    char* buffer = (char*)malloc(file_size + 1);
    if (UNLIKELY(!buffer)) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';
    fclose(file);

    return buffer;
}

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

int get_num_cores(void) {
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    return (num_cores > 0) ? (int)num_cores : 1;
}

int get_optimal_threads(int requested_threads) {
    if (LIKELY(requested_threads > 0)) {
        return requested_threads > 64 ? 64 : requested_threads;
    }

    int num_cores = get_num_cores();

    if (num_cores <= 2) {
        return num_cores;
    } else if (num_cores <= 4) {
        return num_cores - 1;
    } else if (num_cores <= 8) {
        return (num_cores * 3) / 4;
    } else {
        return (num_cores / 2) + 2;
    }
}

// =============================================================================
// WINDOWS PTHREAD COMPATIBILITY (when threading is disabled)
// =============================================================================

#if defined(THREADING_DISABLED) && defined(__WINDOWS__) && !defined(__MINGW32__) && !defined(__MINGW64__)

int pthread_create(pthread_t* thread, void* attr, void* (*start_routine)(void*), void* arg) {
    (void)thread; (void)attr;
    start_routine(arg);
    return 0;
}

int pthread_join(pthread_t thread, void** retval) {
    (void)thread; (void)retval;
    return 0;
}

void pthread_exit(void* retval) {
    (void)retval;
}

int pthread_mutex_init(pthread_mutex_t* mutex, void* attr) {
    (void)mutex; (void)attr;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0;
}

int pthread_cond_init(pthread_cond_t* cond, void* attr) {
    (void)cond; (void)attr;
    return 0;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    (void)cond; (void)mutex;
    return 0;
}

int pthread_cond_signal(pthread_cond_t* cond) {
    (void)cond;
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    (void)cond;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
    (void)cond;
    return 0;
}

#endif

// =============================================================================
// THREAD POOL IMPLEMENTATION
// =============================================================================

// Global task queue size tracking
static size_t g_task_queue_size = 0;

size_t get_task_queue_size(void) {
    return g_task_queue_size;
}

static void* worker_thread(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    Task* task;

    while (1) {
        pthread_mutex_lock(&pool->queue_mutex);

        while (__builtin_expect(pool->task_queue == NULL && !pool->shutdown, 0)) {
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
        }

        if (__builtin_expect(pool->shutdown && pool->task_queue == NULL, 0)) {
            pthread_mutex_unlock(&pool->queue_mutex);
            pthread_exit(NULL);
        }

        task = pool->task_queue;
        if (__builtin_expect(task != NULL, 1)) {
            pool->task_queue = task->next;
            if (__builtin_expect(pool->task_queue == NULL, 0)) {
                pool->task_queue_tail = NULL;
            }
            pool->active_threads++;
            g_task_queue_size--;
        }

        pthread_mutex_unlock(&pool->queue_mutex);

        if (__builtin_expect(task != NULL, 1)) {
            task->function(task->argument);
            free(task);

            pthread_mutex_lock(&pool->queue_mutex);
            pool->active_threads--;
            if (__builtin_expect(pool->active_threads == 0, 0)) {
                pthread_cond_signal(&pool->idle_cond);
            }
            pthread_mutex_unlock(&pool->queue_mutex);
        }
    }

    return NULL;
}

ThreadPool* thread_pool_create(int num_threads) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (pool == NULL) {
        return NULL;
    }
    
    pool->task_queue = NULL;
    pool->task_queue_tail = NULL;
    pool->shutdown = false;
    pool->active_threads = 0;
    
    pool->num_threads = get_optimal_threads(num_threads);
    
    pthread_mutex_init(&pool->queue_mutex, NULL);
    pthread_cond_init(&pool->queue_cond, NULL);
    pthread_cond_init(&pool->idle_cond, NULL);
    
    pool->threads = (pthread_t*)malloc(pool->num_threads * sizeof(pthread_t));
    if (pool->threads == NULL) {
        free(pool);
        return NULL;
    }
    
    for (int i = 0; i < pool->num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            thread_pool_destroy(pool);
            return NULL;
        }
    }
    
    return pool;
}

int thread_pool_add_task(ThreadPool* pool, void (*function)(void*), void* argument) {
    if (__builtin_expect(pool == NULL || function == NULL, 0)) {
        return -1;
    }

    Task* task = (Task*)malloc(sizeof(Task));
    if (__builtin_expect(task == NULL, 0)) {
        return -1;
    }

    task->function = function;
    task->argument = argument;
    task->next = NULL;

    pthread_mutex_lock(&pool->queue_mutex);

    if (__builtin_expect(pool->task_queue == NULL, 0)) {
        pool->task_queue = task;
        pool->task_queue_tail = task;
    } else {
        pool->task_queue_tail->next = task;
        pool->task_queue_tail = task;
    }

    g_task_queue_size++;
    pthread_cond_signal(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);

    return 0;
}

void thread_pool_wait(ThreadPool* pool) {
    if (pool == NULL) {
        return;
    }
    
    pthread_mutex_lock(&pool->queue_mutex);
    
    while (pool->task_queue != NULL || pool->active_threads > 0) {
        pthread_cond_wait(&pool->idle_cond, &pool->queue_mutex);
    }
    
    pthread_mutex_unlock(&pool->queue_mutex);
}

void thread_pool_destroy(ThreadPool* pool) {
    if (pool == NULL) {
        return;
    }
    
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    free(pool->threads);
    
    Task* task = pool->task_queue;
    while (task != NULL) {
        Task* next = task->next;
        free(task);
        task = next;
    }
    
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    pthread_cond_destroy(&pool->idle_cond);
    
    free(pool);
}

int thread_pool_get_thread_count(ThreadPool* pool) {
    if (pool == NULL) {
        return 0;
    }
    return pool->num_threads;
}

size_t thread_pool_get_queue_size(ThreadPool* pool) {
    if (pool == NULL) {
        return 0;
    }

    size_t count = 0;
    pthread_mutex_lock(&pool->queue_mutex);
    Task* current = pool->task_queue;
    while (current != NULL && count < 1000) {
        current = current->next;
        count++;
    }
    pthread_mutex_unlock(&pool->queue_mutex);

    return count + get_task_queue_size();
}

// =============================================================================
// JSON UTILITY FUNCTIONS
// =============================================================================

static cJSON* filter_json_recursive(const cJSON* json, int remove_empty_strings, int remove_nulls) {
    if (UNLIKELY(json == NULL)) return NULL;

    if (cJSON_IsObject(json)) {
        cJSON* new_obj = cJSON_CreateObject();
        if (UNLIKELY(!new_obj)) return NULL;

        const cJSON* child = json->child;
        while (child) {
            int should_skip = 0;

            if (remove_empty_strings && cJSON_IsString(child) &&
                child->valuestring && strlen_simd(child->valuestring) == 0) {
                should_skip = 1;
            }

            if (remove_nulls && cJSON_IsNull(child)) {
                should_skip = 1;
            }

            if (!should_skip) {
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

            if (remove_empty_strings && cJSON_IsString(child) &&
                child->valuestring && strlen_simd(child->valuestring) == 0) {
                should_skip = 1;
            }

            if (remove_nulls && cJSON_IsNull(child)) {
                should_skip = 1;
            }

            if (!should_skip) {
                cJSON* filtered_value = filter_json_recursive(child, remove_empty_strings, remove_nulls);
                if (filtered_value) {
                    cJSON_AddItemToArray(new_array, filtered_value);
                }
            }

            child = child->next;
        }

        return new_array;
    } else {
        return cJSON_Duplicate(json, 1);
    }
}

cJSON* remove_empty_strings(const cJSON* json) {
    if (UNLIKELY(json == NULL)) return NULL;
    return filter_json_recursive(json, 1, 0);
}

cJSON* remove_nulls(const cJSON* json) {
    if (UNLIKELY(json == NULL)) return NULL;
    return filter_json_recursive(json, 0, 1);
}

#ifndef __WINDOWS__
static cJSON* replace_keys_recursive(const cJSON* json, const char* pattern, const char* replacement) {
    if (UNLIKELY(json == NULL)) return NULL;

    regex_t regex;
    int regex_result = regcomp(&regex, pattern, REG_EXTENDED);
    if (regex_result != 0) {
        return cJSON_Duplicate(json, 1);
    }

    if (cJSON_IsObject(json)) {
        cJSON* new_obj = cJSON_CreateObject();
        if (UNLIKELY(!new_obj)) {
            regfree(&regex);
            return NULL;
        }

        const cJSON* child = json->child;
        while (child) {
            const char* key = child->string;
            char* new_key = NULL;

            if (key) {
                regmatch_t match;
                int match_result = regexec(&regex, key, 1, &match, 0);

                if (match_result == 0) {
                    new_key = my_strdup(replacement);
                } else {
                    new_key = my_strdup(key);
                }
            }

            if (new_key) {
                cJSON* processed_value = replace_keys_recursive(child, pattern, replacement);
                if (processed_value) {
                    cJSON_AddItemToObject(new_obj, new_key, processed_value);
                }
                free(new_key);
            }

            child = child->next;
        }

        regfree(&regex);
        return new_obj;
    } else if (cJSON_IsArray(json)) {
        cJSON* new_array = cJSON_CreateArray();
        if (UNLIKELY(!new_array)) {
            regfree(&regex);
            return NULL;
        }

        const cJSON* child = json->child;
        while (child) {
            cJSON* processed_value = replace_keys_recursive(child, pattern, replacement);
            if (processed_value) {
                cJSON_AddItemToArray(new_array, processed_value);
            }
            child = child->next;
        }

        regfree(&regex);
        return new_array;
    } else {
        regfree(&regex);
        return cJSON_Duplicate(json, 1);
    }
}

static cJSON* replace_values_recursive(const cJSON* json, const char* pattern, const char* replacement) {
    if (UNLIKELY(json == NULL)) return NULL;

    regex_t regex;
    int regex_result = regcomp(&regex, pattern, REG_EXTENDED);
    if (regex_result != 0) {
        return cJSON_Duplicate(json, 1);
    }

    if (cJSON_IsObject(json)) {
        cJSON* new_obj = cJSON_CreateObject();
        if (UNLIKELY(!new_obj)) {
            regfree(&regex);
            return NULL;
        }

        const cJSON* child = json->child;
        while (child) {
            const char* key = child->string;
            cJSON* processed_value = replace_values_recursive(child, pattern, replacement);
            if (processed_value && key) {
                cJSON_AddItemToObject(new_obj, key, processed_value);
            }
            child = child->next;
        }

        regfree(&regex);
        return new_obj;
    } else if (cJSON_IsArray(json)) {
        cJSON* new_array = cJSON_CreateArray();
        if (UNLIKELY(!new_array)) {
            regfree(&regex);
            return NULL;
        }

        const cJSON* child = json->child;
        while (child) {
            cJSON* processed_value = replace_values_recursive(child, pattern, replacement);
            if (processed_value) {
                cJSON_AddItemToArray(new_array, processed_value);
            }
            child = child->next;
        }

        regfree(&regex);
        return new_array;
    } else if (cJSON_IsString(json)) {
        const char* string_value = cJSON_GetStringValue(json);
        if (string_value) {
            regmatch_t match;
            int match_result = regexec(&regex, string_value, 1, &match, 0);

            if (match_result == 0) {
                regfree(&regex);
                return cJSON_CreateString(replacement);
            } else {
                regfree(&regex);
                return cJSON_CreateString(string_value);
            }
        } else {
            regfree(&regex);
            return cJSON_Duplicate(json, 1);
        }
    } else {
        regfree(&regex);
        return cJSON_Duplicate(json, 1);
    }
}
#endif

cJSON* replace_keys(const cJSON* json, const char* pattern, const char* replacement) {
    if (UNLIKELY(json == NULL || pattern == NULL || replacement == NULL)) return NULL;
    
#ifdef __WINDOWS__
    (void)pattern; (void)replacement;
    return cJSON_Duplicate(json, 1);
#else
    return replace_keys_recursive(json, pattern, replacement);
#endif
}

cJSON* replace_values(const cJSON* json, const char* pattern, const char* replacement) {
    if (UNLIKELY(json == NULL || pattern == NULL || replacement == NULL)) return NULL;
    
#ifdef __WINDOWS__
    (void)pattern; (void)replacement;
    return cJSON_Duplicate(json, 1);
#else
    return replace_values_recursive(json, pattern, replacement);
#endif
}

// =============================================================================
// JSON FLATTENER IMPLEMENTATION
// =============================================================================

typedef struct {
    char* key;
    cJSON* value;
} FlattenedPair;

typedef struct {
    FlattenedPair* pairs;
    int count;
    int capacity;
    char* memory_pool;
    size_t pool_used;
    size_t pool_size;
} FlattenedArray;

typedef struct {
    cJSON* object;
    cJSON* result;
    pthread_mutex_t* result_mutex;
    char* key_buffer;
    size_t buffer_size;
} ThreadData;

static char* pool_alloc(FlattenedArray* array, size_t size) {
    size = (size + 7) & ~7;

    if (array->pool_used + size <= array->pool_size) {
        char* ptr = array->memory_pool + array->pool_used;
        array->pool_used += size;
        return ptr;
    }

    if (size < 1024) {
        return malloc(size);
    } else {
        return malloc(size);
    }
}

static void init_flattened_array(FlattenedArray* array, int initial_capacity) {
    int capacity = initial_capacity < INITIAL_ARRAY_CAPACITY ? INITIAL_ARRAY_CAPACITY : initial_capacity;

    if (!g_cjson_node_pool) {
        init_global_pools();
    }

    array->pairs = (FlattenedPair*)malloc(capacity * sizeof(FlattenedPair));
    array->count = 0;
    array->capacity = capacity;

    size_t pool_size = MEMORY_POOL_SIZE + (initial_capacity * 64);
    array->memory_pool = (char*)malloc(pool_size);
    array->pool_used = 0;
    array->pool_size = pool_size;
}

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
        if (prefix_len == 0) {
            snprintf(buffer, buffer_size, "[%d]", index);
        } else {
            snprintf(buffer, buffer_size, "%.*s[%d]", (int)prefix_len, prefix, index);
        }
    } else {
        size_t suffix_len = strlen_simd(suffix);
        if (prefix_len == 0) {
            if (suffix_len + 1 < buffer_size) {
                memcpy(buffer, suffix, suffix_len + 1);
            }
        } else {
            if (prefix_len + suffix_len + 2 < buffer_size) {
                memcpy(buffer, prefix, prefix_len);
                buffer[prefix_len] = '.';
                memcpy(buffer + prefix_len + 1, suffix, suffix_len + 1);
            }
        }
    }
}

static char* pool_strdup(FlattenedArray* array, const char* str) {
    if (UNLIKELY(!str)) return NULL;

    size_t len = strlen_simd(str) + 1;
    char* new_str;

    if (LIKELY(len <= 128)) {
        new_str = pool_alloc(array, len);
        if (LIKELY(new_str != NULL)) {
            memcpy(new_str, str, len);
            return new_str;
        }
    }

    if (g_cjson_node_pool && len <= 256) {
        new_str = POOL_ALLOC(g_cjson_node_pool);
        if (new_str) {
            memcpy(new_str, str, len);
            return new_str;
        }
    }

    return my_strdup(str);
}

HOT_PATH void add_pair(FlattenedArray* array, const char* key, cJSON* value) {
    if (UNLIKELY(array->count >= array->capacity)) {
        int new_capacity = array->capacity << 1;
        FlattenedPair* new_pairs = (FlattenedPair*)realloc(array->pairs, new_capacity * sizeof(FlattenedPair));
        if (UNLIKELY(!new_pairs)) {
            return;
        }
        array->pairs = new_pairs;
        array->capacity = new_capacity;

        PREFETCH_WRITE(&array->pairs[array->count]);
    }

    FlattenedPair* pair = &array->pairs[array->count];
    pair->key = pool_strdup(array, key);
    pair->value = value;
    array->count++;

    if (LIKELY(array->count < array->capacity)) {
        PREFETCH_WRITE(&array->pairs[array->count]);
    }
}

static void free_flattened_array(FlattenedArray* array) {
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

static void flatten_json_recursive(cJSON* json, const char* prefix, FlattenedArray* result) {
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

static cJSON* create_flattened_json(FlattenedArray* flattened_array) {
    cJSON* result = cJSON_CreateObject();
    
    for (int i = 0; i < flattened_array->count; i++) {
        FlattenedPair pair = flattened_array->pairs[i];
        
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
                break;
        }
    }
    
    return result;
}

static cJSON* flatten_single_object(cJSON* json) {
    if (!json) return NULL;

    FlattenedArray flattened_array;
    init_flattened_array(&flattened_array, INITIAL_ARRAY_CAPACITY);

    flatten_json_recursive(json, "", &flattened_array);

    cJSON* flattened_json = create_flattened_json(&flattened_array);
    free_flattened_array(&flattened_array);

    return flattened_json;
}

static void flatten_object_task(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    cJSON* flattened = flatten_single_object(data->object);
    if (flattened) {
        data->result = flattened;
    }
}

cJSON* flatten_json_object(cJSON* json) {
    return flatten_single_object(json);
}

cJSON* flatten_json_batch(cJSON* json_array, int use_threads, int num_threads) {
    if (!json_array || json_array->type != cJSON_Array) {
        return NULL;
    }
    
    int array_size = cJSON_GetArraySize(json_array);
    if (array_size == 0) {
        return cJSON_CreateArray();
    }
    
    cJSON* result = cJSON_CreateArray();
    
    int should_use_threads = use_threads && 
                            array_size >= MIN_BATCH_SIZE_FOR_MT && 
                            get_optimal_threads(num_threads) > 1;
    
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
    
    ThreadPool* pool = thread_pool_create(num_threads);
    if (!pool) {
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(json_array, i);
            cJSON* flattened = flatten_single_object(item);
            if (flattened) {
                cJSON_AddItemToArray(result, flattened);
            }
        }
        return result;
    }
    
    ThreadData** thread_data_array = (ThreadData**)calloc(array_size, sizeof(ThreadData*));
    if (!thread_data_array) {
        thread_pool_destroy(pool);
        return result;
    }
    
    for (int i = 0; i < array_size; i++) {
        ThreadData* data = (ThreadData*)malloc(sizeof(ThreadData));
        if (!data) continue;
        
        data->object = cJSON_GetArrayItem(json_array, i);
        data->result = NULL;
        
        thread_data_array[i] = data;
        
        if (thread_pool_add_task(pool, flatten_object_task, data) != 0) {
            flatten_object_task(data);
        }
    }
    
    thread_pool_wait(pool);
    
    for (int i = 0; i < array_size; i++) {
        if (thread_data_array[i] && thread_data_array[i]->result) {
            cJSON_AddItemToArray(result, thread_data_array[i]->result);
        }
        
        if (thread_data_array[i]) {
            free(thread_data_array[i]);
        }
    }
    
    free(thread_data_array);
    thread_pool_destroy(pool);
    
    return result;
}

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
            flattened = flatten_json_batch(json, use_threads, num_threads);
        } else {
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

static const char* get_cjson_type_string(const cJSON* item) {
    if (!item) return "null";

    switch (item->type & 0xFF) {
        case cJSON_False:
        case cJSON_True:
            return "boolean";
        case cJSON_NULL:
            return "null";
        case cJSON_Number:
            if (item->valuedouble == (double)item->valueint) {
                return "integer";
            } else {
                return "number";
            }
        case cJSON_String:
            return "string";
        case cJSON_Array:
            return "array";
        case cJSON_Object:
            return "object";
        default:
            return "unknown";
    }
}

static void collect_paths_with_types_recursive(const cJSON* json, const char* prefix, cJSON* result) {
    if (!json || !result) return;

    char key_buffer[MAX_KEY_LENGTH];
    const cJSON* child = NULL;

    if (cJSON_IsArray(json)) {
        int index = 0;
        cJSON_ArrayForEach(child, json) {
            build_key_optimized(key_buffer, sizeof(key_buffer), prefix, NULL, 1, index);

            if (cJSON_IsObject(child) && child->child) {
                collect_paths_with_types_recursive(child, key_buffer, result);
            } else if (cJSON_IsArray(child)) {
                collect_paths_with_types_recursive(child, key_buffer, result);
            } else {
                const char* type_str = get_cjson_type_string(child);
                cJSON* type_value = cJSON_CreateString(type_str);
                if (type_value) {
                    cJSON_AddItemToObject(result, key_buffer, type_value);
                }
            }
            index++;
        }
    } else {
        cJSON_ArrayForEach(child, json) {
            const char* child_name = child->string;
            if (!child_name) continue;

            if (prefix && strlen(prefix) > 0) {
                build_key_optimized(key_buffer, sizeof(key_buffer), prefix, child_name, 0, 0);
            } else {
                safe_strncpy(key_buffer, child_name, sizeof(key_buffer));
            }

            if (cJSON_IsObject(child) && child->child) {
                collect_paths_with_types_recursive(child, key_buffer, result);
            } else if (cJSON_IsArray(child)) {
                collect_paths_with_types_recursive(child, key_buffer, result);
            } else {
                const char* type_str = get_cjson_type_string(child);
                cJSON* type_value = cJSON_CreateString(type_str);
                if (type_value) {
                    cJSON_AddItemToObject(result, key_buffer, type_value);
                }
            }
        }
    }
}

cJSON* get_flattened_paths_with_types(cJSON* json) {
    if (!json) return NULL;

    cJSON* result = cJSON_CreateObject();
    if (!result) return NULL;

    if (cJSON_IsObject(json)) {
        collect_paths_with_types_recursive(json, "", result);
    } else if (cJSON_IsArray(json)) {
        collect_paths_with_types_recursive(json, "", result);
    } else {
        const char* type_str = get_cjson_type_string(json);
        cJSON* type_value = cJSON_CreateString(type_str);
        if (type_value) {
            cJSON_AddItemToObject(result, "root", type_value);
        }
    }

    return result;
}

char* get_flattened_paths_with_types_string(const char* json_string) {
    if (!json_string) return NULL;

    cJSON* json = cJSON_Parse(json_string);
    if (!json) return NULL;

    cJSON* paths_with_types = get_flattened_paths_with_types(json);
    char* result = NULL;

    if (paths_with_types) {
        result = cJSON_Print(paths_with_types);
        cJSON_Delete(paths_with_types);
    }

    cJSON_Delete(json);
    return result;
}

// =============================================================================
// JSON SCHEMA GENERATOR IMPLEMENTATION
// =============================================================================

typedef enum {
    TYPE_NULL,
    TYPE_BOOLEAN,
    TYPE_INTEGER,
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_ARRAY,
    TYPE_OBJECT,
    TYPE_MIXED
} SchemaType;

typedef struct SchemaNode {
    SchemaType type;
    int required;
    int nullable;
    struct SchemaNode* items;
    struct PropertyNode* properties;
    char** required_props;
    int required_count;
    int required_capacity;
    cJSON* enum_values;
    int enum_count;
} SchemaNode;

typedef struct PropertyNode {
    char* name;
    size_t name_len;
    SchemaNode* schema;
    int required;
    struct PropertyNode* next;
} PropertyNode;

static SchemaNode* create_schema_node(SchemaType type) {
    SchemaNode* node = POOL_ALLOC(g_cjson_node_pool);
    if (UNLIKELY(!node)) return NULL;

    memset(node, 0, sizeof(SchemaNode));
    node->type = type;
    node->required = 1;

    return node;
}

HOT_PATH void add_property(SchemaNode* node, const char* name, SchemaNode* property_schema, int required) {
    PropertyNode* prop = POOL_ALLOC(g_property_node_pool);
    if (UNLIKELY(!prop)) return;

    prop->name = my_strdup(name);
    prop->name_len = strlen(name);
    prop->schema = property_schema;
    prop->required = required;
    prop->next = node->properties;
    node->properties = prop;

    if (required) {
        if (UNLIKELY(node->required_count >= node->required_capacity)) {
            int new_capacity = node->required_capacity == 0 ? 8 : node->required_capacity * 2;
            char** new_props = (char**)realloc(node->required_props, new_capacity * sizeof(char*));
            if (UNLIKELY(!new_props)) return;

            node->required_props = new_props;
            node->required_capacity = new_capacity;
        }
        node->required_props[node->required_count++] = my_strdup(name);
    }
}

static PropertyNode* find_property(SchemaNode* node, const char* name) {
    PropertyNode* prop = node->properties;
    while (prop) {
        if (strcmp(prop->name, name) == 0) {
            return prop;
        }
        prop = prop->next;
    }
    return NULL;
}

static void free_schema_node(SchemaNode* node) {
    if (!node) return;

    if (node->items) {
        free_schema_node(node->items);
    }

    PropertyNode* prop = node->properties;
    while (prop) {
        PropertyNode* next = prop->next;
        free(prop->name);
        free_schema_node(prop->schema);
        POOL_FREE(g_property_node_pool, prop);
        prop = next;
    }

    for (int i = 0; i < node->required_count; i++) {
        free(node->required_props[i]);
    }
    free(node->required_props);

    if (node->enum_values) {
        cJSON_Delete(node->enum_values);
    }

    POOL_FREE(g_cjson_node_pool, node);
}

static SchemaType get_schema_type(cJSON* json) {
    switch (json->type) {
        case cJSON_False:
        case cJSON_True:
            return TYPE_BOOLEAN;
        case cJSON_NULL:
            return TYPE_NULL;
        case cJSON_Number:
            if (json->valuedouble == (double)json->valueint &&
                json->valuedouble >= INT_MIN && json->valuedouble <= INT_MAX) {
                return TYPE_INTEGER;
            }
            return TYPE_NUMBER;
        case cJSON_String:
            return TYPE_STRING;
        case cJSON_Array:
            return TYPE_ARRAY;
        case cJSON_Object:
            return TYPE_OBJECT;
        default:
            return TYPE_NULL;
    }
}

static SchemaNode* analyze_json_value(cJSON* json);

static SchemaNode* merge_schema_nodes(SchemaNode* node1, SchemaNode* node2) {
    if (!node1) return node2;
    if (!node2) return node1;
    
    if (node1->type != node2->type) {
        SchemaNode* merged = create_schema_node(TYPE_MIXED);
        merged->required = node1->required && node2->required;
        merged->nullable = node1->nullable || node2->nullable || 
                          node1->type == TYPE_NULL || node2->type == TYPE_NULL;
        return merged;
    }
    
    SchemaNode* merged = create_schema_node(node1->type);
    merged->required = node1->required && node2->required;
    merged->nullable = node1->nullable || node2->nullable;
    
    switch (node1->type) {
        case TYPE_ARRAY:
            if (node1->items && node2->items) {
                merged->items = merge_schema_nodes(node1->items, node2->items);
            } else if (node1->items) {
                merged->items = create_schema_node(node1->items->type);
                merged->items->nullable = node1->items->nullable;
                merged->items->required = node1->items->required;
            } else if (node2->items) {
                merged->items = create_schema_node(node2->items->type);
                merged->items->nullable = node2->items->nullable;
                merged->items->required = node2->items->required;
            }
            break;
            
        case TYPE_OBJECT: {
            PropertyNode* prop1 = node1->properties;
            while (prop1) {
                PropertyNode* prop2 = find_property(node2, prop1->name);
                if (prop2) {
                    SchemaNode* merged_prop = merge_schema_nodes(prop1->schema, prop2->schema);
                    add_property(merged, prop1->name, merged_prop, prop1->required && prop2->required);
                } else {
                    SchemaNode* prop_copy = create_schema_node(prop1->schema->type);
                    prop_copy->nullable = 1;
                    add_property(merged, prop1->name, prop_copy, 0);
                }
                prop1 = prop1->next;
            }
            
            PropertyNode* prop2 = node2->properties;
            while (prop2) {
                if (!find_property(node1, prop2->name)) {
                    SchemaNode* prop_copy = create_schema_node(prop2->schema->type);
                    prop_copy->nullable = 1;
                    add_property(merged, prop2->name, prop_copy, 0);
                }
                prop2 = prop2->next;
            }
            break;
        }
            
        default:
            break;
    }
    
    return merged;
}

static SchemaNode* analyze_json_value(cJSON* json) {
    if (!json) return NULL;
    
    SchemaType type = get_schema_type(json);
    SchemaNode* node = create_schema_node(type);
    
    if (type == TYPE_NULL) {
        node->required = 0;
        node->nullable = 1;
    }
    
    switch (type) {
        case TYPE_ARRAY: {
            int array_size = cJSON_GetArraySize(json);
            if (__builtin_expect(array_size > 0, 1)) {
                cJSON* first_item = cJSON_GetArrayItem(json, 0);
                SchemaNode* items_schema = analyze_json_value(first_item);
                SchemaType first_type = items_schema->type;

                int check_limit = array_size > MAX_ARRAY_SAMPLE_SIZE ? MAX_ARRAY_SAMPLE_SIZE : array_size;
                int step = array_size > MAX_ARRAY_SAMPLE_SIZE ? array_size / MAX_ARRAY_SAMPLE_SIZE : 1;
                bool types_match = true;

                for (int i = step; i < array_size && types_match; i += step) {
                    if (i >= check_limit) break;

                    cJSON* item = cJSON_GetArrayItem(json, i);
                    SchemaType item_type = get_schema_type(item);

                    if (item_type != first_type) {
                        types_match = false;
                    }
                }

                if (!types_match) {
                    free_schema_node(items_schema);
                    items_schema = create_schema_node(TYPE_MIXED);
                }

                node->items = items_schema;
            } else {
                node->items = create_schema_node(TYPE_NULL);
            }
            break;
        }
        case TYPE_OBJECT: {
            cJSON* child = json->child;
            while (child) {
                SchemaNode* prop_schema = analyze_json_value(child);
                add_property(node, child->string, prop_schema, prop_schema->required);
                child = child->next;
            }
            break;
        }
        default:
            break;
    }
    
    return node;
}

static const char* schema_type_to_string(SchemaType type) {
    switch (type) {
        case TYPE_NULL: return "null";
        case TYPE_BOOLEAN: return "boolean";
        case TYPE_INTEGER: return "integer";
        case TYPE_NUMBER: return "number";
        case TYPE_STRING: return "string";
        case TYPE_ARRAY: return "array";
        case TYPE_OBJECT: return "object";
        case TYPE_MIXED: return "mixed";
        default: return "unknown";
    }
}

static cJSON* schema_node_to_json(SchemaNode* node);

static cJSON* schema_node_to_json(SchemaNode* node) {
    if (!node) return NULL;
    
    cJSON* schema = cJSON_CreateObject();
    
    cJSON_AddStringToObject(schema, "$schema", "http://json-schema.org/draft-07/schema#");
    
    if (node->type == TYPE_MIXED) {
        cJSON* type_array = cJSON_CreateArray();
        cJSON_AddItemToArray(type_array, cJSON_CreateString("string"));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("number"));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("integer"));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("boolean"));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("object"));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("array"));
        
        if (node->nullable) {
            cJSON_AddItemToArray(type_array, cJSON_CreateString("null"));
        }
        
        cJSON_AddItemToObject(schema, "type", type_array);
        return schema;
    }
    
    if (node->nullable) {
        cJSON* type_array = cJSON_CreateArray();
        cJSON_AddItemToArray(type_array, cJSON_CreateString(schema_type_to_string(node->type)));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("null"));
        cJSON_AddItemToObject(schema, "type", type_array);
    } else {
        cJSON_AddStringToObject(schema, "type", schema_type_to_string(node->type));
    }
    
    switch (node->type) {
        case TYPE_ARRAY:
            if (node->items) {
                cJSON* items_schema = cJSON_CreateObject();
                
                if (node->items->type == TYPE_MIXED) {
                    cJSON* type_array = cJSON_CreateArray();
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("string"));
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("number"));
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("integer"));
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("boolean"));
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("object"));
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("array"));
                    
                    if (node->items->nullable) {
                        cJSON_AddItemToArray(type_array, cJSON_CreateString("null"));
                    }
                    
                    cJSON_AddItemToObject(items_schema, "type", type_array);
                } else {
                    if (node->items->nullable) {
                        cJSON* type_array = cJSON_CreateArray();
                        cJSON_AddItemToArray(type_array, cJSON_CreateString(schema_type_to_string(node->items->type)));
                        cJSON_AddItemToArray(type_array, cJSON_CreateString("null"));
                        cJSON_AddItemToObject(items_schema, "type", type_array);
                    } else {
                        cJSON_AddStringToObject(items_schema, "type", schema_type_to_string(node->items->type));
                    }
                }
                
                if (node->items->type == TYPE_OBJECT && node->items->properties) {
                    cJSON* props = cJSON_CreateObject();
                    cJSON* required = cJSON_CreateArray();
                    
                    PropertyNode* prop = node->items->properties;
                    while (prop) {
                        cJSON* prop_schema = schema_node_to_json(prop->schema);
                        cJSON_DeleteItemFromObject(prop_schema, "$schema");
                        cJSON_AddItemToObject(props, prop->name, prop_schema);
                        
                        if (prop->required) {
                            cJSON_AddItemToArray(required, cJSON_CreateString(prop->name));
                        }
                        
                        prop = prop->next;
                    }
                    
                    cJSON_AddItemToObject(items_schema, "properties", props);
                    
                    if (cJSON_GetArraySize(required) > 0) {
                        cJSON_AddItemToObject(items_schema, "required", required);
                    } else {
                        cJSON_Delete(required);
                    }
                }
                
                cJSON_AddItemToObject(schema, "items", items_schema);
            }
            break;
            
        case TYPE_OBJECT:
            if (node->properties) {
                cJSON* props = cJSON_CreateObject();
                cJSON* required = cJSON_CreateArray();
                
                PropertyNode* prop = node->properties;
                while (prop) {
                    cJSON* prop_schema = schema_node_to_json(prop->schema);
                    cJSON_DeleteItemFromObject(prop_schema, "$schema");
                    cJSON_AddItemToObject(props, prop->name, prop_schema);
                    
                    if (prop->required) {
                        cJSON_AddItemToArray(required, cJSON_CreateString(prop->name));
                    }
                    
                    prop = prop->next;
                }
                
                cJSON_AddItemToObject(schema, "properties", props);
                
                if (cJSON_GetArraySize(required) > 0) {
                    cJSON_AddItemToObject(schema, "required", required);
                } else {
                    cJSON_Delete(required);
                }
            }
            break;
            
        default:
            break;
    }
    
    return schema;
}

static void generate_schema_task(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    data->result = (cJSON*)analyze_json_value(data->object);
}

cJSON* generate_schema_from_object(cJSON* json) {
    if (!json) return NULL;
    
    SchemaNode* schema_node = analyze_json_value(json);
    cJSON* schema = schema_node_to_json(schema_node);
    free_schema_node(schema_node);
    
    return schema;
}

cJSON* generate_schema_from_batch(cJSON* json_array, int use_threads, int num_threads) {
    if (!json_array || json_array->type != cJSON_Array) {
        return NULL;
    }

    int array_size = cJSON_GetArraySize(json_array);
    if (array_size == 0) {
        return cJSON_CreateObject();
    }

    SchemaNode** schemas = (SchemaNode**)malloc(array_size * sizeof(SchemaNode*));
    if (!schemas) return NULL;

    if (use_threads && array_size >= MIN_BATCH_SIZE_FOR_MT) {
        ThreadPool* pool = thread_pool_create(num_threads);
        if (pool) {
            ThreadData* thread_data_array = malloc(array_size * sizeof(ThreadData));
            for (int i = 0; i < array_size; i++) {
                thread_data_array[i].object = cJSON_GetArrayItem(json_array, i);
                thread_data_array[i].result = NULL;
                thread_pool_add_task(pool, generate_schema_task, &thread_data_array[i]);
            }
            thread_pool_destroy(pool);

            for (int i = 0; i < array_size; i++) {
                schemas[i] = (SchemaNode*)thread_data_array[i].result;
            }
            free(thread_data_array);
        } else {
            for (int i = 0; i < array_size; i++) {
                schemas[i] = analyze_json_value(cJSON_GetArrayItem(json_array, i));
            }
        }
    } else {
        for (int i = 0; i < array_size; i++) {
            schemas[i] = analyze_json_value(cJSON_GetArrayItem(json_array, i));
        }
    }

    SchemaNode* merged_schema = schemas[0];
    for (int i = 1; i < array_size; i++) {
        merged_schema = merge_schema_nodes(merged_schema, schemas[i]);
        free_schema_node(schemas[i]);
    }

    cJSON* result = schema_node_to_json(merged_schema);
    free_schema_node(merged_schema);
    free(schemas);

    return result;
}

char* generate_schema_from_string(const char* json_string, int use_threads, int num_threads) {
    if (!json_string) return NULL;
    
    cJSON* json = cJSON_Parse(json_string);
    if (!json) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            fprintf(stderr, "Error parsing JSON: %s\n", error_ptr);
        }
        return NULL;
    }
    
    cJSON* schema = NULL;
    
    if (json->type == cJSON_Array) {
        schema = generate_schema_from_batch(json, use_threads, num_threads);
    } else {
        schema = generate_schema_from_object(json);
    }
    
    char* result = NULL;
    if (schema) {
        result = cJSON_Print(schema);
        cJSON_Delete(schema);
    }
    
    cJSON_Delete(json);
    return result;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// =============================================================================
// COMMAND LINE INTERFACE
// =============================================================================

#ifndef CJSON_TOOLS_NO_MAIN

// Print usage information
void print_usage(const char* program_name) {
    printf("JSON Tools - A unified JSON processing utility\n\n");
    printf("Usage: %s [options] [input_file]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help                 Show this help message\n");
    printf("  -f, --flatten              Flatten nested JSON (default action)\n");
    printf("  -s, --schema               Generate JSON schema\n");
    printf("  -e, --remove-empty         Remove keys with empty string values\n");
    printf("  -n, --remove-nulls         Remove keys with null values\n");
    printf("  -r, --replace-keys <pattern> <replacement>\n");
    printf("                             Replace keys matching regex pattern\n");
    printf("  -v, --replace-values <pattern> <replacement>\n");
    printf("                             Replace string values matching regex pattern\n");
    printf("  -t, --threads [num]        Use multi-threading with specified number of threads\n");
    printf("                             (default: auto-detect optimal thread count)\n");
    printf("  -p, --pretty               Pretty-print output (default: compact)\n");
    printf("  -o, --output <file>        Write output to file instead of stdout\n\n");
    printf("If no input file is specified, input is read from stdin.\n");
    printf("Use '-' as input_file to explicitly read from stdin.\n\n");
    printf("Examples:\n");
    printf("  %s input.json                     # Flatten JSON from file\n", program_name);
    printf("  cat input.json | %s -             # Flatten JSON from stdin\n", program_name);
    printf("  %s -s input.json                  # Generate schema from file\n", program_name);
    printf("  %s -e input.json                  # Remove empty string values\n", program_name);
    printf("  %s -n input.json                  # Remove null values\n", program_name);
    printf("  %s -r '^session\\..*' 'session.page' input.json  # Replace keys with regex\n", program_name);
    printf("  %s -v '^old_.*' 'new_value' input.json       # Replace values with regex\n", program_name);
    printf("  %s -f -t 4 large_batch.json       # Flatten with 4 threads\n", program_name);
    printf("  %s -s -t 2 -o schema.json *.json  # Generate schema from multiple files\n", program_name);
}

int main(int argc, char* argv[]) {
    // Initialize global memory pools for better performance
    init_global_pools();

    // Default options
    int action_flatten = 1;
    int action_schema = 0;
    int action_remove_empty = 0;
    int action_remove_nulls = 0;
    int action_replace_keys = 0;
    char* replace_pattern = NULL;
    char* replace_replacement = NULL;
    int action_replace_values = 0;
    char* replace_values_pattern = NULL;
    char* replace_values_replacement = NULL;
    int use_threads = 0;
    int num_threads = 0;
    int pretty_print = 0;

    char* output_file = NULL;
    char* input_file = NULL;

    // Optimized command line argument parsing
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        // Fast path for common single-character options
        if (arg[0] == '-' && arg[1] != '-' && arg[2] == '\0') {
            switch (arg[1]) {
                case 'h':
                    print_usage(argv[0]);
                    cleanup_global_pools();
                    return 0;
                case 'f':
                    action_flatten = 1;
                    action_schema = 0;
                    action_remove_empty = 0;
                    action_remove_nulls = 0;
                    action_replace_keys = 0;
                    action_replace_values = 0;
                    continue;
                case 's':
                    action_flatten = 0;
                    action_schema = 1;
                    action_remove_empty = 0;
                    action_remove_nulls = 0;
                    action_replace_keys = 0;
                    action_replace_values = 0;
                    break;
                case 'e':
                    action_flatten = 0;
                    action_schema = 0;
                    action_remove_empty = 1;
                    action_remove_nulls = 0;
                    action_replace_keys = 0;
                    action_replace_values = 0;
                    break;
                case 'n':
                    action_flatten = 0;
                    action_schema = 0;
                    action_remove_empty = 0;
                    action_remove_nulls = 1;
                    action_replace_keys = 0;
                    action_replace_values = 0;
                    break;
                case 'r':
                    if (i + 2 >= argc) {
                        fprintf(stderr, "Error: -r requires pattern and replacement arguments\n");
                        cleanup_global_pools();
                        return 1;
                    }
                    action_flatten = 0;
                    action_schema = 0;
                    action_remove_empty = 0;
                    action_remove_nulls = 0;
                    action_replace_keys = 1;
                    action_replace_values = 0;
                    replace_pattern = argv[++i];
                    replace_replacement = argv[++i];
                    break;
                case 'v':
                    if (i + 2 >= argc) {
                        fprintf(stderr, "Error: -v requires pattern and replacement arguments\n");
                        cleanup_global_pools();
                        return 1;
                    }
                    action_flatten = 0;
                    action_schema = 0;
                    action_remove_empty = 0;
                    action_remove_nulls = 0;
                    action_replace_keys = 0;
                    action_replace_values = 1;
                    replace_values_pattern = argv[++i];
                    replace_values_replacement = argv[++i];
                    break;
                case 't':
                    use_threads = 1;
                    if (i + 1 < argc && argv[i + 1][0] != '-') {
                        num_threads = atoi(argv[++i]);
                    }
                    break;
                case 'p':
                    pretty_print = 1;
                    break;
                case 'o':
                    if (i + 1 >= argc) {
                        fprintf(stderr, "Error: -o requires output file argument\n");
                        cleanup_global_pools();
                        return 1;
                    }
                    output_file = argv[++i];
                    break;
                default:
                    fprintf(stderr, "Error: Unknown option '-%c'\n", arg[1]);
                    cleanup_global_pools();
                    return 1;
            }
            continue;
        }

        // Long options
        if (strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            cleanup_global_pools();
            return 0;
        } else if (strcmp(arg, "--flatten") == 0) {
            action_flatten = 1;
            action_schema = 0;
            action_remove_empty = 0;
            action_remove_nulls = 0;
            action_replace_keys = 0;
            action_replace_values = 0;
        } else if (strcmp(arg, "--schema") == 0) {
            action_flatten = 0;
            action_schema = 1;
            action_remove_empty = 0;
            action_remove_nulls = 0;
            action_replace_keys = 0;
            action_replace_values = 0;
        } else if (strcmp(arg, "--remove-empty") == 0) {
            action_flatten = 0;
            action_schema = 0;
            action_remove_empty = 1;
            action_remove_nulls = 0;
            action_replace_keys = 0;
            action_replace_values = 0;
        } else if (strcmp(arg, "--remove-nulls") == 0) {
            action_flatten = 0;
            action_schema = 0;
            action_remove_empty = 0;
            action_remove_nulls = 1;
            action_replace_keys = 0;
            action_replace_values = 0;
        } else if (strcmp(arg, "--replace-keys") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "Error: --replace-keys requires pattern and replacement arguments\n");
                cleanup_global_pools();
                return 1;
            }
            action_flatten = 0;
            action_schema = 0;
            action_remove_empty = 0;
            action_remove_nulls = 0;
            action_replace_keys = 1;
            action_replace_values = 0;
            replace_pattern = argv[++i];
            replace_replacement = argv[++i];
        } else if (strcmp(arg, "--replace-values") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "Error: --replace-values requires pattern and replacement arguments\n");
                cleanup_global_pools();
                return 1;
            }
            action_flatten = 0;
            action_schema = 0;
            action_remove_empty = 0;
            action_remove_nulls = 0;
            action_replace_keys = 0;
            action_replace_values = 1;
            replace_values_pattern = argv[++i];
            replace_values_replacement = argv[++i];
        } else if (strcmp(arg, "--threads") == 0) {
            use_threads = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                num_threads = atoi(argv[++i]);
            }
        } else if (strcmp(arg, "--pretty") == 0) {
            pretty_print = 1;
        } else if (strcmp(arg, "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --output requires output file argument\n");
                cleanup_global_pools();
                return 1;
            }
            output_file = argv[++i];
        } else if (input_file == NULL) {
            input_file = argv[i];
        } else {
            fprintf(stderr, "Error: Multiple input files not supported\n");
            cleanup_global_pools();
            return 1;
        }
    }

    // Read JSON input
    char* json_string = NULL;

    if (input_file == NULL || strcmp(input_file, "-") == 0) {
        // Read from stdin
        json_string = read_json_stdin();
    } else {
        // Read from file
        json_string = read_json_file(input_file);
    }

    if (!json_string) {
        fprintf(stderr, "Error: Failed to read JSON input\n");
        cleanup_global_pools();
        return 1;
    }

    // Process JSON based on selected action
    char* result = NULL;

    if (action_flatten) {
        result = flatten_json_string(json_string, use_threads, num_threads);
    } else if (action_schema) {
        result = generate_schema_from_string(json_string, use_threads, num_threads);
    } else if (action_remove_empty || action_remove_nulls || action_replace_keys || action_replace_values) {
        // Parse the JSON first
        cJSON* json = cJSON_Parse(json_string);
        if (!json) {
            fprintf(stderr, "Error: Invalid JSON input\n");
            free(json_string);
            cleanup_global_pools();
            return 1;
        }

        cJSON* processed = json;

        if (action_remove_empty) {
            processed = remove_empty_strings(json);
            cJSON_Delete(json);
        } else if (action_remove_nulls) {
            processed = remove_nulls(json);
            cJSON_Delete(json);
        } else if (action_replace_keys) {
            processed = replace_keys(json, replace_pattern, replace_replacement);
            cJSON_Delete(json);
        } else if (action_replace_values) {
            processed = replace_values(json, replace_values_pattern, replace_values_replacement);
            cJSON_Delete(json);
        }

        if (processed) {
            if (pretty_print) {
                result = cJSON_Print(processed);
            } else {
                result = cJSON_PrintUnformatted(processed);
            }
            cJSON_Delete(processed);
        }
    }

    free(json_string);

    if (!result) {
        fprintf(stderr, "Error: Failed to process JSON\n");
        cleanup_global_pools();
        return 1;
    }

    // Output result
    if (output_file) {
        FILE* output = fopen(output_file, "w");
        if (!output) {
            fprintf(stderr, "Error: Could not open output file %s\n", output_file);
            free(result);
            cleanup_global_pools();
            return 1;
        }
        fprintf(output, "%s\n", result);
        fclose(output);
    } else {
        printf("%s\n", result);
    }

    free(result);
    cleanup_global_pools();
    return 0;
}

#endif // CJSON_TOOLS_NO_MAIN