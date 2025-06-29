#include "../include/thread_pool.h"
#include "../include/json_utils.h"
#include <stdlib.h>
#include <stdio.h>

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#ifndef THREADING_DISABLED
#include <pthread.h>
#include <unistd.h>
#endif

#if defined(THREADING_DISABLED) && defined(__WINDOWS__) && !defined(__MINGW32__) && !defined(__MINGW64__)
// Native Windows (MSVC) implementation - threading disabled for initial release
int pthread_create(pthread_t* thread, void* attr, void* (*start_routine)(void*), void* arg) {
    (void)thread; (void)attr; (void)start_routine; (void)arg;
    // Execute synchronously on Windows for now
    start_routine(arg);
    return 0;
}

int pthread_join(pthread_t thread, void** retval) {
    (void)thread; (void)retval;
    return 0; // No-op on Windows
}

void pthread_exit(void* retval) {
    (void)retval;
    // No-op on Windows
}

int pthread_mutex_init(pthread_mutex_t* mutex, void* attr) {
    (void)mutex; (void)attr;
    return 0; // No-op on Windows
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0; // No-op on Windows
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0; // No-op on Windows
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0; // No-op on Windows
}

int pthread_cond_init(pthread_cond_t* cond, void* attr) {
    (void)cond; (void)attr;
    return 0; // No-op on Windows
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    (void)cond; (void)mutex;
    return 0; // No-op on Windows
}

int pthread_cond_signal(pthread_cond_t* cond) {
    (void)cond;
    return 0; // No-op on Windows
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    (void)cond;
    return 0; // No-op on Windows
}

int pthread_cond_destroy(pthread_cond_t* cond) {
    (void)cond;
    return 0; // No-op on Windows
}
#endif

// Get number of CPU cores for thread affinity (internal implementation)
static int get_num_cores_internal(void) {
    static int num_cores = 0;
    if (num_cores == 0) {
#ifdef __linux__
        num_cores = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(__WINDOWS__) && defined(HAS_LARGE_PAGES)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        num_cores = sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__)
        size_t size = sizeof(num_cores);
        sysctlbyname("hw.ncpu", &num_cores, &size, NULL, 0);
#else
        num_cores = 4; // Default fallback
#endif
        if (num_cores <= 0) num_cores = 4;
    }
    return num_cores;
}

// Optimized worker thread function with reduced lock contention
static void* worker_thread(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    Task* task;

    while (1) {
        // Lock the queue mutex
        pthread_mutex_lock(&pool->queue_mutex);

        // Wait for a task or shutdown signal
        while (__builtin_expect(pool->task_queue == NULL && !pool->shutdown, 0)) {
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
        }

        // Check for shutdown
        if (__builtin_expect(pool->shutdown && pool->task_queue == NULL, 0)) {
            pthread_mutex_unlock(&pool->queue_mutex);
            pthread_exit(NULL);
        }

        // Get a task from the queue
        task = pool->task_queue;
        if (__builtin_expect(task != NULL, 1)) {
            pool->task_queue = task->next;
            if (__builtin_expect(pool->task_queue == NULL, 0)) {
                pool->task_queue_tail = NULL;
            }
            pool->active_threads++;
        }

        pthread_mutex_unlock(&pool->queue_mutex);

        // Execute the task outside of lock
        if (__builtin_expect(task != NULL, 1)) {
            task->function(task->argument);
            free(task);

            // Mark thread as idle with minimal lock time
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

// Create a thread pool
ThreadPool* thread_pool_create(int num_threads) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (pool == NULL) {
        return NULL;
    }
    
    // Initialize pool properties
    pool->task_queue = NULL;
    pool->task_queue_tail = NULL;
    pool->shutdown = false;
    pool->active_threads = 0;
    
    // Determine number of threads
    pool->num_threads = get_optimal_threads(num_threads);
    
    // Initialize mutex and condition variables
    pthread_mutex_init(&pool->queue_mutex, NULL);
    pthread_cond_init(&pool->queue_cond, NULL);
    pthread_cond_init(&pool->idle_cond, NULL);
    
    // Allocate and create threads
    pool->threads = (pthread_t*)malloc(pool->num_threads * sizeof(pthread_t));
    if (pool->threads == NULL) {
        free(pool);
        return NULL;
    }
    
    for (int i = 0; i < pool->num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            // Failed to create thread, clean up and return NULL
            thread_pool_destroy(pool);
            return NULL;
        }
    }
    
    return pool;
}

// Optimized task addition with reduced lock contention
int thread_pool_add_task(ThreadPool* pool, void (*function)(void*), void* argument) {
    if (__builtin_expect(pool == NULL || function == NULL, 0)) {
        return -1;
    }

    // Create a new task
    Task* task = (Task*)malloc(sizeof(Task));
    if (__builtin_expect(task == NULL, 0)) {
        return -1;
    }

    task->function = function;
    task->argument = argument;
    task->next = NULL;

    // Add the task to the queue with minimal lock time
    pthread_mutex_lock(&pool->queue_mutex);

    if (__builtin_expect(pool->task_queue == NULL, 0)) {
        pool->task_queue = task;
        pool->task_queue_tail = task;
    } else {
        pool->task_queue_tail->next = task;
        pool->task_queue_tail = task;
    }

    // Signal a waiting thread (broadcast for better responsiveness)
    pthread_cond_signal(&pool->queue_cond);

    pthread_mutex_unlock(&pool->queue_mutex);

    return 0;
}

// Wait for all tasks to complete
void thread_pool_wait(ThreadPool* pool) {
    if (pool == NULL) {
        return;
    }
    
    pthread_mutex_lock(&pool->queue_mutex);
    
    // Wait until all tasks are completed
    while (pool->task_queue != NULL || pool->active_threads > 0) {
        pthread_cond_wait(&pool->idle_cond, &pool->queue_mutex);
    }
    
    pthread_mutex_unlock(&pool->queue_mutex);
}

// Destroy the thread pool
void thread_pool_destroy(ThreadPool* pool) {
    if (pool == NULL) {
        return;
    }
    
    // Set shutdown flag
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    // Wait for threads to exit
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // Free memory
    free(pool->threads);
    
    // Clean up tasks
    Task* task = pool->task_queue;
    while (task != NULL) {
        Task* next = task->next;
        free(task);
        task = next;
    }
    
    // Destroy mutex and condition variables
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    pthread_cond_destroy(&pool->idle_cond);
    
    free(pool);
}

// Get the number of threads in the pool
int thread_pool_get_thread_count(ThreadPool* pool) {
    if (pool == NULL) {
        return 0;
    }
    return pool->num_threads;
}

// Get approximate queue size for monitoring and load balancing
size_t thread_pool_get_queue_size(ThreadPool* pool) {
    if (pool == NULL) {
        return 0;
    }

    // For local thread pools, count tasks in the queue
    size_t count = 0;
    pthread_mutex_lock(&pool->queue_mutex);
    Task* current = pool->task_queue;
    while (current != NULL && count < 1000) {  // Limit to avoid infinite loops
        current = current->next;
        count++;
    }
    pthread_mutex_unlock(&pool->queue_mutex);

    // Also include global lock-free queue size
    return count + get_task_queue_size();
}