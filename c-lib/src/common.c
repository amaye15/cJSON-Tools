#include "../include/common.h"
#include "../include/memory_pool.h"

// Global initialization state
static ATOMIC_INT g_initialized = 0;

/**
 * Initialize the cJSON-Tools library
 * 
 * This function should be called once before using any library functions.
 * It's thread-safe and can be called multiple times safely.
 * 
 * @return SUCCESS on success, error code on failure
 */
int cjson_tools_init(void) {
    // Check if already initialized
    if (ATOMIC_LOAD(&g_initialized)) {
        return SUCCESS;
    }

    // Initialize memory pools
    init_global_pools();

    // Mark as initialized
    ATOMIC_STORE(&g_initialized, 1);

    DEBUG_PRINT("cJSON-Tools library initialized successfully");
    return SUCCESS;
}

/**
 * Cleanup the cJSON-Tools library
 * 
 * This function should be called when the library is no longer needed.
 * It's thread-safe and can be called multiple times safely.
 */
void cjson_tools_cleanup(void) {
    // Check if not initialized
    if (!ATOMIC_LOAD(&g_initialized)) {
        return;
    }

    // Cleanup memory pools
    cleanup_global_pools();

    // Mark as not initialized
    ATOMIC_STORE(&g_initialized, 0);

    DEBUG_PRINT("cJSON-Tools library cleaned up successfully");
}
