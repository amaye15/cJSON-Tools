#include "json_tools_builder.h"
#include "json_flattener.h"
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#define INITIAL_OPERATION_CAPACITY 16

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

// Internal helper functions
static int add_operation(JsonToolsBuilder* builder, OperationType type, const char* pattern, const char* replacement) {
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
    
    builder->operation_count++;
    return 0;
}

static void clear_operations(JsonToolsBuilder* builder) {
    if (!builder || !builder->operations) return;
    
    for (size_t i = 0; i < builder->operation_count; i++) {
        if (builder->operations[i].pattern) {
            free(builder->operations[i].pattern);
        }
        if (builder->operations[i].replacement) {
            free(builder->operations[i].replacement);
        }
    }
    
    builder->operation_count = 0;
}

static char* execute_operations(JsonToolsBuilder* builder) {
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

// Single-pass JSON processing function
static cJSON* process_json_single_pass(cJSON* json, BuilderOperation* operations, size_t operation_count) {
    if (!json || !operations) return json;

    // Check if we need to flatten (must be done last)
    bool should_flatten = false;
    for (size_t i = 0; i < operation_count; i++) {
        if (operations[i].type == OP_FLATTEN) {
            should_flatten = true;
            break;
        }
    }

    // Process the JSON recursively
    process_json_node_recursive(json, operations, operation_count);

    // Apply flattening if requested (this changes the structure completely)
    if (should_flatten) {
        char* json_str = cJSON_PrintUnformatted(json);
        if (json_str) {
            char* flattened = flatten_json(json_str);
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

static void process_json_node_recursive(cJSON* node, BuilderOperation* operations, size_t operation_count) {
    if (!node) return;

    cJSON* child = node->child;
    cJSON* prev = NULL;

    while (child) {
        cJSON* next = child->next;  // Store next before potential deletion

        // Process the key if it exists
        if (child->string) {
            char* new_key = apply_key_replacements(child->string, operations, operation_count);
            if (new_key && strcmp(new_key, child->string) != 0) {
                free(child->string);
                child->string = new_key;
            } else if (new_key) {
                free(new_key);
            }
        }

        // Check if this item should be removed
        bool should_remove = false;

        // Check for empty string removal
        if (should_remove_empty_string(child, operations, operation_count)) {
            should_remove = true;
        }

        // Check for null removal
        if (!should_remove && should_remove_null(child, operations, operation_count)) {
            should_remove = true;
        }

        if (should_remove) {
            // Remove this child
            if (prev) {
                prev->next = next;
            } else {
                node->child = next;
            }
            if (next) {
                next->prev = prev;
            }
            cJSON_Delete(child);
        } else {
            // Process string values for replacement
            if (cJSON_IsString(child) && child->valuestring) {
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
                process_json_node_recursive(child, operations, operation_count);
            }

            prev = child;
        }

        child = next;
    }
}

// Operation-specific processors
static bool should_remove_empty_string(cJSON* item, BuilderOperation* operations, size_t operation_count) {
    if (!cJSON_IsString(item) || !item->valuestring) return false;

    // Check if remove_empty_strings operation is queued
    for (size_t i = 0; i < operation_count; i++) {
        if (operations[i].type == OP_REMOVE_EMPTY_STRINGS) {
            return strlen(item->valuestring) == 0;
        }
    }
    return false;
}

static bool should_remove_null(cJSON* item, BuilderOperation* operations, size_t operation_count) {
    if (!cJSON_IsNull(item)) return false;

    // Check if remove_nulls operation is queued
    for (size_t i = 0; i < operation_count; i++) {
        if (operations[i].type == OP_REMOVE_NULLS) {
            return true;
        }
    }
    return false;
}

static char* apply_key_replacements(const char* key, BuilderOperation* operations, size_t operation_count) {
    if (!key) return NULL;

    char* result = strdup(key);
    if (!result) return NULL;

    for (size_t i = 0; i < operation_count; i++) {
        if (operations[i].type == OP_REPLACE_KEYS && operations[i].pattern && operations[i].replacement) {
            regex_t regex;
            if (regcomp(&regex, operations[i].pattern, REG_EXTENDED) == 0) {
                if (regexec(&regex, result, 0, NULL, 0) == 0) {
                    free(result);
                    result = strdup(operations[i].replacement);
                    regfree(&regex);
                    break;  // Apply first matching replacement
                }
                regfree(&regex);
            }
        }
    }

    return result;
}

static char* apply_value_replacements(const char* value, BuilderOperation* operations, size_t operation_count) {
    if (!value) return NULL;

    char* result = strdup(value);
    if (!result) return NULL;

    for (size_t i = 0; i < operation_count; i++) {
        if (operations[i].type == OP_REPLACE_VALUES && operations[i].pattern && operations[i].replacement) {
            regex_t regex;
            if (regcomp(&regex, operations[i].pattern, REG_EXTENDED) == 0) {
                if (regexec(&regex, result, 0, NULL, 0) == 0) {
                    free(result);
                    result = strdup(operations[i].replacement);
                    regfree(&regex);
                    break;  // Apply first matching replacement
                }
                regfree(&regex);
            }
        }
    }

    return result;
}
