#ifndef JSON_TOOLS_BUILDER_H
#define JSON_TOOLS_BUILDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "regex_engine.h"
#include "cjson/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct JsonToolsBuilder JsonToolsBuilder;

// Operation types for the builder (using bit flags for fast checking)
typedef enum {
    OP_REMOVE_EMPTY_STRINGS = 1,
    OP_REMOVE_NULLS = 2,
    OP_REPLACE_KEYS = 4,
    OP_REPLACE_VALUES = 8,
    OP_FLATTEN = 16
} OperationType;

// Structure to hold operation parameters with performance optimizations
typedef struct {
    OperationType type;
    char* pattern;              // For regex operations
    char* replacement;          // For regex operations
    regex_engine_t* compiled_regex;  // High-performance compiled regex
    bool regex_valid;           // Whether regex compilation succeeded
    size_t pattern_len;         // Cached pattern length
    size_t replacement_len;     // Cached replacement length
} BuilderOperation;

// Main builder structure with performance optimizations
struct JsonToolsBuilder {
    cJSON* json_data;                    // The JSON data being processed
    BuilderOperation* operations;        // Array of operations to perform
    size_t operation_count;             // Number of operations queued
    size_t operation_capacity;          // Capacity of operations array
    bool pretty_print;                  // Whether to pretty print output
    char* error_message;                // Last error message

    // Performance optimization fields
    uint32_t operation_mask;            // Bitmask of operation types for fast checking
    bool has_regex_operations;          // Whether any regex operations are present
    size_t estimated_string_pool_size;  // Estimated size for string pool allocation
};

// Builder lifecycle functions
JsonToolsBuilder* json_tools_builder_create(void);
void json_tools_builder_destroy(JsonToolsBuilder* builder);

// JSON input
JsonToolsBuilder* json_tools_builder_add_json(JsonToolsBuilder* builder, const char* json_string);

// Filtering operations
JsonToolsBuilder* json_tools_builder_remove_empty_strings(JsonToolsBuilder* builder);
JsonToolsBuilder* json_tools_builder_remove_nulls(JsonToolsBuilder* builder);

// Regex replacement operations
JsonToolsBuilder* json_tools_builder_replace_keys(JsonToolsBuilder* builder, const char* pattern, const char* replacement);
JsonToolsBuilder* json_tools_builder_replace_values(JsonToolsBuilder* builder, const char* pattern, const char* replacement);

// Transformation operations
JsonToolsBuilder* json_tools_builder_flatten(JsonToolsBuilder* builder);

// Output configuration
JsonToolsBuilder* json_tools_builder_pretty_print(JsonToolsBuilder* builder, bool enable);

// Build and execute
char* json_tools_builder_build(JsonToolsBuilder* builder);

// Error handling
const char* json_tools_builder_get_error(JsonToolsBuilder* builder);
bool json_tools_builder_has_error(JsonToolsBuilder* builder);

// Internal helper functions (non-static for Python bindings)
int add_operation(JsonToolsBuilder* builder, OperationType type, const char* pattern, const char* replacement);
void clear_operations(JsonToolsBuilder* builder);
char* execute_operations(JsonToolsBuilder* builder);

// Single-pass JSON processing function
cJSON* process_json_single_pass(cJSON* json, BuilderOperation* operations, size_t operation_count);
void process_json_node_recursive(cJSON* node, BuilderOperation* operations, size_t operation_count);

// Optimized processing functions
void process_json_node_recursive_fast(cJSON* node, BuilderOperation* operations, size_t operation_count, uint32_t operation_mask);
bool should_remove_empty_string_fast(cJSON* item, uint32_t operation_mask);
bool should_remove_null_fast(cJSON* item, uint32_t operation_mask);

// Operation-specific processors (legacy)
bool should_remove_empty_string(cJSON* item, BuilderOperation* operations, size_t operation_count);
bool should_remove_null(cJSON* item, BuilderOperation* operations, size_t operation_count);
char* apply_key_replacements(const char* key, BuilderOperation* operations, size_t operation_count);
char* apply_value_replacements(const char* value, BuilderOperation* operations, size_t operation_count);

#ifdef __cplusplus
}
#endif

#endif // JSON_TOOLS_BUILDER_H
