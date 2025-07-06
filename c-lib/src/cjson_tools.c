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
        #include <malloc.h>
        #include <intrin.h>
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

// Advanced SIMD includes with runtime detection
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define CPU_X86_FAMILY 1
    #ifdef __SSE2__
        #include <emmintrin.h>
        #define HAS_SSE2_INTRINSICS 1
    #endif
    #ifdef __SSE4_1__
        #include <smmintrin.h>
        #define HAS_SSE41_INTRINSICS 1
    #endif
    #ifdef __AVX2__
        #include <immintrin.h>
        #define HAS_AVX2_INTRINSICS 1
    #endif
    #ifdef __AVX512F__
        #include <immintrin.h>
        #define HAS_AVX512_INTRINSICS 1
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define CPU_ARM64_FAMILY 1
    #ifdef __ARM_NEON
        #include <arm_neon.h>
        #define HAS_NEON_INTRINSICS 1
    #endif
#endif

// Apple-specific includes
#ifdef __APPLE__
    #include <sys/sysctl.h>
    #include <libkern/OSCacheControl.h>
#endif

// MSVC-safe operations
#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
    #pragma warning(push)
    #pragma warning(disable: 4996 4005 4013 4244 4267)
#endif

// =============================================================================
// ADVANCED CPU FEATURE DETECTION
// =============================================================================

typedef struct {
    int has_sse2;
    int has_sse41;
    int has_avx2;
    int has_avx512;
    int has_neon;
    int has_popcnt;
    int has_bmi2;
    int cache_line_size;
    int num_cores;
    int l1_cache_size;
    int l2_cache_size;
    int l3_cache_size;
} cpu_features_t;

static cpu_features_t g_cpu = {0};
static volatile int g_cpu_detected = 0;

#ifdef CPU_X86_FAMILY
static void detect_x86_features(void) {
    #ifdef _MSC_VER
    int cpuid_info[4];
    __cpuid(cpuid_info, 1);
    
    g_cpu.has_sse2 = (cpuid_info[3] & (1 << 26)) != 0;
    g_cpu.has_sse41 = (cpuid_info[2] & (1 << 19)) != 0;
    g_cpu.has_popcnt = (cpuid_info[2] & (1 << 23)) != 0;
    
    __cpuid(cpuid_info, 7);
    g_cpu.has_avx2 = (cpuid_info[1] & (1 << 5)) != 0;
    g_cpu.has_bmi2 = (cpuid_info[1] & (1 << 8)) != 0;
    g_cpu.has_avx512 = (cpuid_info[1] & (1 << 16)) != 0;
    
    #elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax, ebx, ecx, edx;
    
    // Check CPUID availability
    if (__builtin_cpu_supports("sse2")) g_cpu.has_sse2 = 1;
    if (__builtin_cpu_supports("sse4.1")) g_cpu.has_sse41 = 1;
    if (__builtin_cpu_supports("avx2")) g_cpu.has_avx2 = 1;
    if (__builtin_cpu_supports("popcnt")) g_cpu.has_popcnt = 1;
    if (__builtin_cpu_supports("bmi2")) g_cpu.has_bmi2 = 1;
    
    // AVX-512 detection (more complex)
    __asm__ volatile ("cpuid"
                     : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                     : "a" (7), "c" (0));
    g_cpu.has_avx512 = (ebx & (1 << 16)) != 0;
    #endif
}
#endif

static void detect_cache_info(void) {
    #ifdef __linux__
    // Try to read from /sys/devices/system/cpu/cpu0/cache/
    FILE* f = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
    if (f) {
        fscanf(f, "%d", &g_cpu.cache_line_size);
        fclose(f);
    }
    
    f = fopen("/sys/devices/system/cpu/cpu0/cache/index0/size", "r");
    if (f) {
        char size_str[32];
        if (fgets(size_str, sizeof(size_str), f)) {
            g_cpu.l1_cache_size = atoi(size_str) * 1024; // Convert KB to bytes
        }
        fclose(f);
    }
    #elif defined(__APPLE__)
    size_t size = sizeof(int);
    sysctlbyname("hw.cachelinesize", &g_cpu.cache_line_size, &size, NULL, 0);
    sysctlbyname("hw.l1dcachesize", &g_cpu.l1_cache_size, &size, NULL, 0);
    sysctlbyname("hw.l2cachesize", &g_cpu.l2_cache_size, &size, NULL, 0);
    sysctlbyname("hw.l3cachesize", &g_cpu.l3_cache_size, &size, NULL, 0);
    #elif defined(_WIN32)
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = NULL;
    DWORD buffer_size = 0;
    
    GetLogicalProcessorInformation(NULL, &buffer_size);
    buffer = malloc(buffer_size);
    
    if (buffer && GetLogicalProcessorInformation(buffer, &buffer_size)) {
        DWORD count = buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        for (DWORD i = 0; i < count; i++) {
            if (buffer[i].Relationship == RelationCache) {
                if (buffer[i].Cache.Level == 1) {
                    g_cpu.l1_cache_size = buffer[i].Cache.Size;
                    g_cpu.cache_line_size = buffer[i].Cache.LineSize;
                } else if (buffer[i].Cache.Level == 2) {
                    g_cpu.l2_cache_size = buffer[i].Cache.Size;
                } else if (buffer[i].Cache.Level == 3) {
                    g_cpu.l3_cache_size = buffer[i].Cache.Size;
                }
            }
        }
    }
    free(buffer);
    #endif
    
    // Fallback defaults
    if (g_cpu.cache_line_size == 0) g_cpu.cache_line_size = 64;
    if (g_cpu.l1_cache_size == 0) g_cpu.l1_cache_size = 32 * 1024;
    if (g_cpu.l2_cache_size == 0) g_cpu.l2_cache_size = 256 * 1024;
    if (g_cpu.l3_cache_size == 0) g_cpu.l3_cache_size = 8 * 1024 * 1024;
}

static void detect_cpu_features(void) {
    if (__atomic_load_n(&g_cpu_detected, __ATOMIC_ACQUIRE)) return;
    
    memset(&g_cpu, 0, sizeof(g_cpu));
    
    #ifdef CPU_X86_FAMILY
    detect_x86_features();
    #elif defined(CPU_ARM64_FAMILY)
    g_cpu.has_neon = 1; // Almost universal on ARM64
    #endif
    
    detect_cache_info();
    g_cpu.num_cores = get_num_cores();
    
    __atomic_store_n(&g_cpu_detected, 1, __ATOMIC_RELEASE);
}

// =============================================================================
// OPTIMIZED MEMORY OPERATIONS
// =============================================================================

// Cache-optimized memory copy
static ALWAYS_INLINE void fast_memcpy(void* restrict dst, const void* restrict src, size_t n) {
    if (UNLIKELY(n == 0)) return;
    
    #ifdef HAS_AVX512_INTRINSICS
    if (g_cpu.has_avx512 && n >= 64 && ((uintptr_t)dst & 63) == 0 && ((uintptr_t)src & 63) == 0) {
        const __m512i* s = (const __m512i*)src;
        __m512i* d = (__m512i*)dst;
        size_t avx512_count = n / 64;
        
        for (size_t i = 0; i < avx512_count; i++) {
            _mm512_store_si512(d + i, _mm512_load_si512(s + i));
        }
        
        size_t remaining = n % 64;
        if (remaining > 0) {
            memcpy((char*)dst + avx512_count * 64, (const char*)src + avx512_count * 64, remaining);
        }
        return;
    }
    #endif
    
    #ifdef HAS_AVX2_INTRINSICS
    if (g_cpu.has_avx2 && n >= 32 && ((uintptr_t)dst & 31) == 0 && ((uintptr_t)src & 31) == 0) {
        const __m256i* s = (const __m256i*)src;
        __m256i* d = (__m256i*)dst;
        size_t avx2_count = n / 32;
        
        for (size_t i = 0; i < avx2_count; i++) {
            _mm256_store_si256(d + i, _mm256_load_si256(s + i));
        }
        
        size_t remaining = n % 32;
        if (remaining > 0) {
            memcpy((char*)dst + avx2_count * 32, (const char*)src + avx2_count * 32, remaining);
        }
        return;
    }
    #endif
    
    #ifdef HAS_NEON_INTRINSICS
    if (g_cpu.has_neon && n >= 16 && ((uintptr_t)dst & 15) == 0 && ((uintptr_t)src & 15) == 0) {
        const uint8x16_t* s = (const uint8x16_t*)src;
        uint8x16_t* d = (uint8x16_t*)dst;
        size_t neon_count = n / 16;
        
        for (size_t i = 0; i < neon_count; i++) {
            vst1q_u8((uint8_t*)(d + i), vld1q_u8((const uint8_t*)(s + i)));
        }
        
        size_t remaining = n % 16;
        if (remaining > 0) {
            memcpy((char*)dst + neon_count * 16, (const char*)src + neon_count * 16, remaining);
        }
        return;
    }
    #endif
    
    // Fallback to standard memcpy
    memcpy(dst, src, n);
}

// Optimized memory comparison
static ALWAYS_INLINE int fast_memcmp(const void* a, const void* b, size_t n) {
    if (UNLIKELY(n == 0)) return 0;
    if (UNLIKELY(a == b)) return 0;
    
    #ifdef HAS_AVX2_INTRINSICS
    if (g_cpu.has_avx2 && n >= 32) {
        const __m256i* pa = (const __m256i*)a;
        const __m256i* pb = (const __m256i*)b;
        size_t avx2_count = n / 32;
        
        for (size_t i = 0; i < avx2_count; i++) {
            __m256i va = _mm256_loadu_si256(pa + i);
            __m256i vb = _mm256_loadu_si256(pb + i);
            __m256i cmp = _mm256_cmpeq_epi8(va, vb);
            uint32_t mask = _mm256_movemask_epi8(cmp);
            
            if (mask != 0xFFFFFFFF) {
                // Found difference, fall back to byte comparison
                const char* ca = (const char*)a + i * 32;
                const char* cb = (const char*)b + i * 32;
                for (size_t j = 0; j < 32; j++) {
                    if (ca[j] != cb[j]) {
                        return (unsigned char)ca[j] - (unsigned char)cb[j];
                    }
                }
            }
        }
        
        // Compare remaining bytes
        size_t remaining = n % 32;
        if (remaining > 0) {
            return memcmp((const char*)a + avx2_count * 32, 
                         (const char*)b + avx2_count * 32, remaining);
        }
        return 0;
    }
    #endif
    
    return memcmp(a, b, n);
}

