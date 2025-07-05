#include "json_tools_builder.h"
#include "json_flattener.h"
#include "memory_pool.h"
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#define INITIAL_OPERATION_CAPACITY 16
#define STRING_POOL_INITIAL_SIZE 4096

// Disabled string pool optimization to prevent memory issues
// Performance optimization: String pool for reduced allocations
// static char* g_string_pool = NULL;
// static size_t g_string_pool_size = 0;
// static size_t g_string_pool_used = 0;

// Builder lifecycle functions
JsonToolsBuilder* json_tools_builder_create(void) {
    JsonToolsBuilder* builder = malloc(sizeof(JsonToolsBuilder));
    if (!builder) return NULL;

    builder->json_data = NULL;
    builder->operations = malloc(sizeof(BuilderOperation) * INITIAL_OPERATION_CAPACITY);
    if (!builder->operations) {
        free(builder);
        return NULL;
    }

    builder->operation_count = 0;
    builder->operation_capacity = INITIAL_OPERATION_CAPACITY;
    builder->pretty_print = false;
    builder->error_message = NULL;

    // Performance optimization fields
    builder->operation_mask = 0;
    builder->has_regex_operations = false;
    builder->estimated_string_pool_size = STRING_POOL_INITIAL_SIZE;

    return builder;
}

void json_tools_builder_destroy(JsonToolsBuilder* builder) {
    if (!builder) return;

    if (builder->json_data) {
        cJSON_Delete(builder->json_data);
    }

    clear_operations(builder);
    free(builder->operations);

    if (builder->error_message) {
        free(builder->error_message);
    }

    free(builder);
}

// JSON input
JsonToolsBuilder* json_tools_builder_add_json(JsonToolsBuilder* builder, const char* json_string) {
    if (!builder || !json_string) return builder;
    
    if (builder->json_data) {
        cJSON_Delete(builder->json_data);
    }
    
    builder->json_data = cJSON_Parse(json_string);
    if (!builder->json_data) {
        builder->error_message = strdup("Invalid JSON string");
        return builder;
    }
    
    return builder;
}

// Filtering operations
JsonToolsBuilder* json_tools_builder_remove_empty_strings(JsonToolsBuilder* builder) {
    if (!builder) return builder;
    add_operation(builder, OP_REMOVE_EMPTY_STRINGS, NULL, NULL);
    return builder;
}

JsonToolsBuilder* json_tools_builder_remove_nulls(JsonToolsBuilder* builder) {
    if (!builder) return builder;
    add_operation(builder, OP_REMOVE_NULLS, NULL, NULL);
    return builder;
}

// Regex replacement operations
JsonToolsBuilder* json_tools_builder_replace_keys(JsonToolsBuilder* builder, const char* pattern, const char* replacement) {
    if (!builder || !pattern || !replacement) return builder;
    add_operation(builder, OP_REPLACE_KEYS, pattern, replacement);
    return builder;
}

JsonToolsBuilder* json_tools_builder_replace_values(JsonToolsBuilder* builder, const char* pattern, const char* replacement) {
    if (!builder || !pattern || !replacement) return builder;
    add_operation(builder, OP_REPLACE_VALUES, pattern, replacement);
    return builder;
}

// Transformation operations
JsonToolsBuilder* json_tools_builder_flatten(JsonToolsBuilder* builder) {
    if (!builder) return builder;
    add_operation(builder, OP_FLATTEN, NULL, NULL);
    return builder;
}

// Output configuration
JsonToolsBuilder* json_tools_builder_pretty_print(JsonToolsBuilder* builder, bool enable) {
    if (!builder) return builder;
    builder->pretty_print = enable;
    return builder;
}

// Build and execute
char* json_tools_builder_build(JsonToolsBuilder* builder) {
    if (!builder || !builder->json_data) {
        if (builder && !builder->error_message) {
            builder->error_message = strdup("No JSON data provided");
        }
        return NULL;
    }
    
    return execute_operations(builder);
}

