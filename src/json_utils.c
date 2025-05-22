#include "../include/json_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Custom string duplication function (replacement for strdup)
 */
char* my_strdup(const char* str) {
    if (str == NULL) return NULL;
    
    size_t len = strlen(str) + 1;
    char* new_str = (char*)malloc(len);
    
    if (new_str == NULL) return NULL;
    
    return (char*)memcpy(new_str, str, len);
}

/**
 * Reads a JSON file into a string
 */
char* read_json_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate buffer
    char* buffer = (char*)malloc(file_size + 1);
    if (!buffer) {
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
 * Determines the optimal number of threads to use
 */
int get_optimal_threads(int requested_threads) {
    if (requested_threads > 0) {
        return requested_threads;
    }
    
    int num_cores = get_num_cores();
    // Use number of cores, but cap at 4 to avoid excessive threading overhead
    return (num_cores > 4) ? 4 : num_cores;
}