// =============================================================================
// ULTRA-FAST STRING OPERATIONS
// =============================================================================

// Vectorized strlen with multiple implementations
static size_t strlen_avx512(const char* str) {
    #ifdef HAS_AVX512_INTRINSICS
    if (!g_cpu.has_avx512) return strlen(str);
    
    const char* start = str;
    const __m512i zero = _mm512_setzero_si512();
    
    // Align to 64-byte boundary
    while ((uintptr_t)str & 63) {
        if (*str == 0) return str - start;
        str++;
    }
    
    // Process 64 bytes at a time
    while (1) {
        __m512i chunk = _mm512_load_si512((const __m512i*)str);
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, zero);
        
        if (mask != 0) {
            return str - start + __builtin_ctzll(mask);
        }
        str += 64;
        
        // Safety check
        if (str - start > 1000000) break;
    }
    #endif
    
    return strlen(str);
}

static size_t strlen_avx2(const char* str) {
    #ifdef HAS_AVX2_INTRINSICS
    if (!g_cpu.has_avx2) return strlen(str);
    
    const char* start = str;
    const __m256i zero = _mm256_setzero_si256();
    
    // Handle unaligned start
    while ((uintptr_t)str & 31) {
        if (*str == 0) return str - start;
        str++;
    }
    
    // Process 32 bytes at a time
    while (1) {
        __m256i chunk = _mm256_load_si256((const __m256i*)str);
        __m256i cmp = _mm256_cmpeq_epi8(chunk, zero);
        uint32_t mask = _mm256_movemask_epi8(cmp);
        
        if (mask != 0) {
            return str - start + __builtin_ctz(mask);
        }
        str += 32;
        
        // Safety check
        if (str - start > 1000000) break;
    }
    #endif
    
    return strlen(str);
}

static size_t strlen_neon(const char* str) {
    #ifdef HAS_NEON_INTRINSICS
    if (!g_cpu.has_neon) return strlen(str);
    
    const char* start = str;
    const uint8x16_t zero = vdupq_n_u8(0);
    
    // Handle unaligned start
    while ((uintptr_t)str & 15) {
        if (*str == 0) return str - start;
        str++;
    }
    
    // Process 16 bytes at a time
    while (1) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*)str);
        uint8x16_t cmp = vceqq_u8(chunk, zero);
        
        // Convert to mask
        uint64x2_t mask64 = vreinterpretq_u64_u8(cmp);
        uint64_t low = vgetq_lane_u64(mask64, 0);
        uint64_t high = vgetq_lane_u64(mask64, 1);
        
        if (low || high) {
            for (int i = 0; i < 16; i++) {
                if (str[i] == 0) return str - start + i;
            }
        }
        
        str += 16;
        
        // Safety check
        if (str - start > 1000000) break;
    }
    #endif
    
    return strlen(str);
}

size_t strlen_simd(const char* str) {
    if (UNLIKELY(!str)) return 0;
    
    detect_cpu_features();
    
    #ifdef HAS_AVX512_INTRINSICS
    if (g_cpu.has_avx512) return strlen_avx512(str);
    #endif
    
    #ifdef HAS_AVX2_INTRINSICS
    if (g_cpu.has_avx2) return strlen_avx2(str);
    #endif
    
    #ifdef HAS_NEON_INTRINSICS
    if (g_cpu.has_neon) return strlen_neon(str);
    #endif
    
    return strlen(str);
}

// Optimized string search
const char* fast_strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle || !*needle) return haystack;
    
    size_t needle_len = strlen_simd(needle);
    if (needle_len == 1) {
        return strchr(haystack, needle[0]);
    }
    
    #ifdef HAS_AVX2_INTRINSICS
    if (g_cpu.has_avx2 && needle_len <= 32) {
        const __m256i first_char = _mm256_set1_epi8(needle[0]);
        const char* h = haystack;
        
        while (*h) {
            // Find potential matches
            __m256i chunk = _mm256_loadu_si256((const __m256i*)h);
            __m256i cmp = _mm256_cmpeq_epi8(chunk, first_char);
            uint32_t mask = _mm256_movemask_epi8(cmp);
            
            while (mask) {
                int pos = __builtin_ctz(mask);
                if (strncmp(h + pos, needle, needle_len) == 0) {
                    return h + pos;
                }
                mask &= mask - 1; // Clear lowest set bit
            }
            
            h += 32;
        }
        return NULL;
    }
    #endif
    
    return strstr(haystack, needle);
}

// =============================================================================
// ENHANCED MEMORY POOL SYSTEM
// =============================================================================

typedef struct MemoryBlock {
    struct MemoryBlock* next;
    size_t size;
    char data[];
} MemoryBlock;

typedef struct {
    MemoryBlock* free_blocks[16]; // Size classes: 64, 128, 256, ..., 1MB
    MemoryBlock* large_blocks;
    void* arena_start;
    void* arena_current;
    size_t arena_size;
    size_t arena_used;
    pthread_mutex_t mutex;
    _Alignas(CACHE_LINE_SIZE) volatile size_t alloc_count;
    _Alignas(CACHE_LINE_SIZE) volatile size_t free_count;
} AdvancedAllocator;

static AdvancedAllocator* g_allocator = NULL;

static size_t get_size_class(size_t size) {
    if (size <= 64) return 0;
    if (size <= 128) return 1;
    if (size <= 256) return 2;
    if (size <= 512) return 3;
    if (size <= 1024) return 4;
    if (size <= 2048) return 5;
    if (size <= 4096) return 6;
    if (size <= 8192) return 7;
    if (size <= 16384) return 8;
    if (size <= 32768) return 9;
    if (size <= 65536) return 10;
    if (size <= 131072) return 11;
    if (size <= 262144) return 12;
    if (size <= 524288) return 13;
    if (size <= 1048576) return 14;
    return 15; // Large allocation
}

static void* arena_alloc(AdvancedAllocator* alloc, size_t size) {
    size = (size + 15) & ~15; // 16-byte alignment
    
    if (alloc->arena_used + size > alloc->arena_size) {
        return NULL; // Arena exhausted
    }
    
    void* ptr = (char*)alloc->arena_current + alloc->arena_used;
    alloc->arena_used += size;
    
    return ptr;
}