// Error handling
const char* json_tools_builder_get_error(JsonToolsBuilder* builder) {
    return builder ? builder->error_message : "Invalid builder";
}

bool json_tools_builder_has_error(JsonToolsBuilder* builder) {
    return builder && builder->error_message != NULL;
}

// Internal helper functions with performance optimizations
int add_operation(JsonToolsBuilder* builder, OperationType type, const char* pattern, const char* replacement) {
    if (!builder) return -1;

    // Resize operations array if needed
    if (builder->operation_count >= builder->operation_capacity) {
        size_t new_capacity = builder->operation_capacity * 2;
        BuilderOperation* new_operations = realloc(builder->operations, sizeof(BuilderOperation) * new_capacity);
        if (!new_operations) {
            builder->error_message = strdup("Memory allocation failed");
            return -1;
        }
        builder->operations = new_operations;
        builder->operation_capacity = new_capacity;
    }

    BuilderOperation* op = &builder->operations[builder->operation_count];
    op->type = type;
    op->pattern = pattern ? strdup(pattern) : NULL;
    op->replacement = replacement ? strdup(replacement) : NULL;
    op->compiled_regex = NULL;
    op->regex_valid = false;
    op->pattern_len = pattern ? strlen(pattern) : 0;
    op->replacement_len = replacement ? strlen(replacement) : 0;

    // Performance optimization: Update operation mask for fast checking
    builder->operation_mask |= type;

    // SAFER: Pre-compile regex patterns with comprehensive error checking
    if ((type == OP_REPLACE_KEYS || type == OP_REPLACE_VALUES) && pattern) {
        // Validate pattern is not empty and not too long
        size_t pattern_len = strlen(pattern);
        if (pattern_len == 0 || pattern_len > 1024) {
            op->regex_valid = false;
            op->compiled_regex = NULL;
        } else {
            op->compiled_regex = malloc(sizeof(regex_t));
            if (op->compiled_regex) {
                // Use safer regex compilation with REG_NOSUB for better performance and safety
                int regex_result = regcomp(op->compiled_regex, pattern, REG_EXTENDED | REG_NOSUB);
                if (regex_result == 0) {
                    op->regex_valid = true;
                    builder->has_regex_operations = true;
                } else {
                    // Clean up on regex compilation failure
                    regfree(op->compiled_regex);
                    free(op->compiled_regex);
                    op->compiled_regex = NULL;
                    op->regex_valid = false;
                }
            } else {
                op->regex_valid = false;
            }
        }
    } else {
        op->regex_valid = false;
        op->compiled_regex = NULL;
    }

    builder->operation_count++;
    return 0;
}

void clear_operations(JsonToolsBuilder* builder) {
    if (!builder || !builder->operations) return;

    for (size_t i = 0; i < builder->operation_count; i++) {
        if (builder->operations[i].pattern) {
            free(builder->operations[i].pattern);
        }
        if (builder->operations[i].replacement) {
            free(builder->operations[i].replacement);
        }
        // Clean up pre-compiled regex
        if (builder->operations[i].compiled_regex) {
            regfree(builder->operations[i].compiled_regex);
            free(builder->operations[i].compiled_regex);
        }
    }
    
    builder->operation_count = 0;
}

char* execute_operations(JsonToolsBuilder* builder) {
    if (!builder || !builder->json_data) return NULL;

    // Create a deep copy of the JSON data for processing
    cJSON* working_json = cJSON_Duplicate(builder->json_data, 1);
    if (!working_json) {
        builder->error_message = strdup("Failed to duplicate JSON data");
        return NULL;
    }

    // Process all operations in a single pass
    cJSON* result = process_json_single_pass(working_json, builder->operations, builder->operation_count);
    if (!result) {
        cJSON_Delete(working_json);
        builder->error_message = strdup("Failed to process operations");
        return NULL;
    }

    // Convert to string
    char* output;
    if (builder->pretty_print) {
        output = cJSON_Print(result);
    } else {
        output = cJSON_PrintUnformatted(result);
    }

    cJSON_Delete(result);
    return output;
}

