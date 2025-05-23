#include "../include/thread_pool.h"
#include "../include/json_utils.h"
#include <stdlib.h>
#include <stdio.h>

// Platform-specific threading functions
#ifdef _WIN32
    // Windows implementation

    // Initialize mutex
    static void mutex_init(mutex_t* mutex) {
        InitializeCriticalSection(mutex);
    }

    // Lock mutex
    static void mutex_lock(mutex_t* mutex) {
        EnterCriticalSection(mutex);
    }

    // Unlock mutex
    static void mutex_unlock(mutex_t* mutex) {
        LeaveCriticalSection(mutex);
    }

    // Destroy mutex
    static void mutex_destroy(mutex_t* mutex) {
        DeleteCriticalSection(mutex);
    }

    // Initialize condition variable
    static void cond_init(cond_t* cond) {
        InitializeConditionVariable(cond);
    }

    // Wait on condition variable
    static void cond_wait(cond_t* cond, mutex_t* mutex) {
        SleepConditionVariableCS(cond, mutex, INFINITE);
    }

    // Signal condition variable
    static void cond_signal(cond_t* cond) {
        WakeConditionVariable(cond);
    }

    // Broadcast condition variable
    static void cond_broadcast(cond_t* cond) {
        WakeAllConditionVariable(cond);
    }

    // Destroy condition variable (no-op on Windows)
    static void cond_destroy(cond_t* cond) {
        // Windows condition variables don't need to be destroyed
        (void)cond;
    }

    // Create thread
    static int thread_create(thread_t* thread, void* (*start_routine)(void*), void* arg) {
        *thread = (HANDLE)_beginthreadex(NULL, 0, (unsigned int (__stdcall *)(void*))start_routine, arg, 0, NULL);
        return (*thread == NULL) ? -1 : 0;
    }

    // Join thread
    static int thread_join(thread_t thread) {
        DWORD result = WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        return (result == WAIT_OBJECT_0) ? 0 : -1;
    }

#else
    // POSIX implementation

    // Initialize mutex
    static void mutex_init(mutex_t* mutex) {
        pthread_mutex_init(mutex, NULL);
    }

    // Lock mutex
    static void mutex_lock(mutex_t* mutex) {
        pthread_mutex_lock(mutex);
    }

    // Unlock mutex
    static void mutex_unlock(mutex_t* mutex) {
        pthread_mutex_unlock(mutex);
    }

    // Destroy mutex
    static void mutex_destroy(mutex_t* mutex) {
        pthread_mutex_destroy(mutex);
    }

    // Initialize condition variable
    static void cond_init(cond_t* cond) {
        pthread_cond_init(cond, NULL);
    }

    // Wait on condition variable
    static void cond_wait(cond_t* cond, mutex_t* mutex) {
        pthread_cond_wait(cond, mutex);
    }

    // Signal condition variable
    static void cond_signal(cond_t* cond) {
        pthread_cond_signal(cond);
    }

    // Broadcast condition variable
    static void cond_broadcast(cond_t* cond) {
        pthread_cond_broadcast(cond);
    }

    // Destroy condition variable
    static void cond_destroy(cond_t* cond) {
        pthread_cond_destroy(cond);
    }

    // Create thread
    static int thread_create(thread_t* thread, void* (*start_routine)(void*), void* arg) {
        return pthread_create(thread, NULL, start_routine, arg);
    }

    // Join thread
    static int thread_join(thread_t thread) {
        return pthread_join(thread, NULL);
    }
#endif

// Worker thread function
static THREAD_RETURN_TYPE worker_thread(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    Task* task;
    
    while (1) {
        // Lock the queue mutex
        mutex_lock(&pool->queue_mutex);
        
        // Wait for a task or shutdown signal
        while (pool->task_queue == NULL && !pool->shutdown) {
            cond_wait(&pool->queue_cond, &pool->queue_mutex);
        }
        
        // Check for shutdown
        if (pool->shutdown && pool->task_queue == NULL) {
            mutex_unlock(&pool->queue_mutex);
            return THREAD_RETURN_VALUE;
        }
        
        // Get a task from the queue
        task = pool->task_queue;
        if (task != NULL) {
            pool->task_queue = task->next;
            if (pool->task_queue == NULL) {
                pool->task_queue_tail = NULL;
            }
            pool->active_threads++;
        }
        
        mutex_unlock(&pool->queue_mutex);
        
        // Execute the task
        if (task != NULL) {
            task->function(task->argument);
            free(task);
            
            // Mark thread as idle
            mutex_lock(&pool->queue_mutex);
            pool->active_threads--;
            if (pool->active_threads == 0) {
                cond_signal(&pool->idle_cond);
            }
            mutex_unlock(&pool->queue_mutex);
        }
    }
    
    return THREAD_RETURN_VALUE;
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
    mutex_init(&pool->queue_mutex);
    cond_init(&pool->queue_cond);
    cond_init(&pool->idle_cond);
    
    // Allocate and create threads
    pool->threads = (thread_t*)malloc(pool->num_threads * sizeof(thread_t));
    if (pool->threads == NULL) {
        free(pool);
        return NULL;
    }
    
    for (int i = 0; i < pool->num_threads; i++) {
        if (thread_create(&pool->threads[i], worker_thread, pool) != 0) {
            // Failed to create thread, clean up and return NULL
            thread_pool_destroy(pool);
            return NULL;
        }
    }
    
    return pool;
}

// Add a task to the thread pool
int thread_pool_add_task(ThreadPool* pool, void (*function)(void*), void* argument) {
    if (pool == NULL || function == NULL) {
        return -1;
    }
    
    // Create a new task
    Task* task = (Task*)malloc(sizeof(Task));
    if (task == NULL) {
        return -1;
    }
    
    task->function = function;
    task->argument = argument;
    task->next = NULL;
    
    // Add the task to the queue
    mutex_lock(&pool->queue_mutex);
    
    if (pool->task_queue == NULL) {
        pool->task_queue = task;
        pool->task_queue_tail = task;
    } else {
        pool->task_queue_tail->next = task;
        pool->task_queue_tail = task;
    }
    
    // Signal a waiting thread
    cond_signal(&pool->queue_cond);
    
    mutex_unlock(&pool->queue_mutex);
    
    return 0;
}

// Wait for all tasks to complete
void thread_pool_wait(ThreadPool* pool) {
    if (pool == NULL) {
        return;
    }
    
    mutex_lock(&pool->queue_mutex);
    
    // Wait until all tasks are completed
    while (pool->task_queue != NULL || pool->active_threads > 0) {
        cond_wait(&pool->idle_cond, &pool->queue_mutex);
    }
    
    mutex_unlock(&pool->queue_mutex);
}

// Destroy the thread pool
void thread_pool_destroy(ThreadPool* pool) {
    if (pool == NULL) {
        return;
    }
    
    // Set shutdown flag
    mutex_lock(&pool->queue_mutex);
    pool->shutdown = true;
    cond_broadcast(&pool->queue_cond);
    mutex_unlock(&pool->queue_mutex);
    
    // Wait for threads to exit
    for (int i = 0; i < pool->num_threads; i++) {
        thread_join(pool->threads[i]);
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
    mutex_destroy(&pool->queue_mutex);
    cond_destroy(&pool->queue_cond);
    cond_destroy(&pool->idle_cond);
    
    free(pool);
}

// Get the number of threads in the pool
int thread_pool_get_thread_count(ThreadPool* pool) {
    if (pool == NULL) {
        return 0;
    }
    return pool->num_threads;
}