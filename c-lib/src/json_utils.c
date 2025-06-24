#include "../include/json_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Optimized string duplication function with branch prediction
 */
char* my_strdup(const char* str) {
    if (__builtin_expect(str == NULL, 0)) return NULL;

    size_t len = strlen(str) + 1;
    char* new_str = (char*)malloc(len);

    if (__builtin_expect(new_str == NULL, 0)) return NULL;

    return (char*)memcpy(new_str, str, len);
}

/**
 * Optimized JSON file reader with better error handling
 */
char* read_json_file(const char* filename) {
    FILE* file = fopen(filename, "rb"); // Use binary mode for better performance
    if (__builtin_expect(!file, 0)) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }

    // Get file size
    if (__builtin_expect(fseek(file, 0, SEEK_END) != 0, 0)) {
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (__builtin_expect(file_size < 0, 0)) {
        fclose(file);
        return NULL;
    }

    if (__builtin_expect(fseek(file, 0, SEEK_SET) != 0, 0)) {
        fclose(file);
        return NULL;
    }

    // Allocate buffer with extra space for null terminator
    char* buffer = (char*)malloc(file_size + 1);
    if (__builtin_expect(!buffer, 0)) {
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
        size_t buffer_len = strlen(buffer);
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
    if (__builtin_expect(requested_threads > 0, 1)) {
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