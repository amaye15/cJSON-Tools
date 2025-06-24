#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Cache line size for alignment
#define CACHE_LINE_SIZE 64
#define ALIGN_TO_CACHE(size) (((size) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1))

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

// Function declarations
SlabAllocator* slab_allocator_create(size_t object_size, size_t initial_objects);
void* slab_alloc(SlabAllocator* allocator);
void slab_free(SlabAllocator* allocator, void* ptr);
void slab_allocator_destroy(SlabAllocator* allocator);

// Global object pools for common allocations
extern SlabAllocator* g_cjson_node_pool;
extern SlabAllocator* g_property_node_pool;
extern SlabAllocator* g_task_pool;

// Convenience macros
#define POOL_ALLOC(pool) slab_alloc(pool)
#define POOL_FREE(pool, ptr) slab_free(pool, ptr)

// Initialize global pools
void init_global_pools(void);
void cleanup_global_pools(void);

#endif /* MEMORY_POOL_H */
