#ifndef JSON_TOOLS_BUILDER_H
#define JSON_TOOLS_BUILDER_H

#include <stdbool.h>
#include <stddef.h>
#include "cjson/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct JsonToolsBuilder JsonToolsBuilder;

// Operation types for the builder
typedef enum {
    OP_REMOVE_EMPTY_STRINGS,
    OP_REMOVE_NULLS,
    OP_REPLACE_KEYS,
    OP_REPLACE_VALUES,
    OP_FLATTEN
} OperationType;

// Structure to hold operation parameters
typedef struct {
    OperationType type;
    char* pattern;      // For regex operations
    char* replacement;  // For regex operations
} BuilderOperation;

// Main builder structure
struct JsonToolsBuilder {
    cJSON* json_data;                    // The JSON data being processed
    BuilderOperation* operations;        // Array of operations to perform
    size_t operation_count;             // Number of operations queued
    size_t operation_capacity;          // Capacity of operations array
    bool pretty_print;                  // Whether to pretty print output
    char* error_message;                // Last error message
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

// Operation-specific processors
bool should_remove_empty_string(cJSON* item, BuilderOperation* operations, size_t operation_count);
bool should_remove_null(cJSON* item, BuilderOperation* operations, size_t operation_count);
char* apply_key_replacements(const char* key, BuilderOperation* operations, size_t operation_count);
char* apply_value_replacements(const char* value, BuilderOperation* operations, size_t operation_count);

#ifdef __cplusplus
}
#endif

#endif // JSON_TOOLS_BUILDER_H
