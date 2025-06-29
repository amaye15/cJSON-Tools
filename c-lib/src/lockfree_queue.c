#include "../include/compiler_hints.h"
#include <stdlib.h>
#include <stdint.h>

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

// Windows-compatible atomic operations
#ifdef __WINDOWS__
    #include <windows.h>
    #include <intrin.h>
    
    // Define atomic types for Windows
    typedef volatile LONG64 atomic_uintptr_t;
    
    // Define atomic operations for Windows
    #define atomic_store(ptr, val) InterlockedExchange64((volatile LONG64*)(ptr), (LONG64)(val))
    #define atomic_load(ptr) InterlockedOr64((volatile LONG64*)(ptr), 0)
    #define atomic_compare_exchange_weak(ptr, expected, desired) \
        (InterlockedCompareExchange64((volatile LONG64*)(ptr), (LONG64)(desired), (LONG64)*(expected)) == (LONG64)*(expected))
    
    // Memory barriers
    #define MEMORY_BARRIER() MemoryBarrier()
    
#else
    // Unix-like systems with C11 atomics
    #include <stdatomic.h>
    #define MEMORY_BARRIER() __sync_synchronize()
#endif


// Michael & Scott lock-free queue for better threading performance
typedef struct QueueNode {
    atomic_uintptr_t data;
    atomic_uintptr_t next;
    char padding[CACHE_LINE_SIZE - 2 * sizeof(atomic_uintptr_t)];  // Avoid false sharing
} CACHE_ALIGNED QueueNode;

typedef struct {
    atomic_uintptr_t head;
    atomic_uintptr_t tail;
    char padding[CACHE_LINE_SIZE - 2 * sizeof(atomic_uintptr_t)];  // Avoid false sharing
} CACHE_ALIGNED LockFreeQueue;

// Initialize lock-free queue
HOT_PATH static void lf_queue_init(LockFreeQueue* queue) {
    QueueNode* dummy = malloc(sizeof(QueueNode));
    atomic_store(&dummy->data, 0);
    atomic_store(&dummy->next, 0);
    
    atomic_store(&queue->head, (uintptr_t)dummy);
    atomic_store(&queue->tail, (uintptr_t)dummy);
}

// Enqueue operation (lock-free)
HOT_PATH static void lf_queue_enqueue(LockFreeQueue* queue, void* data) {
    QueueNode* node = malloc(sizeof(QueueNode));
    atomic_store(&node->data, (uintptr_t)data);
    atomic_store(&node->next, 0);
    
    while (1) {
        QueueNode* tail = (QueueNode*)atomic_load(&queue->tail);
        QueueNode* next = (QueueNode*)atomic_load(&tail->next);
        
        // Check if tail is still the same
        if (LIKELY(tail == (QueueNode*)atomic_load(&queue->tail))) {
            if (next == NULL) {
                // Try to link the new node
                uintptr_t expected = 0;
                if (atomic_compare_exchange_weak(&tail->next, &expected, (uintptr_t)node)) {
                    break;
                }
            } else {
                // Help advance tail
                uintptr_t expected_tail = (uintptr_t)tail;
                atomic_compare_exchange_weak(&queue->tail, &expected_tail, (uintptr_t)next);
            }
        }
        cpu_relax();  // Reduce contention
    }
    
    // Try to advance tail
    uintptr_t expected_tail = (uintptr_t)((QueueNode*)atomic_load(&queue->tail));
    atomic_compare_exchange_weak(&queue->tail, &expected_tail, (uintptr_t)node);
}

// Dequeue operation (lock-free)
HOT_PATH static void* lf_queue_dequeue(LockFreeQueue* queue) {
    while (1) {
        QueueNode* head = (QueueNode*)atomic_load(&queue->head);
        QueueNode* tail = (QueueNode*)atomic_load(&queue->tail);
        QueueNode* next = (QueueNode*)atomic_load(&head->next);
        
        // Check if head is still the same
        if (LIKELY(head == (QueueNode*)atomic_load(&queue->head))) {
            if (head == tail) {
                if (next == NULL) {
                    return NULL;  // Queue is empty
                }
                // Help advance tail
                uintptr_t expected_tail = (uintptr_t)tail;
                atomic_compare_exchange_weak(&queue->tail, &expected_tail, (uintptr_t)next);
            } else {
                if (next == NULL) {
                    continue;  // Inconsistent state, retry
                }
                
                // Read data before CAS
                void* data = (void*)atomic_load(&next->data);
                
                // Try to advance head
                uintptr_t expected_head = (uintptr_t)head;
                if (atomic_compare_exchange_weak(&queue->head, &expected_head, (uintptr_t)next)) {
                    free(head);  // Safe to free old head
                    return data;
                }
            }
        }
        cpu_relax();  // Reduce contention
    }
}