// OPTIMIZED: Single-pass JSON processing function with performance improvements
cJSON* process_json_single_pass(cJSON* json, BuilderOperation* operations, size_t operation_count) {
    if (!json || !operations) return json;

    // OPTIMIZATION: Calculate operation mask once for fast checking
    uint32_t operation_mask = 0;
    bool should_flatten = false;

    for (size_t i = 0; i < operation_count; i++) {
        operation_mask |= operations[i].type;
        if (operations[i].type == OP_FLATTEN) {
            should_flatten = true;
        }
    }

    // OPTIMIZATION: Early exit if no operations to perform
    if (operation_mask == 0) return json;

    // Process the JSON recursively using optimized function
    process_json_node_recursive_fast(json, operations, operation_count, operation_mask);

    // Apply flattening if requested (must be done last)
    if (should_flatten) {
        char* json_str = cJSON_PrintUnformatted(json);
        if (json_str) {
            char* flattened = flatten_json_string(json_str, 0, 0);
            free(json_str);
            if (flattened) {
                cJSON_Delete(json);
                json = cJSON_Parse(flattened);
                free(flattened);
            }
        }
    }

    return json;
}

// OPTIMIZED: Fast recursive processing using operation mask and pre-compiled regex
void process_json_node_recursive_fast(cJSON* node, BuilderOperation* operations, size_t operation_count, uint32_t operation_mask) {
    if (!node) return;

    cJSON* child = node->child;
    cJSON* prev = NULL;

    while (child) {
        cJSON* next = child->next;  // Store next before potential deletion

        // OPTIMIZATION: Process the key if it exists and we have key replacement operations
        if (child->string && (operation_mask & OP_REPLACE_KEYS)) {
            char* new_key = apply_key_replacements(child->string, operations, operation_count);
            if (new_key && strcmp(new_key, child->string) != 0) {
                free(child->string);
                child->string = new_key;
            } else if (new_key) {
                free(new_key);
            }
        }

        // OPTIMIZATION: Fast removal checks using bitmask
        bool should_remove = false;

        // Fast empty string check
        if (should_remove_empty_string_fast(child, operation_mask)) {
            should_remove = true;
        }

        // Fast null check
        if (!should_remove && should_remove_null_fast(child, operation_mask)) {
            should_remove = true;
        }

        if (should_remove) {
            // Remove this child from the linked list
            if (prev) {
                prev->next = next;
            } else {
                node->child = next;
            }
            if (next) {
                next->prev = prev;
            }

            // Detach the child before deletion to avoid corrupting the list
            child->next = NULL;
            child->prev = NULL;
            cJSON_Delete(child);
        } else {
            // OPTIMIZATION: Process string values for replacement only if we have value operations
            if (cJSON_IsString(child) && child->valuestring && (operation_mask & OP_REPLACE_VALUES)) {
                char* new_value = apply_value_replacements(child->valuestring, operations, operation_count);
                if (new_value && strcmp(new_value, child->valuestring) != 0) {
                    free(child->valuestring);
                    child->valuestring = new_value;
                } else if (new_value) {
                    free(new_value);
                }
            }

            // Recursively process children
            if (cJSON_IsObject(child) || cJSON_IsArray(child)) {
                process_json_node_recursive_fast(child, operations, operation_count, operation_mask);
            }

            prev = child;
        }

        child = next;
    }
}

// Legacy function for compatibility (slower)
void process_json_node_recursive(cJSON* node, BuilderOperation* operations, size_t operation_count) {
    // Calculate operation mask for legacy calls
    uint32_t operation_mask = 0;
    for (size_t i = 0; i < operation_count; i++) {
        operation_mask |= operations[i].type;
    }
    process_json_node_recursive_fast(node, operations, operation_count, operation_mask);
}

