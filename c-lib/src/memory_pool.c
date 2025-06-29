#include "../include/memory_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
    #include <windows.h>
    #include <malloc.h>
    // Windows doesn't have MAP_ANONYMOUS
    #define MAP_ANONYMOUS 0x20
#else
    #ifdef __unix__
    #include <sys/mman.h>
    #include <unistd.h>
    // Handle different MAP_ANONYMOUS definitions across platforms
    #if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
    #define MAP_ANONYMOUS MAP_ANON
    #elif !defined(MAP_ANONYMOUS)
    #define MAP_ANONYMOUS 0x20
    #endif
    #endif
#endif

// Enhanced platform-specific memory optimizations
#if defined(__linux__)
    #ifdef __has_include
        #if __has_include(<linux/mman.h>)
            #include <linux/mman.h>  // For MAP_HUGE_*
            #define HAS_TRANSPARENT_HUGEPAGES 1
        #endif
    #endif
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
    #define HAS_SUPERPAGE_SUPPORT 1
#elif defined(__WINDOWS__)
    #include <windows.h>
    #define HAS_LARGE_PAGES 1
#endif

// CPU cache line detection
static size_t get_cache_line_size(void) {
    static size_t cache_line_size = 0;
    if (cache_line_size == 0) {
#if defined(__linux__)
        long size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
        cache_line_size = (size > 0) ? (size_t)size : 64;
#elif defined(__WINDOWS__) && defined(HAS_LARGE_PAGES)
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer[256];
        DWORD length = sizeof(buffer);
        if (GetLogicalProcessorInformation(buffer, &length)) {
            for (DWORD i = 0; i < length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); i++) {
                if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1) {
                    cache_line_size = buffer[i].Cache.LineSize;
                    break;
                }
            }
        }
        if (cache_line_size == 0) cache_line_size = 64;
#else
        cache_line_size = 64; // Default assumption
#endif
    }
    return cache_line_size;
}

// For aligned_alloc - only define if not available
#if !defined(__APPLE__) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L)
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
// Fallback for older systems
static inline void* my_aligned_alloc(size_t alignment, size_t size) {
    (void)alignment;
    return malloc(size);
}
#define aligned_alloc my_aligned_alloc
#endif
#endif

// Global pools
SlabAllocator* g_cjson_node_pool = NULL;
SlabAllocator* g_property_node_pool = NULL;
SlabAllocator* g_task_pool = NULL;

SlabAllocator* slab_allocator_create(size_t object_size, size_t initial_objects) {
    SlabAllocator* allocator = malloc(sizeof(SlabAllocator));
    if (!allocator) return NULL;

    // Optimize alignment based on actual cache line size
    size_t cache_line_size = get_cache_line_size();
    allocator->object_size = (object_size + cache_line_size - 1) & ~(cache_line_size - 1);

    // Optimize slab size for better memory utilization
    size_t optimal_slab_size = 4096;
#ifdef HAS_TRANSPARENT_HUGEPAGES
    // Use 2MB huge pages on Linux if available
    optimal_slab_size = 2 * 1024 * 1024;
#elif defined(HAS_LARGE_PAGES)
    // Use large pages on Windows if available
    optimal_slab_size = 2 * 1024 * 1024;
#endif

    allocator->objects_per_slab = optimal_slab_size / allocator->object_size;
    if (allocator->objects_per_slab < 1) allocator->objects_per_slab = 1;

    // Calculate how many slabs we need for initial_objects
    size_t slabs_needed = (initial_objects + allocator->objects_per_slab - 1) / allocator->objects_per_slab;
    if (slabs_needed < 1) slabs_needed = 1;

    size_t slab_size = allocator->object_size * allocator->objects_per_slab * slabs_needed;
    allocator->use_huge_pages = false;
    
#ifdef __unix__
    // Try to use huge pages for better TLB performance
    allocator->memory = mmap(NULL, slab_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (allocator->memory != MAP_FAILED) {
        allocator->use_huge_pages = true;
    } else {
        // Fallback to regular allocation
        allocator->memory = aligned_alloc(CACHE_LINE_SIZE, slab_size);
    }
#else
    // Windows/other platforms
    allocator->memory = aligned_alloc(CACHE_LINE_SIZE, slab_size);
#endif

    if (!allocator->memory) {
        free(allocator);
        return NULL;
    }

    // Initialize free list using pointer arithmetic
    allocator->free_list = allocator->memory;
    for (size_t i = 0; i < allocator->objects_per_slab - 1; i++) {
        void** current = (void**)((char*)allocator->memory + i * allocator->object_size);
        *current = (char*)allocator->memory + (i + 1) * allocator->object_size;
    }
    
    // Last object points to NULL
    void** last = (void**)((char*)allocator->memory + 
                          (allocator->objects_per_slab - 1) * allocator->object_size);
    *last = NULL;

    allocator->total_slabs = 1;
    allocator->allocated_objects = 0;

    return allocator;
}

// Lock-free allocation using atomic operations
void* slab_alloc(SlabAllocator* allocator) {
    if (!allocator) return malloc(64); // Fallback
    
    void* old_head;
    void* new_head;
    
    do {
        old_head = __atomic_load_n(&allocator->free_list, __ATOMIC_ACQUIRE);
        if (!old_head) {
            // Pool exhausted, fallback to malloc
            return malloc(allocator->object_size);
        }
        
        new_head = *(void**)old_head;
    } while (!__atomic_compare_exchange_n(&allocator->free_list, &old_head, new_head,
                                         false, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
    
    __atomic_fetch_add(&allocator->allocated_objects, 1, __ATOMIC_RELAXED);
    return old_head;
}

void slab_free(SlabAllocator* allocator, void* ptr) {
    if (!allocator || !ptr) {
        free(ptr);
        return;
    }
    
    // Check if pointer is from our pool
    char* pool_start = (char*)allocator->memory;
    char* pool_end = pool_start + (allocator->object_size * allocator->objects_per_slab);
    
    if (ptr < (void*)pool_start || ptr >= (void*)pool_end) {
        // Not from our pool, use regular free
        free(ptr);
        return;
    }
    
    // Add back to free list atomically
    void* old_head;
    do {
        old_head = __atomic_load_n(&allocator->free_list, __ATOMIC_ACQUIRE);
        *(void**)ptr = old_head;
    } while (!__atomic_compare_exchange_n(&allocator->free_list, &old_head, ptr,
                                         false, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
    
    __atomic_fetch_sub(&allocator->allocated_objects, 1, __ATOMIC_RELAXED);
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
#else
    free(allocator->memory);
#endif
    
    free(allocator);
}

void init_global_pools(void) {
    // Prevent double initialization
    if (g_cjson_node_pool != NULL) {
        return;
    }

    // Initialize pools for common object sizes
    g_cjson_node_pool = slab_allocator_create(256, 1000);      // cJSON nodes
    g_property_node_pool = slab_allocator_create(128, 500);    // Property nodes
    g_task_pool = slab_allocator_create(64, 200);              // Task objects
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