// Destroy lock-free queue
static void lf_queue_destroy(LockFreeQueue* queue) {
    // Drain the queue
    while (lf_queue_dequeue(queue) != NULL) {
        // Continue draining
    }
    
    // Free the dummy node
    QueueNode* head = (QueueNode*)atomic_load(&queue->head);
    free(head);
}

// Check if queue is empty (approximate)
static int lf_queue_is_empty(LockFreeQueue* queue) {
    QueueNode* head = (QueueNode*)atomic_load(&queue->head);
    QueueNode* tail = (QueueNode*)atomic_load(&queue->tail);
    return (head == tail) && (atomic_load(&head->next) == 0);
}

// Get approximate queue size (for monitoring)
size_t lf_queue_size_approx(LockFreeQueue* queue) {
    size_t count = 0;
    QueueNode* current = (QueueNode*)atomic_load(&queue->head);
    QueueNode* tail = (QueueNode*)atomic_load(&queue->tail);
    
    while (current != tail && count < 1000) {  // Limit to avoid infinite loops
        QueueNode* next = (QueueNode*)atomic_load(&current->next);
        if (next == NULL) break;
        current = next;
        count++;
    }
    
    return count;
}

// Global lock-free queue for task distribution
static LockFreeQueue g_task_queue;

#ifdef __WINDOWS__
    static volatile LONG g_queue_initialized = 0;
    #define atomic_int volatile LONG
    #define atomic_compare_exchange_strong(ptr, expected, desired) \
        (InterlockedCompareExchange((volatile LONG*)(ptr), (LONG)(desired), (LONG)*(expected)) == (LONG)*(expected))
    #define atomic_load_int(ptr) InterlockedOr((volatile LONG*)(ptr), 0)
    #define atomic_store_int(ptr, val) InterlockedExchange((volatile LONG*)(ptr), (LONG)(val))
#else
    static atomic_int g_queue_initialized = 0;
    #define atomic_load_int(ptr) atomic_load(ptr)
    #define atomic_store_int(ptr, val) atomic_store(ptr, val)
#endif

// Initialize global task queue
void init_lockfree_task_queue(void) {
#ifdef __WINDOWS__
    LONG expected = 0;
    if (InterlockedCompareExchange(&g_queue_initialized, 1, 0) == 0) {
        lf_queue_init(&g_task_queue);
    }
#else
    int expected = 0;
    if (atomic_compare_exchange_strong(&g_queue_initialized, &expected, 1)) {
        lf_queue_init(&g_task_queue);
    }
#endif
}

// Add task to global queue
void enqueue_task(void* task) {
    if (LIKELY(atomic_load_int(&g_queue_initialized))) {
        lf_queue_enqueue(&g_task_queue, task);
    }
}

// Get task from global queue
void* dequeue_task(void) {
    if (LIKELY(atomic_load_int(&g_queue_initialized))) {
        return lf_queue_dequeue(&g_task_queue);
    }
    return NULL;
}

// Check if task queue is empty
int is_task_queue_empty(void) {
    if (LIKELY(atomic_load_int(&g_queue_initialized))) {
        return lf_queue_is_empty(&g_task_queue);
    }
    return 1;
}

// Cleanup global task queue
void cleanup_lockfree_task_queue(void) {
    if (atomic_load_int(&g_queue_initialized)) {
        lf_queue_destroy(&g_task_queue);
        atomic_store_int(&g_queue_initialized, 0);
    }
}

// Public function to get task queue size for monitoring
size_t get_task_queue_size(void) {
    if (!atomic_load_int(&g_queue_initialized)) {
        return 0;
    }
    return lf_queue_size_approx(&g_task_queue);
}