// Optimized operation-specific processors using bitmask
bool should_remove_empty_string_fast(cJSON* item, uint32_t operation_mask) {
    if (!cJSON_IsString(item) || !item->valuestring) return false;

    // Fast bitmask check instead of linear search
    if (!(operation_mask & OP_REMOVE_EMPTY_STRINGS)) return false;

    return item->valuestring[0] == '\0';  // Optimized empty string check
}

bool should_remove_null_fast(cJSON* item, uint32_t operation_mask) {
    if (!cJSON_IsNull(item)) return false;

    // Fast bitmask check instead of linear search
    return (operation_mask & OP_REMOVE_NULLS) != 0;
}

// Legacy functions for compatibility (slower)
bool should_remove_empty_string(cJSON* item, BuilderOperation* operations, size_t operation_count) {
    if (!cJSON_IsString(item) || !item->valuestring) return false;

    // Check if remove_empty_strings operation is queued
    for (size_t i = 0; i < operation_count; i++) {
        if (operations[i].type == OP_REMOVE_EMPTY_STRINGS) {
            return item->valuestring[0] == '\0';  // Optimized empty string check
        }
    }
    return false;
}

bool should_remove_null(cJSON* item, BuilderOperation* operations, size_t operation_count) {
    if (!cJSON_IsNull(item)) return false;

    // Check if remove_nulls operation is queued
    for (size_t i = 0; i < operation_count; i++) {
        if (operations[i].type == OP_REMOVE_NULLS) {
            return true;
        }
    }
    return false;
}

// OPTIMIZED: Use pre-compiled regex patterns for massive performance boost
char* apply_key_replacements(const char* key, BuilderOperation* operations, size_t operation_count) {
    if (!key) return NULL;

    char* result = strdup(key);
    if (!result) return NULL;

    for (size_t i = 0; i < operation_count; i++) {
        if (operations[i].type == OP_REPLACE_KEYS && operations[i].replacement) {
            if (operations[i].regex_valid && operations[i].compiled_regex) {
                // SAFER: Use pre-compiled regex with additional safety checks
                int regex_result = regexec(operations[i].compiled_regex, result, 0, NULL, 0);
                if (regex_result == 0) {
                    char* new_result = strdup(operations[i].replacement);
                    if (new_result) {
                        free(result);
                        result = new_result;
                        break;  // Apply first matching replacement
                    }
                }
            } else if (operations[i].pattern) {
                // FALLBACK: Simple string matching for safety
                if (strstr(result, operations[i].pattern) != NULL) {
                    char* new_result = strdup(operations[i].replacement);
                    if (new_result) {
                        free(result);
                        result = new_result;
                        break;  // Apply first matching replacement
                    }
                }
            }
        }
    }

    return result;
}

char* apply_value_replacements(const char* value, BuilderOperation* operations, size_t operation_count) {
    if (!value) return NULL;

    char* result = strdup(value);
    if (!result) return NULL;

    for (size_t i = 0; i < operation_count; i++) {
        if (operations[i].type == OP_REPLACE_VALUES && operations[i].replacement) {
            if (operations[i].regex_valid && operations[i].compiled_regex) {
                // SAFER: Use pre-compiled regex with additional safety checks
                int regex_result = regexec(operations[i].compiled_regex, result, 0, NULL, 0);
                if (regex_result == 0) {
                    char* new_result = strdup(operations[i].replacement);
                    if (new_result) {
                        free(result);
                        result = new_result;
                        break;  // Apply first matching replacement
                    }
                }
            } else if (operations[i].pattern) {
                // FALLBACK: Simple string matching for safety
                if (strstr(result, operations[i].pattern) != NULL) {
                    char* new_result = strdup(operations[i].replacement);
                    if (new_result) {
                        free(result);
                        result = new_result;
                        break;  // Apply first matching replacement
                    }
                }
            }
        }
    }

    return result;
}