SlabAllocator* slab_allocator_create(size_t object_size, size_t initial_objects) {
    (void)initial_objects; // Suppress unused parameter warning
    if (!g_allocator) {
        g_allocator = calloc(1, sizeof(AdvancedAllocator));
        if (!g_allocator) return NULL;
        
        // Allocate large arena (64MB)
        g_allocator->arena_size = 64 * 1024 * 1024;
        
        #ifdef __unix__
        g_allocator->arena_start = mmap(NULL, g_allocator->arena_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (g_allocator->arena_start == MAP_FAILED) {
            g_allocator->arena_start = malloc(g_allocator->arena_size);
        }
        #else
        g_allocator->arena_start = malloc(g_allocator->arena_size);
        #endif
        
        if (!g_allocator->arena_start) {
            free(g_allocator);
            g_allocator = NULL;
            return NULL;
        }
        
        g_allocator->arena_current = g_allocator->arena_start;
        pthread_mutex_init(&g_allocator->mutex, NULL);
    }
    
    // For compatibility, create a simple slab allocator
    SlabAllocator* allocator = malloc(sizeof(SlabAllocator));
    if (!allocator) return NULL;
    
    allocator->object_size = (object_size + 15) & ~15; // 16-byte align
    allocator->objects_per_slab = 1000; // Fixed size for simplicity
    allocator->total_slabs = 1;
    allocator->allocated_objects = 0;
    allocator->use_huge_pages = false;
    
    size_t total_size = allocator->object_size * allocator->objects_per_slab;
    allocator->memory = arena_alloc(g_allocator, total_size);
    
    if (!allocator->memory) {
        // Fallback to malloc
        allocator->memory = malloc(total_size);
        if (!allocator->memory) {
            free(allocator);
            return NULL;
        }
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
    
    return allocator;
}

void* slab_alloc(SlabAllocator* allocator) {
    if (!allocator) {
        size_t size_class = get_size_class(256);
        pthread_mutex_lock(&g_allocator->mutex);
        
        MemoryBlock* block = g_allocator->free_blocks[size_class];
        if (block) {
            g_allocator->free_blocks[size_class] = block->next;
            pthread_mutex_unlock(&g_allocator->mutex);
            __atomic_fetch_add(&g_allocator->alloc_count, 1, __ATOMIC_RELAXED);
            return block->data;
        }
        
        pthread_mutex_unlock(&g_allocator->mutex);
        return malloc(256);
    }
    
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
        if (ptr) free(ptr);
        return;
    }

    // Safety check for valid pointer
    if ((uintptr_t)ptr < 0x1000) {
        return; // Invalid pointer
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
    
    // Don't free arena memory - it's managed globally
    free(allocator);
}

// Global pools
SlabAllocator* g_cjson_node_pool = NULL;
SlabAllocator* g_property_node_pool = NULL;
SlabAllocator* g_task_pool = NULL;

void init_global_pools(void) {
    if (g_cjson_node_pool) return;
    
    detect_cpu_features();
    
    g_cjson_node_pool = slab_allocator_create(256, 2000);
    g_property_node_pool = slab_allocator_create(128, 1000);
    g_task_pool = slab_allocator_create(64, 500);
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
    
    if (g_allocator) {
        #ifdef __unix__
        if (g_allocator->arena_start != MAP_FAILED) {
            munmap(g_allocator->arena_start, g_allocator->arena_size);
        } else {
            free(g_allocator->arena_start);
        }
        #else
        free(g_allocator->arena_start);
        #endif
        
        pthread_mutex_destroy(&g_allocator->mutex);
        free(g_allocator);
        g_allocator = NULL;
    }
}

// =============================================================================
// STRING VIEW IMPLEMENTATION
// =============================================================================

int string_view_equals(const StringView* a, const StringView* b) {
    if (a->length != b->length) return 0;
    if (a->data == b->data) return 1;
    
    // Compare hashes first
    if (string_view_hash(a) != string_view_hash(b)) return 0;
    
    return fast_memcmp(a->data, b->data, a->length) == 0;
}

int string_view_equals_cstr(const StringView* sv, const char* str) {
    size_t str_len = strlen_simd(str);
    if (sv->length != str_len) return 0;
    return fast_memcmp(sv->data, str, str_len) == 0;
}

int string_view_starts_with(const StringView* sv, const char* prefix) {
    size_t prefix_len = strlen_simd(prefix);
    if (sv->length < prefix_len) return 0;
    return fast_memcmp(sv->data, prefix, prefix_len) == 0;
}

int string_view_ends_with(const StringView* sv, const char* suffix) {
    size_t suffix_len = strlen_simd(suffix);
    if (sv->length < suffix_len) return 0;
    return fast_memcmp(sv->data + sv->length - suffix_len, suffix, suffix_len) == 0;
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
        fast_memcpy(str, sv->data, sv->length);
        str[sv->length] = '\0';
    }
    return str;
}

// =============================================================================
// ADVANCED WHITESPACE SKIPPING
// =============================================================================

const char* skip_whitespace_optimized(const char* str, size_t len) {
    if (!str || len == 0) return str;
    
    detect_cpu_features();
    
    #ifdef HAS_AVX512_INTRINSICS
    if (g_cpu.has_avx512 && len >= 64) {
        const __m512i spaces = _mm512_set1_epi8(' ');
        const __m512i tabs = _mm512_set1_epi8('\t');
        const __m512i newlines = _mm512_set1_epi8('\n');
        const __m512i returns = _mm512_set1_epi8('\r');
        
        size_t i = 0;
        for (; i + 64 <= len; i += 64) {
            __m512i chunk = _mm512_loadu_si512((const __m512i*)(str + i));
            
            __mmask64 is_space = _mm512_cmpeq_epi8_mask(chunk, spaces);
            __mmask64 is_tab = _mm512_cmpeq_epi8_mask(chunk, tabs);
            __mmask64 is_newline = _mm512_cmpeq_epi8_mask(chunk, newlines);
            __mmask64 is_return = _mm512_cmpeq_epi8_mask(chunk, returns);
            
            __mmask64 is_whitespace = is_space | is_tab | is_newline | is_return;
            __mmask64 not_whitespace = ~is_whitespace;
            
            if (not_whitespace != 0) {
                return str + i + __builtin_ctzll(not_whitespace);
            }
        }
        
        // Handle remaining bytes
        for (; i < len; i++) {
            char c = str[i];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                return str + i;
            }
        }
        
        return str + len;
    }
    #endif
    
    #ifdef HAS_AVX2_INTRINSICS
    if (g_cpu.has_avx2 && len >= 32) {
        const __m256i spaces = _mm256_set1_epi8(' ');
        const __m256i tabs = _mm256_set1_epi8('\t');
        const __m256i newlines = _mm256_set1_epi8('\n');
        const __m256i returns = _mm256_set1_epi8('\r');
        
        size_t i = 0;
        for (; i + 32 <= len; i += 32) {
            __m256i chunk = _mm256_loadu_si256((const __m256i*)(str + i));
            
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
        
        // Handle remaining bytes
        for (; i < len; i++) {
            char c = str[i];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                return str + i;
            }
        }
        
        return str + len;
    }
    #endif
    
    #ifdef HAS_NEON_INTRINSICS
    if (g_cpu.has_neon && len >= 16) {
        const uint8x16_t spaces = vdupq_n_u8(' ');
        const uint8x16_t tabs = vdupq_n_u8('\t');
        const uint8x16_t newlines = vdupq_n_u8('\n');
        const uint8x16_t returns = vdupq_n_u8('\r');
        
        size_t i = 0;
        for (; i + 16 <= len; i += 16) {
            uint8x16_t chunk = vld1q_u8((const uint8_t*)(str + i));
            
            uint8x16_t is_space = vceqq_u8(chunk, spaces);
            uint8x16_t is_tab = vceqq_u8(chunk, tabs);
            uint8x16_t is_newline = vceqq_u8(chunk, newlines);
            uint8x16_t is_return = vceqq_u8(chunk, returns);
            
            uint8x16_t is_whitespace = vorrq_u8(
                vorrq_u8(is_space, is_tab),
                vorrq_u8(is_newline, is_return)
            );
            
            // Find first non-whitespace
            uint64x2_t mask64 = vreinterpretq_u64_u8(is_whitespace);
            uint64_t low = ~vgetq_lane_u64(mask64, 0);
            uint64_t high = ~vgetq_lane_u64(mask64, 1);
            
            if (low || high) {
                for (int j = 0; j < 16; j++) {
                    char c = str[i + j];
                    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                        return str + i + j;
                    }
                }
            }
        }
        
        // Handle remaining bytes
        for (; i < len; i++) {
            char c = str[i];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                return str + i;
            }
        }
        
        return str + len;
    }
    #endif
    
    // Scalar fallback
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (LIKELY(c != ' ' && c != '\t' && c != '\n' && c != '\r')) {
            return str + i;
        }
    }
    return str + len;
}

const char* find_delimiter_optimized(const char* str, size_t len) {
    if (!str || len == 0) return str;
    
    #ifdef HAS_AVX2_INTRINSICS
    if (g_cpu.has_avx2 && len >= 32) {
        const __m256i quote = _mm256_set1_epi8('"');
        const __m256i comma = _mm256_set1_epi8(',');
        const __m256i colon = _mm256_set1_epi8(':');
        const __m256i lbrace = _mm256_set1_epi8('{');
        const __m256i rbrace = _mm256_set1_epi8('}');
        const __m256i lbracket = _mm256_set1_epi8('[');
        const __m256i rbracket = _mm256_set1_epi8(']');
        
        for (size_t i = 0; i + 32 <= len; i += 32) {
            __m256i chunk = _mm256_loadu_si256((const __m256i*)(str + i));
            
            __m256i match = _mm256_or_si256(
                _mm256_or_si256(
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk, quote), _mm256_cmpeq_epi8(chunk, comma)),
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk, colon), _mm256_cmpeq_epi8(chunk, lbrace))
                ),
                _mm256_or_si256(
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk, rbrace), _mm256_cmpeq_epi8(chunk, lbracket)),
                    _mm256_cmpeq_epi8(chunk, rbracket)
                )
            );
            
            uint32_t mask = _mm256_movemask_epi8(match);
            if (mask != 0) {
                return str + i + __builtin_ctz(mask);
            }
        }
    }
    #endif
    
    // Scalar fallback
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (c == '"' || c == ',' || c == ':' || c == '{' || c == '}' || c == '[' || c == ']') {
            return str + i;
        }
    }
    return str + len;
}

// =============================================================================
// OPTIMIZED STRING DUPLICATION
// =============================================================================

char* my_strdup(const char* str) {
    if (UNLIKELY(str == NULL)) return NULL;

    size_t len = strlen_simd(str) + 1;

    // Always use regular malloc to avoid mixing allocation methods
    // This ensures we can always use regular free() to deallocate
    
    // Allocate with optimal alignment
    char* new_str;
    if (len >= 64) {
        #ifdef _MSC_VER
        new_str = _aligned_malloc(len, 32);
        #elif defined(__APPLE__)
        new_str = malloc(len); // macOS malloc is already well-aligned
        #else
        if (posix_memalign((void**)&new_str, 32, len) != 0) {
            new_str = NULL;
        }
        #endif
    } else {
        new_str = malloc(len);
    }
    
    if (UNLIKELY(new_str == NULL)) return NULL;
    
    fast_memcpy(new_str, str, len);
    return new_str;
}

// Corresponding free function for my_strdup
void my_strfree(char* str) {
    if (!str) return;

    // Safety check for valid pointer
    if ((uintptr_t)str < 0x1000) {
        return; // Invalid pointer
    }

    // Try to free from slab pool first (for small strings that might have been slab-allocated)
    if (g_cjson_node_pool) {
        char* pool_start = (char*)g_cjson_node_pool->memory;
        char* pool_end = pool_start + (g_cjson_node_pool->object_size * g_cjson_node_pool->objects_per_slab);

        if (str >= pool_start && str < pool_end) {
            slab_free(g_cjson_node_pool, str);
            return;
        }
    }

    // Otherwise use regular free
    free(str);
}

// =============================================================================
// SYSTEM UTILITIES
// =============================================================================

#ifdef __WINDOWS__
static long msvc_sysconf(int name) {
    (void)name;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (long)si.dwNumberOfProcessors;
}
#define sysconf(x) msvc_sysconf(x)
#define _SC_NPROCESSORS_ONLN 1
#endif

int get_num_cores(void) {
    #ifdef __APPLE__
    int apple_cores;
    size_t size = sizeof(apple_cores);
    if (sysctlbyname("hw.ncpu", &apple_cores, &size, NULL, 0) == 0) {
        return apple_cores;
    }
    #endif

    long unix_cores = sysconf(_SC_NPROCESSORS_ONLN);
    return (unix_cores > 0) ? (int)unix_cores : 1;
}

