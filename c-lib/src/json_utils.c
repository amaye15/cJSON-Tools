#include "../include/json_utils.h"
#include "../include/simd_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simplified Windows implementation - avoid complex header conflicts
#ifdef _WIN32
    #include <process.h>

    // Simple Windows implementation without advanced features
    static long msvc_sysconf(int name) {
        (void)name; // Suppress unused parameter warning
        return 4; // Default to 4 cores for Windows
    }

    #define sysconf(x) msvc_sysconf(x)
    #define _SC_NPROCESSORS_ONLN 1
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #ifdef __unix__
    #include <sys/mman.h>
    #endif
#endif



// Disable SIMD on Windows builds for initial PyPI release
#ifdef THREADING_DISABLED
#undef __SSE2__
#undef __AVX2__
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifdef __SSE2__
/**
 * SIMD-optimized string duplication for better performance
 */
static char* my_strdup_simd(const char* str) {
    if (UNLIKELY(str == NULL)) return NULL;

    size_t len = strlen_simd(str);
    // Align allocation to 16 bytes for SIMD operations
    size_t aligned_size = (len + 16) & ~15;
    char* new_str;

    // Use posix_memalign for better compatibility
    if (posix_memalign((void**)&new_str, 16, aligned_size) != 0) {
        return NULL;
    }

    if (UNLIKELY(new_str == NULL)) return NULL;

    // Copy 16 bytes at a time using SIMD
    size_t simd_len = len & ~15;
    for (size_t i = 0; i < simd_len; i += 16) {
        __m128i chunk = _mm_loadu_si128((__m128i*)(str + i));
        _mm_store_si128((__m128i*)(new_str + i), chunk);
    }

    // Copy remaining bytes
    memcpy(new_str + simd_len, str + simd_len, len - simd_len + 1);
    return new_str;
}
#endif

/**
 * Optimized string duplication function with branch prediction
 */
char* my_strdup(const char* str) {
    if (UNLIKELY(str == NULL)) return NULL;

    size_t len = strlen_simd(str) + 1;

#ifdef __SSE2__
    // Use SIMD for longer strings
    if (len > 32) {
        return my_strdup_simd(str);
    }
#endif

    char* new_str = (char*)malloc(len);
    if (UNLIKELY(new_str == NULL)) return NULL;

    return (char*)memcpy(new_str, str, len);
}

/**
 * Memory-mapped file reading for large files
 */
static char* read_json_file_mmap(const char* filename) {
#ifdef __unix__
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return NULL;

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return NULL;
    }

    // Use mmap for files larger than 64KB
    if (st.st_size > 65536) {
        void* mapped = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        if (mapped == MAP_FAILED) return NULL;

        // Copy to heap memory and unmap
        char* buffer = malloc(st.st_size + 1);
        if (buffer) {
            memcpy(buffer, mapped, st.st_size);
            buffer[st.st_size] = '\0';
        }
        munmap(mapped, st.st_size);

        return buffer;
    }

    close(fd);
#elif defined(_WIN32)
    // Disable memory mapping on Windows for now to avoid header conflicts
    (void)filename; // Suppress unused parameter warning
#endif
    return NULL; // Fall back to regular read
}

/**
 * Optimized JSON file reader with better error handling
 */
char* read_json_file(const char* filename) {
    // Try memory mapping for large files first
    char* result = read_json_file_mmap(filename);
    if (result) return result;

    // Fall back to standard FILE* operations for all platforms
    FILE* file = fopen(filename, "rb"); // Use binary mode for better performance
    if (UNLIKELY(!file)) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }

    // Get file size
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

    // Allocate buffer with extra space for null terminator
    char* buffer = (char*)malloc(file_size + 1);
    if (UNLIKELY(!buffer)) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    // Read file into buffer
    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';
    fclose(file);

    return buffer;
}

/**
 * Reads JSON from stdin into a string
 */
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

/**
 * Determines the number of CPU cores available
 */
int get_num_cores(void) {
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    return (num_cores > 0) ? (int)num_cores : 1;
}

/**
 * Optimized thread count determination with better scaling
 */
int get_optimal_threads(int requested_threads) {
    if (LIKELY(requested_threads > 0)) {
        // Cap at reasonable maximum to prevent resource exhaustion
        return requested_threads > 64 ? 64 : requested_threads;
    }

    int num_cores = get_num_cores();

    // Improved heuristic for better performance scaling:
    // - For 1-2 cores: use all cores
    // - For 3-4 cores: use cores-1 (leave one for system)
    // - For 5-8 cores: use 75% of cores
    // - For >8 cores: use 50% + 2 (better for large systems)

    if (num_cores <= 2) {
        return num_cores;
    } else if (num_cores <= 4) {
        return num_cores - 1;
    } else if (num_cores <= 8) {
        return (num_cores * 3) / 4; // 75% of cores
    } else {
        return (num_cores / 2) + 2;
    }
}