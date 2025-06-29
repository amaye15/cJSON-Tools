#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stdbool.h>
#include <stddef.h>  // For size_t

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

// Cross-platform threading support
#if defined(__WINDOWS__) && !defined(__MINGW32__) && !defined(__MINGW64__)
    // Native Windows (MSVC) - threading disabled for initial PyPI release
    // Will be implemented in future version
    #ifndef THREADING_DISABLED
#define THREADING_DISABLED
#endif
    typedef int pthread_t;
    typedef int pthread_mutex_t;
    typedef int pthread_cond_t;
    #define PTHREAD_MUTEX_INITIALIZER 0
    #define PTHREAD_COND_INITIALIZER 0

    // Windows pthread function declarations (implemented as no-ops)
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
#elif defined(__MINGW32__) || defined(__MINGW64__)
    // MinGW has real pthread support, but disable threading for initial release
    #define THREADING_DISABLED
    #include <pthread.h>
#else
    // Unix-like systems with full pthread support
    #include <pthread.h>
#endif

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

/**
 * Creates a new thread pool
 * 
 * @param num_threads Number of threads in the pool (0 for auto)
 * @return A new thread pool, or NULL on failure
 */
ThreadPool* thread_pool_create(int num_threads);

/**
 * Adds a task to the thread pool
 * 
 * @param pool The thread pool
 * @param function The function to execute
 * @param argument The argument to pass to the function
 * @return 0 on success, -1 on failure
 */
int thread_pool_add_task(ThreadPool* pool, void (*function)(void*), void* argument);

/**
 * Waits for all tasks to complete and destroys the thread pool
 * 
 * @param pool The thread pool
 */
void thread_pool_destroy(ThreadPool* pool);

/**
 * Waits for all tasks to complete
 *
 * @param pool The thread pool
 */
void thread_pool_wait(ThreadPool* pool);

/**
 * Gets the approximate number of pending tasks in the queue
 * Useful for monitoring and load balancing
 *
 * @return Approximate number of tasks in queue
 */
size_t get_task_queue_size(void);

/**
 * Gets the approximate number of pending tasks in a thread pool
 * Includes both local queue and global lock-free queue
 *
 * @param pool The thread pool
 * @return Approximate number of tasks in queue
 */
size_t thread_pool_get_queue_size(ThreadPool* pool);

/**
 * Gets the number of threads in the pool
 * 
 * @param pool The thread pool
 * @return The number of threads
 */
int thread_pool_get_thread_count(ThreadPool* pool);

#endif /* THREAD_POOL_H */