int get_optimal_threads(int requested_threads) {
    if (LIKELY(requested_threads > 0)) {
        return requested_threads > 128 ? 128 : requested_threads;
    }
    
    int num_cores = g_cpu.num_cores > 0 ? g_cpu.num_cores : get_num_cores();
    
    // More sophisticated thread calculation based on workload characteristics
    if (num_cores <= 2) {
        return num_cores;
    } else if (num_cores <= 4) {
        return num_cores;
    } else if (num_cores <= 8) {
        return num_cores - 1; // Leave one core for system
    } else if (num_cores <= 16) {
        return (num_cores * 3) / 4; // Use 75% of cores
    } else {
        return num_cores / 2 + 4; // Use half + some extra for high core count systems
    }
}

// =============================================================================
// FILE I/O OPTIMIZATIONS
// =============================================================================

char* read_json_file(const char* filename) {
    if (!filename) return NULL;
    
    #ifdef __unix__
    // Try memory mapping for large files
    int fd = open(filename, O_RDONLY);
    if (fd != -1) {
        struct stat st;
        if (fstat(fd, &st) == 0 && st.st_size > 8192) {
            void* mapped = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (mapped != MAP_FAILED) {
                char* buffer = malloc(st.st_size + 1);
                if (buffer) {
                    fast_memcpy(buffer, mapped, st.st_size);
                    buffer[st.st_size] = '\0';
                }
                munmap(mapped, st.st_size);
                close(fd);
                return buffer;
            }
        }
        close(fd);
    }
    #endif
    
    // Fallback to regular file I/O
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;
    
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    
    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }
    
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    
    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';
    fclose(file);
    
    return buffer;
}

char* read_json_stdin(void) {
    size_t capacity = 8192;
    size_t used = 0;
    char* content = malloc(capacity);
    
    if (!content) return NULL;
    
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        size_t buffer_len = strlen_simd(buffer);
        
        // Ensure capacity
        while (used + buffer_len + 1 > capacity) {
            capacity *= 2;
            char* new_content = realloc(content, capacity);
            if (!new_content) {
                free(content);
                return NULL;
            }
            content = new_content;
        }
        
        fast_memcpy(content + used, buffer, buffer_len);
        used += buffer_len;
    }
    
    content[used] = '\0';
    return content;
}

// =============================================================================
// WINDOWS PTHREAD COMPATIBILITY
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
// THREAD POOL WITH WORK-STEALING
// =============================================================================

// WorkStealingQueue and WorkStealingPool structures are now defined in the header

static WorkStealingQueue* create_queue(int capacity) {
    WorkStealingQueue* queue = malloc(sizeof(WorkStealingQueue));
    if (!queue) return NULL;
    
    // Ensure capacity is power of 2
    capacity = 1;
    while (capacity < 1024) capacity <<= 1;
    
    queue->tasks = malloc(capacity * sizeof(Task));
    if (!queue->tasks) {
        free(queue);
        return NULL;
    }
    
    queue->capacity = capacity;
    queue->mask = capacity - 1;
    queue->head = 0;
    queue->tail = 0;
    
    return queue;
}

static int queue_push(WorkStealingQueue* queue, Task* task) {
    int tail = __atomic_load_n(&queue->tail, __ATOMIC_RELAXED);
    int next_tail = (tail + 1) & queue->mask;
    
    if (next_tail == __atomic_load_n(&queue->head, __ATOMIC_ACQUIRE)) {
        return 0; // Queue full
    }
    
    queue->tasks[tail] = *task;
    __atomic_store_n(&queue->tail, next_tail, __ATOMIC_RELEASE);
    
    return 1;
}

static int queue_pop(WorkStealingQueue* queue, Task* task) {
    int tail = __atomic_load_n(&queue->tail, __ATOMIC_RELAXED);
    int head = __atomic_load_n(&queue->head, __ATOMIC_RELAXED);
    
    if (head == tail) {
        return 0; // Queue empty
    }
    
    tail = (tail - 1) & queue->mask;
    *task = queue->tasks[tail];
    
    if (!__atomic_compare_exchange_n(&queue->tail, &tail, tail, false, 
                                   __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
        return 0; // Failed to pop
    }
    
    return 1;
}

static int queue_steal(WorkStealingQueue* queue, Task* task) {
    int head = __atomic_load_n(&queue->head, __ATOMIC_ACQUIRE);
    int tail = __atomic_load_n(&queue->tail, __ATOMIC_ACQUIRE);
    
    if (head == tail) {
        return 0; // Queue empty
    }
    
    *task = queue->tasks[head];
    
    if (!__atomic_compare_exchange_n(&queue->head, &head, (head + 1) & queue->mask,
                                   false, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
        return 0; // Failed to steal
    }
    
    return 1;
}

// Global task queue size tracking (improved)
static _Alignas(CACHE_LINE_SIZE) volatile size_t g_global_task_count = 0;

size_t get_task_queue_size(void) {
    return __atomic_load_n(&g_global_task_count, __ATOMIC_RELAXED);
}

static void* worker_thread_stealing(void* arg) {
    struct {
        WorkStealingPool* pool;
        int thread_id;
    }* data = arg;
    
    WorkStealingPool* pool = data->pool;
    int thread_id = data->thread_id;
    WorkStealingQueue* my_queue = &pool->queues[thread_id];
    
    Task task;
    int idle_count = 0;
    
    while (!__atomic_load_n(&pool->shutdown, __ATOMIC_ACQUIRE)) {
        // Try to pop from own queue first
        if (queue_pop(my_queue, &task)) {
            task.function(task.argument);
            __atomic_fetch_sub(&pool->global_task_count, 1, __ATOMIC_RELAXED);
            __atomic_fetch_sub(&g_global_task_count, 1, __ATOMIC_RELAXED);
            idle_count = 0;
            continue;
        }
        
        // Try to steal from other queues
        int stolen = 0;
        for (int i = 1; i < pool->num_threads; i++) {
            int victim = (thread_id + i) % pool->num_threads;
            if (queue_steal(&pool->queues[victim], &task)) {
                task.function(task.argument);
                __atomic_fetch_sub(&pool->global_task_count, 1, __ATOMIC_RELAXED);
                __atomic_fetch_sub(&g_global_task_count, 1, __ATOMIC_RELAXED);
                stolen = 1;
                idle_count = 0;
                break;
            }
        }
        
        if (!stolen) {
            idle_count++;
            if (idle_count < 100) {
                cpu_relax();
            } else if (idle_count < 1000) {
                sched_yield();
            } else {
                usleep(1000); // Sleep for 1ms
            }
        }
    }
    
    return NULL;
}

// Original ThreadPool structure for compatibility
struct ThreadPool {
    WorkStealingPool* ws_pool;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_cond_t idle_cond;
    Task* task_queue;
    Task* task_queue_tail;
    int num_threads;
    int active_threads;
    bool shutdown;
};

ThreadPool* thread_pool_create(int num_threads) {
    detect_cpu_features();
    
    ThreadPool* pool = calloc(1, sizeof(ThreadPool));
    if (!pool) return NULL;
    
    pool->num_threads = get_optimal_threads(num_threads);
    pool->shutdown = false;
    
    // Initialize traditional fields for compatibility
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);
    pthread_cond_init(&pool->idle_cond, NULL);
    
    // Create work-stealing pool
    WorkStealingPool* ws_pool = malloc(sizeof(WorkStealingPool));
    if (!ws_pool) {
        free(pool);
        return NULL;
    }
    
    ws_pool->num_threads = pool->num_threads;
    ws_pool->shutdown = 0;
    ws_pool->global_task_count = 0;
    
    // Create per-thread queues
    ws_pool->queues = malloc(pool->num_threads * sizeof(WorkStealingQueue));
    if (!ws_pool->queues) {
        free(ws_pool);
        free(pool);
        return NULL;
    }
    
    for (int i = 0; i < pool->num_threads; i++) {
        WorkStealingQueue* queue = create_queue(1024);
        if (!queue) {
            // Cleanup on failure
            for (int j = 0; j < i; j++) {
                free(ws_pool->queues[j].tasks);
            }
            free(ws_pool->queues);
            free(ws_pool);
            free(pool);
            return NULL;
        }
        ws_pool->queues[i] = *queue;
        free(queue);
    }
    
    // Create threads
    ws_pool->threads = malloc(pool->num_threads * sizeof(pthread_t));
    if (!ws_pool->threads) {
        for (int i = 0; i < pool->num_threads; i++) {
            free(ws_pool->queues[i].tasks);
        }
        free(ws_pool->queues);
        free(ws_pool);
        free(pool);
        return NULL;
    }
    
    // Start worker threads
    struct {
        WorkStealingPool* pool;
        int thread_id;
    }* thread_data = malloc(pool->num_threads * sizeof(*thread_data));
    
    for (int i = 0; i < pool->num_threads; i++) {
        thread_data[i].pool = ws_pool;
        thread_data[i].thread_id = i;
        
        if (pthread_create(&ws_pool->threads[i], NULL, worker_thread_stealing, &thread_data[i]) != 0) {
            // Cleanup on failure
            ws_pool->shutdown = 1;
            for (int j = 0; j < i; j++) {
                pthread_join(ws_pool->threads[j], NULL);
            }
            free(thread_data);
            for (int j = 0; j < pool->num_threads; j++) {
                free(ws_pool->queues[j].tasks);
            }
            free(ws_pool->queues);
            free(ws_pool->threads);
            free(ws_pool);
            free(pool);
            return NULL;
        }
    }
    
    pool->ws_pool = ws_pool;
    return pool;
}

int thread_pool_add_task(ThreadPool* pool, void (*function)(void*), void* argument) {
    if (!pool || !function) return -1;
    
    Task task = {function, argument, NULL};
    
    // Round-robin task distribution to minimize contention
    static _Alignas(CACHE_LINE_SIZE) volatile int next_queue = 0;
    int queue_id = __atomic_fetch_add(&next_queue, 1, __ATOMIC_RELAXED) % pool->num_threads;
    
    // Try the selected queue first
    if (queue_push(&pool->ws_pool->queues[queue_id], &task)) {
        __atomic_fetch_add(&pool->ws_pool->global_task_count, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_global_task_count, 1, __ATOMIC_RELAXED);
        return 0;
    }
    
    // If that fails, try other queues
    for (int i = 0; i < pool->num_threads; i++) {
        int try_queue = (queue_id + i) % pool->num_threads;
        if (queue_push(&pool->ws_pool->queues[try_queue], &task)) {
            __atomic_fetch_add(&pool->ws_pool->global_task_count, 1, __ATOMIC_RELAXED);
            __atomic_fetch_add(&g_global_task_count, 1, __ATOMIC_RELAXED);
            return 0;
        }
    }
    
    return -1; // All queues full
}

void thread_pool_wait(ThreadPool* pool) {
    if (!pool) return;
    
    // Spin-wait for better responsiveness on small workloads
    int spin_count = 0;
    while (__atomic_load_n(&pool->ws_pool->global_task_count, __ATOMIC_ACQUIRE) > 0) {
        if (spin_count < 1000) {
            cpu_relax();
            spin_count++;
        } else {
            usleep(100); // 0.1ms
            spin_count = 0;
        }
    }
}

void thread_pool_destroy(ThreadPool* pool) {
    if (!pool) return;
    
    // Signal shutdown
    __atomic_store_n(&pool->ws_pool->shutdown, 1, __ATOMIC_RELEASE);
    
    // Wait for all threads to finish
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->ws_pool->threads[i], NULL);
    }
    
    // Cleanup
    for (int i = 0; i < pool->num_threads; i++) {
        free(pool->ws_pool->queues[i].tasks);
    }
    free(pool->ws_pool->queues);
    free(pool->ws_pool->threads);
    free(pool->ws_pool);
    
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    pthread_cond_destroy(&pool->idle_cond);
    
    free(pool);
}

int thread_pool_get_thread_count(ThreadPool* pool) {
    return pool ? pool->num_threads : 0;
}

size_t thread_pool_get_queue_size(ThreadPool* pool) {
    if (!pool) return 0;
    return __atomic_load_n(&pool->ws_pool->global_task_count, __ATOMIC_RELAXED);
}

// =============================================================================
// JSON UTILITY FUNCTIONS (OPTIMIZED)
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
// JSON FLATTENER IMPLEMENTATION (ULTRA-OPTIMIZED)
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
    int is_sorted; // Track if pairs are sorted for binary search
} FlattenedArray;

typedef struct {
    cJSON* object;
    cJSON* result;
    pthread_mutex_t* result_mutex;
    char* key_buffer;
    size_t buffer_size;
} ThreadData;

// Optimized pool allocation with different size classes
static char* pool_alloc(FlattenedArray* array, size_t size) {
    size = (size + 15) & ~15; // 16-byte alignment
    
    if (array->pool_used + size <= array->pool_size) {
        char* ptr = array->memory_pool + array->pool_used;
        array->pool_used += size;
        return ptr;
    }
    
    // Pool exhausted, fall back to regular allocation
    return malloc(size);
}

static void init_flattened_array(FlattenedArray* array, int initial_capacity) {
    int capacity = initial_capacity < INITIAL_ARRAY_CAPACITY ? INITIAL_ARRAY_CAPACITY : initial_capacity;
    
    detect_cpu_features();
    init_global_pools();
    
    // Align capacity to cache line for better performance
    capacity = (capacity + g_cpu.cache_line_size - 1) & ~(g_cpu.cache_line_size - 1);
    
    array->pairs = malloc(capacity * sizeof(FlattenedPair));
    array->count = 0;
    array->capacity = capacity;
    array->is_sorted = 1; // Start sorted
    
    // Size pool based on expected usage
    size_t pool_size = MEMORY_POOL_SIZE + (initial_capacity * 128);
    
    // Use huge pages if available
    #ifdef __linux__
    array->memory_pool = mmap(NULL, pool_size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (array->memory_pool == MAP_FAILED) {
        array->memory_pool = malloc(pool_size);
    }
    #else
    array->memory_pool = malloc(pool_size);
    #endif
    
    array->pool_used = 0;
    array->pool_size = pool_size;
}

// Ultra-fast key building with SIMD-optimized operations
static inline void build_key_optimized(char* restrict buffer, size_t buffer_size,
                                     const char* restrict prefix, const char* restrict suffix,
                                     int is_array_index, int index) {
    if (UNLIKELY(!buffer || buffer_size == 0)) return;
    
    if (!prefix || *prefix == '\0') {
        if (is_array_index) {
            snprintf(buffer, buffer_size, "[%d]", index);
        } else if (suffix) {
            size_t suffix_len = strlen_simd(suffix);
            if (suffix_len + 1 < buffer_size) {
                fast_memcpy(buffer, suffix, suffix_len);
                buffer[suffix_len] = '\0';
            } else {
                strncpy(buffer, suffix, buffer_size - 1);
                buffer[buffer_size - 1] = '\0';
            }
        } else {
            buffer[0] = '\0';
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
    } else if (suffix) {
        size_t suffix_len = strlen_simd(suffix);
        if (prefix_len == 0) {
            if (suffix_len + 1 < buffer_size) {
                fast_memcpy(buffer, suffix, suffix_len);
                buffer[suffix_len] = '\0';
            } else {
                strncpy(buffer, suffix, buffer_size - 1);
                buffer[buffer_size - 1] = '\0';
            }
        } else {
            // Optimized concatenation
            if (prefix_len + suffix_len + 2 < buffer_size) {
                fast_memcpy(buffer, prefix, prefix_len);
                buffer[prefix_len] = '.';
                fast_memcpy(buffer + prefix_len + 1, suffix, suffix_len);
                buffer[prefix_len + suffix_len + 1] = '\0';
            } else {
                snprintf(buffer, buffer_size, "%.*s.%.*s", 
                        (int)prefix_len, prefix, (int)suffix_len, suffix);
            }
        }
    } else {
        if (prefix_len + 1 < buffer_size) {
            fast_memcpy(buffer, prefix, prefix_len);
            buffer[prefix_len] = '\0';
        } else {
            strncpy(buffer, prefix, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
        }
    }
}

static char* pool_strdup(FlattenedArray* array, const char* str) {
    if (UNLIKELY(!str)) return NULL;
    
    size_t len = strlen_simd(str) + 1;
    char* new_str;
    
    // Use pool for small strings
    if (LIKELY(len <= 256)) {
        new_str = pool_alloc(array, len);
        if (LIKELY(new_str != NULL)) {
            fast_memcpy(new_str, str, len);
            return new_str;
        }
    }
    
    // Fallback to slab allocator
    if (g_cjson_node_pool && len <= 256) {
        new_str = slab_alloc(g_cjson_node_pool);
        if (new_str) {
            fast_memcpy(new_str, str, len);
            return new_str;
        }
    }
    
    return my_strdup(str);
}

// Hot path function with aggressive optimization
HOT_PATH void add_pair(FlattenedArray* array, const char* key, cJSON* value) {
    if (UNLIKELY(array->count >= array->capacity)) {
        int new_capacity = array->capacity << 1;
        FlattenedPair* new_pairs = realloc(array->pairs, new_capacity * sizeof(FlattenedPair));
        if (UNLIKELY(!new_pairs)) {
            return;
        }
        array->pairs = new_pairs;
        array->capacity = new_capacity;
        
        // Prefetch next cache lines
        PREFETCH_WRITE(&array->pairs[array->count]);
        PREFETCH_WRITE(&array->pairs[array->count + g_cpu.cache_line_size / sizeof(FlattenedPair)]);
    }
    
    FlattenedPair* pair = &array->pairs[array->count];
    pair->key = pool_strdup(array, key);
    pair->value = value;
    array->count++;
    
    // Check if still sorted (for optimization)
    if (array->is_sorted && array->count > 1) {
        if (strcmp(array->pairs[array->count - 2].key, pair->key) > 0) {
            array->is_sorted = 0;
        }
    }
    
    // Prefetch next pair location
    if (LIKELY(array->count < array->capacity)) {
        PREFETCH_WRITE(&array->pairs[array->count]);
    }
}

static void free_flattened_array(FlattenedArray* array) {
    // Free keys that aren't in the pool
    for (int i = 0; i < array->count; i++) {
        char* key = array->pairs[i].key;
        if (key && (key < array->memory_pool || key >= array->memory_pool + array->pool_size)) {
            free(key);
        }
    }
    
    free(array->pairs);
    
    #ifdef __linux__
    if (array->memory_pool != MAP_FAILED) {
        munmap(array->memory_pool, array->pool_size);
    } else {
        free(array->memory_pool);
    }
    #else
    free(array->memory_pool);
    #endif
    
    memset(array, 0, sizeof(*array));
}

// Recursive flattening with tail-call optimization simulation
static void flatten_json_recursive(cJSON* json, const char* prefix, FlattenedArray* result) {
    if (UNLIKELY(!json)) return;
    
    char key_buffer[MAX_KEY_LENGTH];
    
    // Handle different JSON types with optimized dispatch
    switch (json->type & 0xFF) {
        case cJSON_Object: {
            cJSON* child = json->child;
            while (child) {
                build_key_optimized(key_buffer, sizeof(key_buffer), prefix, child->string, 0, 0);
                
                // Tail recursion optimization for single child
                if (!child->next && (child->type == cJSON_Object || child->type == cJSON_Array)) {
                    json = child;
                    prefix = my_strdup(key_buffer); // TODO: optimize this allocation
                    goto tail_recurse;
                }
                
                flatten_json_recursive(child, key_buffer, result);
                child = child->next;
            }
            break;
        }
        
        case cJSON_Array: {
            cJSON* child = json->child;
            int i = 0;
            while (child) {
                build_key_optimized(key_buffer, sizeof(key_buffer), prefix, NULL, 1, i);
                
                // Tail recursion optimization for single child
                if (!child->next && (child->type == cJSON_Object || child->type == cJSON_Array)) {
                    json = child;
                    prefix = my_strdup(key_buffer); // TODO: optimize this allocation
                    goto tail_recurse;
                }
                
                flatten_json_recursive(child, key_buffer, result);
                child = child->next;
                i++;
            }
            break;
        }
        
        default:
            add_pair(result, prefix, json);
            return;
    }
    
    return;
    
tail_recurse:
    // Simulate tail recursion
    flatten_json_recursive(json, prefix, result);
}

// Highly optimized JSON creation with pre-calculated sizes
static cJSON* create_flattened_json(FlattenedArray* flattened_array) {
    cJSON* result = cJSON_CreateObject();
    if (UNLIKELY(!result)) return NULL;
    
    // Sort pairs if not sorted for better cache locality during iteration
    if (!flattened_array->is_sorted && flattened_array->count > 16) {
        // Quick sort implementation would go here
        // For now, keeping simple
    }
    
    // Pre-allocate hashtable space if cJSON supported it
    // This would significantly improve performance for large objects
    
    for (int i = 0; i < flattened_array->count; i++) {
        FlattenedPair* pair = &flattened_array->pairs[i];
        
        // Optimized value creation based on type
        switch (pair->value->type & 0xFF) {
            case cJSON_False:
                cJSON_AddFalseToObject(result, pair->key);
                break;
            case cJSON_True:
                cJSON_AddTrueToObject(result, pair->key);
                break;
            case cJSON_NULL:
                cJSON_AddNullToObject(result, pair->key);
                break;
            case cJSON_Number:
                // Optimize integer vs float detection
                if (pair->value->valuedouble == (double)pair->value->valueint &&
                    pair->value->valuedouble >= INT_MIN && pair->value->valuedouble <= INT_MAX) {
                    cJSON_AddNumberToObject(result, pair->key, pair->value->valueint);
                } else {
                    cJSON_AddNumberToObject(result, pair->key, pair->value->valuedouble);
                }
                break;
            case cJSON_String:
                cJSON_AddStringToObject(result, pair->key, pair->value->valuestring);
                break;
            default:
                break;
        }
        
        // Prefetch next pair for better cache utilization
        if (i + 1 < flattened_array->count) {
            PREFETCH_READ(&flattened_array->pairs[i + 1]);
        }
    }
    
    return result;
}

static cJSON* flatten_single_object(cJSON* json) {
    if (UNLIKELY(!json)) return NULL;
    
    // Estimate initial capacity based on object complexity
    int estimated_capacity = INITIAL_ARRAY_CAPACITY;
    if (json->type == cJSON_Object) {
        // Count immediate children for better estimation
        int child_count = 0;
        cJSON* child = json->child;
        while (child && child_count < 100) { // Limit counting for performance
            child_count++;
            child = child->next;
        }
        estimated_capacity = child_count * 4; // Assume average nesting of 4
    }
    
    FlattenedArray flattened_array;
    init_flattened_array(&flattened_array, estimated_capacity);
    
    flatten_json_recursive(json, "", &flattened_array);
    
    cJSON* flattened_json = create_flattened_json(&flattened_array);
    free_flattened_array(&flattened_array);
    
    return flattened_json;
}

static void flatten_object_task(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    data->result = flatten_single_object(data->object);
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
    if (!result) return NULL;
    
    // Enhanced heuristics for threading decision
    int should_use_threads = use_threads && 
                            array_size >= MIN_BATCH_SIZE_FOR_MT && 
                            get_optimal_threads(num_threads) > 1;
    
    // Additional check: estimate work complexity
    if (should_use_threads && array_size < 1000) {
        // Sample first few objects to estimate complexity
        int complex_objects = 0;
        for (int i = 0; i < MIN(array_size, 10); i++) {
            cJSON* item = cJSON_GetArrayItem(json_array, i);
            if (item && item->type == cJSON_Object) {
                int child_count = 0;
                cJSON* child = item->child;
                while (child && child_count < 20) {
                    child_count++;
                    child = child->next;
                }
                if (child_count > 5) complex_objects++;
            }
        }
        
        // Only use threading if objects are complex enough
        if (complex_objects < 3) {
            should_use_threads = 0;
        }
    }
    
    if (!should_use_threads) {
        // Single-threaded path with optimizations
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(json_array, i);
            cJSON* flattened = flatten_single_object(item);
            if (flattened) {
                cJSON_AddItemToArray(result, flattened);
            }
            
            // Prefetch next item
            if (i + 1 < array_size) {
                PREFETCH_READ(cJSON_GetArrayItem(json_array, i + 1));
            }
        }
        return result;
    }
    
    // Multi-threaded path
    ThreadPool* pool = thread_pool_create(num_threads);
    if (!pool) {
        // Fallback to single-threaded
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(json_array, i);
            cJSON* flattened = flatten_single_object(item);
            if (flattened) {
                cJSON_AddItemToArray(result, flattened);
            }
        }
        return result;
    }
    
    ThreadData* thread_data_array = calloc(array_size, sizeof(ThreadData));
    if (!thread_data_array) {
        thread_pool_destroy(pool);
        return result;
    }
    
    // Submit all tasks
    for (int i = 0; i < array_size; i++) {
        thread_data_array[i].object = cJSON_GetArrayItem(json_array, i);
        thread_data_array[i].result = NULL;
        
        if (thread_pool_add_task(pool, flatten_object_task, &thread_data_array[i]) != 0) {
            // Queue full, execute synchronously
            flatten_object_task(&thread_data_array[i]);
        }
    }
    
    // Wait for completion
    thread_pool_wait(pool);
    
    // Collect results in order
    for (int i = 0; i < array_size; i++) {
        if (thread_data_array[i].result) {
            cJSON_AddItemToArray(result, thread_data_array[i].result);
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
        
        // Quick scan for objects (limit scan for performance)
        for (int i = 0; i < MIN(array_size, 50); i++) {
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

// Path extraction with types (optimized)
static const char* get_cjson_type_string(const cJSON* item) {
    if (!item) return "null";
    
    switch (item->type & 0xFF) {
        case cJSON_False:
        case cJSON_True:
            return "boolean";
        case cJSON_NULL:
            return "null";
        case cJSON_Number:
            if (item->valuedouble == (double)item->valueint &&
                item->valuedouble >= INT_MIN && item->valuedouble <= INT_MAX) {
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

            if (prefix && strlen_simd(prefix) > 0) {
                build_key_optimized(key_buffer, sizeof(key_buffer), prefix, child_name, 0, 0);
            } else {
                strncpy(key_buffer, child_name, sizeof(key_buffer) - 1);
                key_buffer[sizeof(key_buffer) - 1] = '\0';
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
// ULTRA-OPTIMIZED JSON SCHEMA GENERATOR
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
    // Optimization: cache frequently accessed properties
    struct PropertyNode* property_cache[8];
    int cache_size;
} SchemaNode;

typedef struct PropertyNode {
    char* name;
    size_t name_len;
    uint32_t name_hash; // Cache hash for faster lookups
    SchemaNode* schema;
    int required;
    struct PropertyNode* next;
} PropertyNode;

// Fast hash function for property names
static uint32_t hash_property_name(const char* name, size_t len) {
    uint32_t hash = 2166136261u; // FNV-1a offset basis
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)name[i];
        hash *= 16777619u; // FNV-1a prime
    }
    return hash;
}

static SchemaNode* create_schema_node(SchemaType type) {
    // Temporarily use regular malloc to avoid memory pool issues
    SchemaNode* node = malloc(sizeof(SchemaNode));
    if (!node) return NULL;

    memset(node, 0, sizeof(SchemaNode));
    node->type = type;
    node->required = 1;

    return node;
}

// Optimized property addition with caching
HOT_PATH void add_property(SchemaNode* node, const char* name, SchemaNode* property_schema, int required) {
    PropertyNode* prop = slab_alloc(g_property_node_pool);
    if (UNLIKELY(!prop)) {
        prop = malloc(sizeof(PropertyNode));
        if (!prop) return;
    }

    size_t name_len = strlen_simd(name);
    prop->name = my_strdup(name);
    prop->name_len = name_len;
    prop->name_hash = hash_property_name(name, name_len);
    prop->schema = property_schema;
    prop->required = required;
    prop->next = node->properties;
    node->properties = prop;

    // Add to cache if there's space
    if (node->cache_size < 8) {
        node->property_cache[node->cache_size++] = prop;
    }

    if (required) {
        if (UNLIKELY(node->required_count >= node->required_capacity)) {
            int new_capacity = node->required_capacity == 0 ? 8 : node->required_capacity * 2;
            char** new_props = realloc(node->required_props, new_capacity * sizeof(char*));
            if (UNLIKELY(!new_props)) return;

            node->required_props = new_props;
            node->required_capacity = new_capacity;
        }
        node->required_props[node->required_count++] = my_strdup(name);
    }
}

// Optimized property lookup with cache and hash
static PropertyNode* find_property(SchemaNode* node, const char* name) {
    if (!node || !name) return NULL;
    
    size_t name_len = strlen_simd(name);
    uint32_t name_hash = hash_property_name(name, name_len);
    
    // Check cache first
    for (int i = 0; i < node->cache_size; i++) {
        PropertyNode* cached = node->property_cache[i];
        if (cached->name_hash == name_hash && 
            cached->name_len == name_len &&
            fast_memcmp(cached->name, name, name_len) == 0) {
            return cached;
        }
    }
    
    // Search in linked list
    PropertyNode* prop = node->properties;
    while (prop) {
        if (prop->name_hash == name_hash && 
            prop->name_len == name_len &&
            fast_memcmp(prop->name, name, name_len) == 0) {
            
            // Add to cache if there's space
            if (node->cache_size < 8) {
                node->property_cache[node->cache_size++] = prop;
            }
            return prop;
        }
        prop = prop->next;
    }
    return NULL;
}

static void free_schema_node(SchemaNode* node) {
    if (!node) return;

    // Safety check for valid pointer
    if ((uintptr_t)node < 0x1000) {
        return; // Invalid pointer
    }

    if (node->items) {
        free_schema_node(node->items);
    }

    PropertyNode* prop = node->properties;
    while (prop) {
        PropertyNode* next = prop->next;
        my_strfree(prop->name);
        free_schema_node(prop->schema);
        slab_free(g_property_node_pool, prop);
        prop = next;
    }

    for (int i = 0; i < node->required_count; i++) {
        my_strfree(node->required_props[i]);
    }
    free(node->required_props);

    if (node->enum_values) {
        cJSON_Delete(node->enum_values);
    }

    slab_free(g_cjson_node_pool, node);
}

static SchemaType get_schema_type(cJSON* json) {
    switch (json->type & 0xFF) {
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

// Optimized schema merging with type compatibility matrix
static const int type_compatibility[8][8] = {
    // NULL, BOOL, INT, NUM, STR, ARR, OBJ, MIX
    {1, 0, 0, 0, 0, 0, 0, 1}, // NULL
    {0, 1, 0, 0, 0, 0, 0, 1}, // BOOLEAN
    {0, 0, 1, 1, 0, 0, 0, 1}, // INTEGER
    {0, 0, 1, 1, 0, 0, 0, 1}, // NUMBER
    {0, 0, 0, 0, 1, 0, 0, 1}, // STRING
    {0, 0, 0, 0, 0, 1, 0, 1}, // ARRAY
    {0, 0, 0, 0, 0, 0, 1, 1}, // OBJECT
    {1, 1, 1, 1, 1, 1, 1, 1}  // MIXED
};

static SchemaNode* merge_schema_nodes(SchemaNode* node1, SchemaNode* node2) {
    if (!node1) return node2;
    if (!node2) return node1;
    
    SchemaType merged_type;
    if (type_compatibility[node1->type][node2->type]) {
        merged_type = (node1->type == node2->type) ? node1->type : 
                     (node1->type == TYPE_INTEGER && node2->type == TYPE_NUMBER) ? TYPE_NUMBER :
                     (node1->type == TYPE_NUMBER && node2->type == TYPE_INTEGER) ? TYPE_NUMBER :
                     TYPE_MIXED;
    } else {
        merged_type = TYPE_MIXED;
    }
    
    SchemaNode* merged = create_schema_node(merged_type);
    merged->required = node1->required && node2->required;
    merged->nullable = node1->nullable || node2->nullable || 
                      node1->type == TYPE_NULL || node2->type == TYPE_NULL;
    
    switch (merged_type) {
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
            // Merge properties efficiently
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
                if (!find_property(merged, prop2->name)) {
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

// Optimized JSON analysis with early type detection
static SchemaNode* analyze_json_value(cJSON* json) {
    if (!json) return NULL;
    
    SchemaType type = get_schema_type(json);
    SchemaNode* node = create_schema_node(type);
    
    if (type == TYPE_NULL) {
        node->required = 0;
        node->nullable = 1;
        return node;
    }
    
    switch (type) {
        case TYPE_ARRAY: {
            int array_size = cJSON_GetArraySize(json);
            if (LIKELY(array_size > 0)) {
                // Optimized sampling strategy
                int sample_size = MIN(array_size, MAX_ARRAY_SAMPLE_SIZE);
                int step = array_size > MAX_ARRAY_SAMPLE_SIZE ? array_size / MAX_ARRAY_SAMPLE_SIZE : 1;
                
                SchemaNode* items_schema = NULL;
                SchemaType first_type = TYPE_NULL;
                bool types_uniform = true;

                for (int i = 0; i < array_size && i / step < sample_size; i += step) {
                    cJSON* item = cJSON_GetArrayItem(json, i);
                    if (!item) continue;
                    
                    SchemaType item_type = get_schema_type(item);
                    
                    if (first_type == TYPE_NULL) {
                        first_type = item_type;
                        items_schema = analyze_json_value(item);
                    } else if (item_type != first_type) {
                        types_uniform = false;
                        SchemaNode* item_schema = analyze_json_value(item);
                        items_schema = merge_schema_nodes(items_schema, item_schema);
                        free_schema_node(item_schema);
                    }
                    
                    // Early exit if we know it's mixed
                    if (!types_uniform && items_schema && items_schema->type == TYPE_MIXED) {
                        break;
                    }
                }

                node->items = items_schema ? items_schema : create_schema_node(TYPE_NULL);
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

// Optimized schema JSON generation with reusable components
static cJSON* schema_node_to_json(SchemaNode* node) {
    if (!node) return NULL;
    
    cJSON* schema = cJSON_CreateObject();
    if (!schema) return NULL;
    
    cJSON_AddStringToObject(schema, "$schema", "http://json-schema.org/draft-07/schema#");
    
    if (node->type == TYPE_MIXED) {
        cJSON* type_array = cJSON_CreateArray();
        const char* types[] = {"string", "number", "integer", "boolean", "object", "array"};
        for (int i = 0; i < 6; i++) {
            cJSON_AddItemToArray(type_array, cJSON_CreateString(types[i]));
        }
        
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
                cJSON* items_schema = schema_node_to_json(node->items);
                if (items_schema) {
                    cJSON_DeleteItemFromObject(items_schema, "$schema");
                    cJSON_AddItemToObject(schema, "items", items_schema);
                }
            }
            break;
            
        case TYPE_OBJECT:
            if (node->properties) {
                cJSON* props = cJSON_CreateObject();
                cJSON* required = cJSON_CreateArray();
                
                PropertyNode* prop = node->properties;
                while (prop) {
                    cJSON* prop_schema = schema_node_to_json(prop->schema);
                    if (prop_schema) {
                        cJSON_DeleteItemFromObject(prop_schema, "$schema");
                        cJSON_AddItemToObject(props, prop->name, prop_schema);
                        
                        if (prop->required) {
                            cJSON_AddItemToArray(required, cJSON_CreateString(prop->name));
                        }
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

    init_global_pools();

    SchemaNode* schema_node = analyze_json_value(json);
    if (!schema_node) return NULL;
    
    cJSON* schema = schema_node_to_json(schema_node);
    free_schema_node(schema_node);
    
    return schema;
}

cJSON* generate_schema_from_batch(cJSON* json_array, int use_threads, int num_threads) {
    if (!json_array || json_array->type != cJSON_Array) {
        return NULL;
    }

    init_global_pools();

    int array_size = cJSON_GetArraySize(json_array);
    if (array_size == 0) {
        return cJSON_CreateObject();
    }

    // Enhanced threading heuristics for schema generation
    bool should_use_threads = use_threads && 
                             array_size >= MIN_BATCH_SIZE_FOR_MT && 
                             get_optimal_threads(num_threads) > 1;

    SchemaNode** schemas = malloc(array_size * sizeof(SchemaNode*));
    if (!schemas) return NULL;

    if (should_use_threads) {
        ThreadPool* pool = thread_pool_create(num_threads);
        if (pool) {
            ThreadData* thread_data_array = malloc(array_size * sizeof(ThreadData));
            if (thread_data_array) {
                // Submit all tasks
                for (int i = 0; i < array_size; i++) {
                    thread_data_array[i].object = cJSON_GetArrayItem(json_array, i);
                    thread_data_array[i].result = NULL;
                    
                    if (thread_pool_add_task(pool, generate_schema_task, &thread_data_array[i]) != 0) {
                        generate_schema_task(&thread_data_array[i]);
                    }
                }
                
                thread_pool_wait(pool);
                
                // Collect results
                for (int i = 0; i < array_size; i++) {
                    schemas[i] = (SchemaNode*)thread_data_array[i].result;
                }
                
                free(thread_data_array);
            } else {
                should_use_threads = false;
            }
            thread_pool_destroy(pool);
        } else {
            should_use_threads = false;
        }
    }

    if (!should_use_threads) {
        // Single-threaded fallback
        for (int i = 0; i < array_size; i++) {
            schemas[i] = analyze_json_value(cJSON_GetArrayItem(json_array, i));
        }
    }

    // Merge schemas efficiently
    SchemaNode* merged_schema = schemas[0];
    for (int i = 1; i < array_size; i++) {
        if (schemas[i]) {
            SchemaNode* new_merged = merge_schema_nodes(merged_schema, schemas[i]);
            if (merged_schema != schemas[0]) {
                free_schema_node(merged_schema);
            }
            free_schema_node(schemas[i]);
            merged_schema = new_merged;
        }
    }

    cJSON* result = NULL;
    if (merged_schema) {
        result = schema_node_to_json(merged_schema);
        if (merged_schema != schemas[0]) {
            free_schema_node(merged_schema);
        }
    }
    
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
// OPTIMIZED COMMAND LINE INTERFACE
// =============================================================================

#ifndef CJSON_TOOLS_NO_MAIN

// Print usage information with performance tips
void print_usage(const char* program_name) {
    printf("JSON Tools - High-Performance JSON Processing Utility v1.9.0\n\n");
    printf("Usage: %s [options] [input_file]\n\n", program_name);
    printf("🚀 PERFORMANCE OPTIONS:\n");
    printf("  -t, --threads [num]        Use multi-threading (auto-detect optimal count)\n");
    printf("                             Recommended for files >100KB or >1000 objects\n");
    printf("  --threads 0                Auto-detect optimal thread count\n");
    printf("  --threads N                Use exactly N threads (1-128)\n\n");
    
    printf("🔧 PROCESSING OPTIONS:\n");
    printf("  -f, --flatten              Flatten nested JSON (default action)\n");
    printf("  -s, --schema               Generate JSON schema (Draft-07)\n");
    printf("  -e, --remove-empty         Remove keys with empty string values\n");
    printf("  -n, --remove-nulls         Remove keys with null values\n");
    printf("  -r, --replace-keys <pattern> <replacement>\n");
    printf("                             Replace keys matching regex pattern\n");
    printf("  -v, --replace-values <pattern> <replacement>\n");
    printf("                             Replace string values matching regex pattern\n\n");
    
    printf("📄 OUTPUT OPTIONS:\n");
    printf("  -p, --pretty               Pretty-print output (formatted JSON)\n");
    printf("  -o, --output <file>        Write output to file instead of stdout\n");
    printf("  -h, --help                 Show this help message\n\n");
    
    printf("📥 INPUT:\n");
    printf("  If no input file is specified, input is read from stdin.\n");
    printf("  Use '-' as input_file to explicitly read from stdin.\n\n");
    
    printf("💡 PERFORMANCE EXAMPLES:\n");
    printf("  %s -f large_data.json                    # Single-threaded flatten\n", program_name);
    printf("  %s -f -t 0 large_data.json               # Auto-threaded flatten\n", program_name);
    printf("  %s -f -t 8 huge_dataset.json             # 8-thread flatten\n", program_name);
    printf("  %s -s -t 4 batch_data.json               # 4-thread schema generation\n", program_name);
    printf("  cat data.json | %s -f -t 0              # Streaming with auto-threading\n", program_name);
    printf("  %s -e -p messy_data.json                 # Clean & format\n", program_name);
    printf("  %s -r '^old_' 'new_' -t 2 data.json     # Regex replace with threading\n", program_name);
    
    printf("\n🎯 OPTIMIZATION TIPS:\n");
    printf("  • Use threading (-t) for files >100KB or >1000 objects\n");
    printf("  • Auto-threading (-t 0) adapts to your CPU and workload\n");
    printf("  • Regex operations work on Unix-like systems (Linux, macOS)\n");
    printf("  • Memory usage scales with input size and threading level\n");
    printf("  • For huge datasets, consider processing in smaller batches\n\n");
}

int main(int argc, char* argv[]) {
    // Early initialization for maximum performance
    detect_cpu_features();
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

    // Optimized command line argument parsing with jump table
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
                    action_schema = action_remove_empty = action_remove_nulls = 0;
                    action_replace_keys = action_replace_values = 0;
                    continue;
                case 's':
                    action_schema = 1;
                    action_flatten = action_remove_empty = action_remove_nulls = 0;
                    action_replace_keys = action_replace_values = 0;
                    break;
                case 'e':
                    action_remove_empty = 1;
                    action_flatten = action_schema = action_remove_nulls = 0;
                    action_replace_keys = action_replace_values = 0;
                    break;
                case 'n':
                    action_remove_nulls = 1;
                    action_flatten = action_schema = action_remove_empty = 0;
                    action_replace_keys = action_replace_values = 0;
                    break;
                case 'r':
                    if (i + 2 >= argc) {
                        fprintf(stderr, "Error: -r requires pattern and replacement arguments\n");
                        cleanup_global_pools();
                        return 1;
                    }
                    action_replace_keys = 1;
                    action_flatten = action_schema = action_remove_empty = action_remove_nulls = 0;
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
                    action_replace_values = 1;
                    action_flatten = action_schema = action_remove_empty = action_remove_nulls = 0;
                    action_replace_keys = 0;
                    replace_values_pattern = argv[++i];
                    replace_values_replacement = argv[++i];
                    break;
                case 't':
                    use_threads = 1;
                    if (i + 1 < argc && argv[i + 1][0] != '-') {
                        num_threads = atoi(argv[++i]);
                        if (num_threads < 0) num_threads = 0;
                        if (num_threads > 128) num_threads = 128;
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

        // Long options with optimized string comparison
        if (arg[0] == '-' && arg[1] == '-') {
            const char* long_opt = arg + 2;
            
            if (strcmp(long_opt, "help") == 0) {
                print_usage(argv[0]);
                cleanup_global_pools();
                return 0;
            } else if (strcmp(long_opt, "flatten") == 0) {
                action_flatten = 1;
                action_schema = action_remove_empty = action_remove_nulls = 0;
                action_replace_keys = action_replace_values = 0;
            } else if (strcmp(long_opt, "schema") == 0) {
                action_schema = 1;
                action_flatten = action_remove_empty = action_remove_nulls = 0;
                action_replace_keys = action_replace_values = 0;
            } else if (strcmp(long_opt, "remove-empty") == 0) {
                action_remove_empty = 1;
                action_flatten = action_schema = action_remove_nulls = 0;
                action_replace_keys = action_replace_values = 0;
            } else if (strcmp(long_opt, "remove-nulls") == 0) {
                action_remove_nulls = 1;
                action_flatten = action_schema = action_remove_empty = 0;
                action_replace_keys = action_replace_values = 0;
            } else if (strcmp(long_opt, "replace-keys") == 0) {
                if (i + 2 >= argc) {
                    fprintf(stderr, "Error: --replace-keys requires pattern and replacement arguments\n");
                    cleanup_global_pools();
                    return 1;
                }
                action_replace_keys = 1;
                action_flatten = action_schema = action_remove_empty = action_remove_nulls = 0;
                action_replace_values = 0;
                replace_pattern = argv[++i];
                replace_replacement = argv[++i];
            } else if (strcmp(long_opt, "replace-values") == 0) {
                if (i + 2 >= argc) {
                    fprintf(stderr, "Error: --replace-values requires pattern and replacement arguments\n");
                    cleanup_global_pools();
                    return 1;
                }
                action_replace_values = 1;
                action_flatten = action_schema = action_remove_empty = action_remove_nulls = 0;
                action_replace_keys = 0;
                replace_values_pattern = argv[++i];
                replace_values_replacement = argv[++i];
            } else if (strcmp(long_opt, "threads") == 0) {
                use_threads = 1;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    num_threads = atoi(argv[++i]);
                    if (num_threads < 0) num_threads = 0;
                    if (num_threads > 128) num_threads = 128;
                }
            } else if (strcmp(long_opt, "pretty") == 0) {
                pretty_print = 1;
            } else if (strcmp(long_opt, "output") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: --output requires output file argument\n");
                    cleanup_global_pools();
                    return 1;
                }
                output_file = argv[++i];
            } else {
                fprintf(stderr, "Error: Unknown long option '--%s'\n", long_opt);
                cleanup_global_pools();
                return 1;
            }
        } else if (input_file == NULL) {
            input_file = argv[i];
        } else {
            fprintf(stderr, "Error: Multiple input files not supported\n");
            cleanup_global_pools();
            return 1;
        }
    }

    // Performance information for user
    if (use_threads && num_threads == 0) {
        num_threads = get_optimal_threads(0);
        if (isatty(STDERR_FILENO)) {
            fprintf(stderr, "🚀 Auto-detected %d threads for optimal performance\n", num_threads);
        }
    }

    // Read JSON input with optimizations
    char* json_string = NULL;

    if (input_file == NULL || strcmp(input_file, "-") == 0) {
        json_string = read_json_stdin();
    } else {
        json_string = read_json_file(input_file);
    }

    if (!json_string) {
        fprintf(stderr, "Error: Failed to read JSON input\n");
        cleanup_global_pools();
        return 1;
    }

    // Show performance hint for large inputs
    size_t input_size = strlen_simd(json_string);
    if (input_size > 100000 && !use_threads && isatty(STDERR_FILENO)) {
        fprintf(stderr, "💡 Tip: Use -t 0 for auto-threading on large files (%.1fMB detected)\n", 
                input_size / 1024.0 / 1024.0);
    }

    // Process JSON based on selected action with timing
    char* result = NULL;
    clock_t start_time = clock();

    if (action_flatten) {
        result = flatten_json_string(json_string, use_threads, num_threads);
    } else if (action_schema) {
        result = generate_schema_from_string(json_string, use_threads, num_threads);
    } else if (action_remove_empty || action_remove_nulls || action_replace_keys || action_replace_values) {
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
            result = pretty_print ? cJSON_Print(processed) : cJSON_PrintUnformatted(processed);
            cJSON_Delete(processed);
        }
    }

    free(json_string);

    if (!result) {
        fprintf(stderr, "Error: Failed to process JSON\n");
        cleanup_global_pools();
        return 1;
    }

    // Show performance information
    clock_t end_time = clock();
    double processing_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    if (processing_time > 0.1 && isatty(STDERR_FILENO)) {
        double throughput = input_size / processing_time / 1024.0 / 1024.0;
        fprintf(stderr, "⚡ Processed %.1fMB in %.3fs (%.1fMB/s)\n", 
                input_size / 1024.0 / 1024.0, processing_time, throughput);
    }

    // Output result with error handling
    if (output_file) {
        FILE* output = fopen(output_file, "w");
        if (!output) {
            fprintf(stderr, "Error: Could not open output file %s\n", output_file);
            free(result);
            cleanup_global_pools();
            return 1;
        }
        
        size_t result_len = strlen_simd(result);
        size_t written = fwrite(result, 1, result_len, output);
        fputc('\n', output);
        
        if (written != result_len) {
            fprintf(stderr, "Error: Failed to write complete output\n");
            fclose(output);
            free(result);
            cleanup_global_pools();
            return 1;
        }
        
        fclose(output);
        
        if (isatty(STDERR_FILENO)) {
            fprintf(stderr, "✅ Output written to %s\n", output_file);
        }
    } else {
        printf("%s\n", result);
    }

    free(result);
    cleanup_global_pools();
    return 0;
}

#endif // CJSON_TOOLS_NO_MAIN