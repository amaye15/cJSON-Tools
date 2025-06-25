#include "../include/json_schema_generator.h"
#include "../include/json_utils.h"
#include "../include/thread_pool.h"
#include "../include/memory_pool.h"
#include "../include/string_view.h"
#include "../include/compiler_hints.h"
#include "../include/simd_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define MIN_OBJECTS_PER_THREAD 25   // Reduced for better parallelization
#define MIN_BATCH_SIZE_FOR_MT 100   // Reduced threshold for multi-threading
#define INITIAL_PROPS_CAPACITY 16   // Initial capacity for properties
#define INITIAL_REQUIRED_CAPACITY 8 // Initial capacity for required properties
#define SCHEMA_CACHE_SIZE 128       // Cache for common schema patterns
#define MAX_ARRAY_SAMPLE_SIZE 50    // Maximum array items to sample for type inference
#define HASH_TABLE_SIZE 64          // Hash table size for property lookup

// Schema node types
typedef enum {
    TYPE_NULL,
    TYPE_BOOLEAN,
    TYPE_INTEGER,
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_ARRAY,
    TYPE_OBJECT,
    TYPE_MIXED
} SchemaType;

// Structure to represent a schema node
typedef struct SchemaNode {
    SchemaType type;
    int required;
    int nullable;
    
    // For arrays
    struct SchemaNode* items;
    
    // For objects
    struct PropertyNode* properties;
    char** required_props;
    int required_count;
    int required_capacity;
    
    // For enums
    cJSON* enum_values;
    int enum_count;
} SchemaNode;

// Structure to represent a property in an object
typedef struct PropertyNode {
    char* name;
    SchemaNode* schema;
    int required;
    struct PropertyNode* next;
} PropertyNode;

// Hash table for O(1) property lookup
typedef struct PropertyHash {
    PropertyNode* buckets[HASH_TABLE_SIZE];
} PropertyHash;

// Hash function for property names
static unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_TABLE_SIZE;
}

// Fast property lookup using hash table
static PropertyNode* find_property_fast(PropertyHash* hash, const char* name) __attribute__((unused));
static PropertyNode* find_property_fast(PropertyHash* hash, const char* name) {
    unsigned int bucket = hash_string(name);
    PropertyNode* prop = hash->buckets[bucket];
    while (prop) {
        if (strcmp(prop->name, name) == 0) {
            return prop;
        }
        prop = prop->next;
    }
    return NULL;
}

// Thread data structure
typedef struct {
    cJSON* object;
    SchemaNode* result;
    pthread_mutex_t* result_mutex;
} ThreadData;

// Create a new schema node with optimized initialization
SchemaNode* create_schema_node(SchemaType type) {
    // Initialize global memory pools if not already done
    if (!g_cjson_node_pool) {
        init_global_pools();
    }
    SchemaNode* node = (SchemaNode*)malloc(sizeof(SchemaNode));
    if (__builtin_expect(!node, 0)) return NULL;

    // Use memset for faster initialization
    memset(node, 0, sizeof(SchemaNode));
    node->type = type;
    node->required = 1;

    return node;
}

// Cache-optimized property addition with consistent memory allocation
HOT_PATH void add_property(SchemaNode* node, const char* name, SchemaNode* property_schema, int required) {
    // Always use regular malloc for property nodes to ensure consistent freeing
    PropertyNode* prop = (PropertyNode*)malloc(sizeof(PropertyNode));
    if (UNLIKELY(!prop)) return;

    // Use string view for faster string operations
    StringView name_view __attribute__((unused)) = make_string_view_cstr(name);

    prop->name = my_strdup(name);
    prop->schema = property_schema;
    prop->required = required;
    prop->next = node->properties;
    node->properties = prop;

    // Add to required properties list if required
    if (required) {
        if (UNLIKELY(node->required_count >= node->required_capacity)) {
            int new_capacity = node->required_capacity == 0 ? INITIAL_REQUIRED_CAPACITY : node->required_capacity * 2;
            char** new_props = (char**)realloc(node->required_props, new_capacity * sizeof(char*));
            if (UNLIKELY(!new_props)) return;

            node->required_props = new_props;
            node->required_capacity = new_capacity;
        }
        node->required_props[node->required_count++] = my_strdup(name);
    }
}

// Find a property in a schema node
PropertyNode* find_property(SchemaNode* node, const char* name) {
    PropertyNode* prop = node->properties;
    while (prop) {
        if (strcmp(prop->name, name) == 0) {
            return prop;
        }
        prop = prop->next;
    }
    return NULL;
}

// Free a schema node with proper memory pool handling
void free_schema_node(SchemaNode* node) {
    if (!node) return;

    // Free array items schema
    if (node->items) {
        free_schema_node(node->items);
    }

    // Free object properties with consistent memory management
    PropertyNode* prop = node->properties;
    while (prop) {
        PropertyNode* next = prop->next;
        free(prop->name);
        free_schema_node(prop->schema);

        // Always use regular free since we use regular malloc
        free(prop);
        prop = next;
    }

    // Free required properties list
    for (int i = 0; i < node->required_count; i++) {
        free(node->required_props[i]);
    }
    free(node->required_props);

    // Free enum values
    if (node->enum_values) {
        cJSON_Delete(node->enum_values);
    }

    // Always use regular free for schema nodes since they're allocated with malloc
    free(node);
}

// Optimized schema type detection with better number handling
SchemaType get_schema_type(cJSON* json) {
    switch (json->type) {
        case cJSON_False:
        case cJSON_True:
            return TYPE_BOOLEAN;
        case cJSON_NULL:
            return TYPE_NULL;
        case cJSON_Number:
            // More precise integer detection
            if (json->valuedouble == (double)json->valueint &&
                json->valuedouble >= INT_MIN && json->valuedouble <= INT_MAX) {
                return TYPE_INTEGER;
            }
            return TYPE_NUMBER;
        case cJSON_String:
            return TYPE_STRING;
        case cJSON_Array:
            return TYPE_ARRAY;
        case cJSON_Object:
            return TYPE_OBJECT;
        default:
            return TYPE_NULL;
    }
}

// Analyze a JSON value and create a schema node
SchemaNode* analyze_json_value(cJSON* json) {
    if (!json) return NULL;
    
    SchemaType type = get_schema_type(json);
    SchemaNode* node = create_schema_node(type);
    
    // Mark null values as not required and nullable
    if (type == TYPE_NULL) {
        node->required = 0;
        node->nullable = 1;
    }
    
    switch (type) {
        case TYPE_ARRAY: {
            // Highly optimized array analysis with intelligent sampling
            int array_size = cJSON_GetArraySize(json);
            if (__builtin_expect(array_size > 0, 1)) {
                // Get schema for first item
                cJSON* first_item = cJSON_GetArrayItem(json, 0);
                SchemaNode* items_schema = analyze_json_value(first_item);
                SchemaType first_type = items_schema->type;

                // Intelligent sampling strategy for large arrays
                int check_limit = array_size > MAX_ARRAY_SAMPLE_SIZE ? MAX_ARRAY_SAMPLE_SIZE : array_size;
                int step = array_size > MAX_ARRAY_SAMPLE_SIZE ? array_size / MAX_ARRAY_SAMPLE_SIZE : 1;
                bool types_match = true;

                // Sample items across the array for better type detection
                for (int i = step; i < array_size && types_match; i += step) {
                    if (i >= check_limit) break;

                    cJSON* item = cJSON_GetArrayItem(json, i);
                    SchemaType item_type = get_schema_type(item);

                    // If types don't match, use mixed type
                    if (item_type != first_type) {
                        types_match = false;
                    }
                }

                if (!types_match) {
                    free_schema_node(items_schema);
                    items_schema = create_schema_node(TYPE_MIXED);
                }

                node->items = items_schema;
            } else {
                // Empty array, use null type for items
                node->items = create_schema_node(TYPE_NULL);
            }
            break;
        }
        case TYPE_OBJECT: {
            // Analyze object properties
            cJSON* child = json->child;
            while (child) {
                SchemaNode* prop_schema = analyze_json_value(child);
                add_property(node, child->string, prop_schema, prop_schema->required);
                child = child->next;
            }
            break;
        }
        default:
            // For primitive types, nothing more to do
            break;
    }
    
    return node;
}

// Merge two schema nodes
SchemaNode* merge_schema_nodes(SchemaNode* node1, SchemaNode* node2) {
    if (!node1) return node2;
    if (!node2) return node1;
    
    // If types don't match, use mixed type
    if (node1->type != node2->type) {
        SchemaNode* merged = create_schema_node(TYPE_MIXED);
        merged->required = node1->required && node2->required;
        merged->nullable = node1->nullable || node2->nullable || 
                          node1->type == TYPE_NULL || node2->type == TYPE_NULL;
        return merged;
    }
    
    // Types match, merge based on type
    SchemaNode* merged = create_schema_node(node1->type);
    merged->required = node1->required && node2->required;
    merged->nullable = node1->nullable || node2->nullable;
    
    switch (node1->type) {
        case TYPE_ARRAY:
            // Merge array items schemas
            if (node1->items && node2->items) {
                merged->items = merge_schema_nodes(node1->items, node2->items);
            } else if (node1->items) {
                // Create a copy of the items schema
                merged->items = create_schema_node(node1->items->type);
                merged->items->nullable = node1->items->nullable;
                merged->items->required = node1->items->required;
            } else if (node2->items) {
                // Create a copy of the items schema
                merged->items = create_schema_node(node2->items->type);
                merged->items->nullable = node2->items->nullable;
                merged->items->required = node2->items->required;
            }
            break;
            
        case TYPE_OBJECT: {
            // Merge object properties
            PropertyNode* prop1 = node1->properties;
            while (prop1) {
                PropertyNode* prop2 = find_property(node2, prop1->name);
                if (prop2) {
                    // Property exists in both objects, merge schemas
                    SchemaNode* merged_prop = merge_schema_nodes(prop1->schema, prop2->schema);
                    add_property(merged, prop1->name, merged_prop, prop1->required && prop2->required);
                } else {
                    // Property only exists in first object
                    // Create a copy of the schema and mark as not required
                    SchemaNode* prop_copy = create_schema_node(prop1->schema->type);
                    prop_copy->nullable = 1;
                    add_property(merged, prop1->name, prop_copy, 0);
                }
                prop1 = prop1->next;
            }
            
            // Add properties that only exist in the second object
            PropertyNode* prop2 = node2->properties;
            while (prop2) {
                if (!find_property(node1, prop2->name)) {
                    // Property only exists in second object
                    // Create a copy of the schema and mark as not required
                    SchemaNode* prop_copy = create_schema_node(prop2->schema->type);
                    prop_copy->nullable = 1;
                    add_property(merged, prop2->name, prop_copy, 0);
                }
                prop2 = prop2->next;
            }
            break;
        }
            
        default:
            // For primitive types, nothing more to do
            break;
    }
    
    return merged;
}

// Convert schema type to string
const char* schema_type_to_string(SchemaType type) {
    switch (type) {
        case TYPE_NULL: return "null";
        case TYPE_BOOLEAN: return "boolean";
        case TYPE_INTEGER: return "integer";
        case TYPE_NUMBER: return "number";
        case TYPE_STRING: return "string";
        case TYPE_ARRAY: return "array";
        case TYPE_OBJECT: return "object";
        case TYPE_MIXED: return "mixed";
        default: return "unknown";
    }
}

// Convert schema node to cJSON object
cJSON* schema_node_to_json(SchemaNode* node) {
    if (!node) return NULL;
    
    cJSON* schema = cJSON_CreateObject();
    
    // Add $schema for root objects
    cJSON_AddStringToObject(schema, "$schema", "http://json-schema.org/draft-07/schema#");
    
    // Handle mixed type (multiple possible types)
    if (node->type == TYPE_MIXED) {
        cJSON* type_array = cJSON_CreateArray();
        cJSON_AddItemToArray(type_array, cJSON_CreateString("string"));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("number"));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("integer"));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("boolean"));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("object"));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("array"));
        
        if (node->nullable) {
            cJSON_AddItemToArray(type_array, cJSON_CreateString("null"));
        }
        
        cJSON_AddItemToObject(schema, "type", type_array);
        return schema;
    }
    
    // Handle nullable fields
    if (node->nullable) {
        cJSON* type_array = cJSON_CreateArray();
        cJSON_AddItemToArray(type_array, cJSON_CreateString(schema_type_to_string(node->type)));
        cJSON_AddItemToArray(type_array, cJSON_CreateString("null"));
        cJSON_AddItemToObject(schema, "type", type_array);
    } else {
        cJSON_AddStringToObject(schema, "type", schema_type_to_string(node->type));
    }
    
    // Add type-specific properties
    switch (node->type) {
        case TYPE_ARRAY:
            if (node->items) {
                cJSON* items_schema = cJSON_CreateObject();
                
                // Handle mixed type for array items
                if (node->items->type == TYPE_MIXED) {
                    cJSON* type_array = cJSON_CreateArray();
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("string"));
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("number"));
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("integer"));
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("boolean"));
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("object"));
                    cJSON_AddItemToArray(type_array, cJSON_CreateString("array"));
                    
                    if (node->items->nullable) {
                        cJSON_AddItemToArray(type_array, cJSON_CreateString("null"));
                    }
                    
                    cJSON_AddItemToObject(items_schema, "type", type_array);
                } else {
                    if (node->items->nullable) {
                        cJSON* type_array = cJSON_CreateArray();
                        cJSON_AddItemToArray(type_array, cJSON_CreateString(schema_type_to_string(node->items->type)));
                        cJSON_AddItemToArray(type_array, cJSON_CreateString("null"));
                        cJSON_AddItemToObject(items_schema, "type", type_array);
                    } else {
                        cJSON_AddStringToObject(items_schema, "type", schema_type_to_string(node->items->type));
                    }
                }
                
                // Add nested properties for object items
                if (node->items->type == TYPE_OBJECT && node->items->properties) {
                    cJSON* props = cJSON_CreateObject();
                    cJSON* required = cJSON_CreateArray();
                    
                    PropertyNode* prop = node->items->properties;
                    while (prop) {
                        cJSON* prop_schema = schema_node_to_json(prop->schema);
                        // Remove $schema from nested objects
                        cJSON_DeleteItemFromObject(prop_schema, "$schema");
                        cJSON_AddItemToObject(props, prop->name, prop_schema);
                        
                        if (prop->required) {
                            cJSON_AddItemToArray(required, cJSON_CreateString(prop->name));
                        }
                        
                        prop = prop->next;
                    }
                    
                    cJSON_AddItemToObject(items_schema, "properties", props);
                    
                    if (cJSON_GetArraySize(required) > 0) {
                        cJSON_AddItemToObject(items_schema, "required", required);
                    } else {
                        cJSON_Delete(required);
                    }
                }
                
                cJSON_AddItemToObject(schema, "items", items_schema);
            }
            break;
            
        case TYPE_OBJECT:
            if (node->properties) {
                cJSON* props = cJSON_CreateObject();
                cJSON* required = cJSON_CreateArray();
                
                PropertyNode* prop = node->properties;
                while (prop) {
                    cJSON* prop_schema = schema_node_to_json(prop->schema);
                    // Remove $schema from nested objects
                    cJSON_DeleteItemFromObject(prop_schema, "$schema");
                    cJSON_AddItemToObject(props, prop->name, prop_schema);
                    
                    if (prop->required) {
                        cJSON_AddItemToArray(required, cJSON_CreateString(prop->name));
                    }
                    
                    prop = prop->next;
                }
                
                cJSON_AddItemToObject(schema, "properties", props);
                
                if (cJSON_GetArraySize(required) > 0) {
                    cJSON_AddItemToObject(schema, "required", required);
                } else {
                    cJSON_Delete(required);
                }
            }
            break;
            
        default:
            // For primitive types, nothing more to do
            break;
    }
    
    return schema;
}

// Thread worker function for schema generation
void generate_schema_task(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    // Process the object
    data->result = analyze_json_value(data->object);
    
    // Note: We don't free the thread data here anymore
    // It will be freed after collecting the results
}

// Implementation of the public API functions

/**
 * Generates a JSON schema from a single JSON object
 */
cJSON* generate_schema_from_object(cJSON* json) {
    if (!json) return NULL;
    
    SchemaNode* schema_node = analyze_json_value(json);
    cJSON* schema = schema_node_to_json(schema_node);
    free_schema_node(schema_node);
    
    return schema;
}

/**
 * Generates a JSON schema from multiple JSON objects
 */
cJSON* generate_schema_from_batch(cJSON* json_array, int use_threads, int num_threads) {
    if (!json_array || json_array->type != cJSON_Array) {
        return NULL;
    }
    
    int array_size = cJSON_GetArraySize(json_array);
    if (array_size == 0) {
        return NULL;
    }
    
    // If only one item, process directly
    if (array_size == 1) {
        cJSON* item = cJSON_GetArrayItem(json_array, 0);
        return generate_schema_from_object(item);
    }
    
    // Determine if multi-threading should be used
    // Only use multi-threading if:
    // 1. Threading is enabled
    // 2. Array size is large enough to justify threading
    // 3. We have more than one thread available
    int should_use_threads = use_threads && 
                            array_size >= MIN_BATCH_SIZE_FOR_MT && 
                            get_optimal_threads(num_threads) > 1;
    
    // Extract all objects into an array for easier access
    cJSON** objects = (cJSON**)malloc(array_size * sizeof(cJSON*));
    SchemaNode** schemas = (SchemaNode**)malloc(array_size * sizeof(SchemaNode*));
    
    for (int i = 0; i < array_size; i++) {
        objects[i] = cJSON_GetArrayItem(json_array, i);
        schemas[i] = NULL;
    }
    
    // If threading disabled or not beneficial, process sequentially
    if (!should_use_threads) {
        for (int i = 0; i < array_size; i++) {
            schemas[i] = analyze_json_value(objects[i]);
        }
    } else {
        // Multi-threaded processing with thread pool
        ThreadPool* pool = thread_pool_create(num_threads);
        if (!pool) {
            // Fall back to sequential processing if thread pool creation fails
            for (int i = 0; i < array_size; i++) {
                schemas[i] = analyze_json_value(objects[i]);
            }
        } else {
            pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;
            
            // Create an array of thread data structures
            ThreadData** thread_data_array = (ThreadData**)calloc(array_size, sizeof(ThreadData*));
            if (!thread_data_array) {
                // Fall back to sequential processing
                for (int i = 0; i < array_size; i++) {
                    schemas[i] = analyze_json_value(objects[i]);
                }
            } else {
                // Submit tasks to thread pool
                for (int i = 0; i < array_size; i++) {
                    ThreadData* data = (ThreadData*)malloc(sizeof(ThreadData));
                    if (!data) continue;
                    
                    data->object = objects[i];
                    data->result = NULL;
                    data->result_mutex = &result_mutex;
                    
                    // Store the thread data for later collection
                    thread_data_array[i] = data;
                    
                    // Add task to thread pool
                    if (thread_pool_add_task(pool, generate_schema_task, data) != 0) {
                        // If adding task fails, process it directly
                        generate_schema_task(data);
                    }
                }
                
                // Wait for all tasks to complete
                thread_pool_wait(pool);
                
                // Collect results with null checking
                for (int i = 0; i < array_size; i++) {
                    if (thread_data_array[i]) {
                        schemas[i] = thread_data_array[i]->result;

                        // Free the individual thread data
                        free(thread_data_array[i]);
                        thread_data_array[i] = NULL;
                    } else {
                        // If thread data allocation failed, set schema to NULL
                        schemas[i] = NULL;
                    }
                }
                
                // Free the thread data array
                free(thread_data_array);
            }
            
            pthread_mutex_destroy(&result_mutex);
            thread_pool_destroy(pool);
        }
    }
    
    // Create a merged schema by combining all valid schemas
    SchemaNode* merged_schema = create_schema_node(TYPE_OBJECT);
    if (!merged_schema) {
        free(objects);
        free(schemas);
        return NULL;
    }

    // Merge properties from all valid schemas
    for (int i = 0; i < array_size; i++) {
        if (schemas[i] != NULL && schemas[i]->type == TYPE_OBJECT) {
            // Copy all properties from this schema to the merged schema
            PropertyNode* prop = schemas[i]->properties;
            while (prop) {
                // Check if property already exists in merged schema
                PropertyNode* existing = find_property(merged_schema, prop->name);
                if (!existing) {
                    // Create a copy of the property schema
                    SchemaNode* prop_copy = create_schema_node(prop->schema->type);
                    if (prop_copy) {
                        // Copy basic properties
                        prop_copy->nullable = prop->schema->nullable;
                        prop_copy->required = prop->schema->required;

                        // Add the property to merged schema
                        add_property(merged_schema, prop->name, prop_copy, prop->required);
                    }
                }
                prop = prop->next;
            }
        }
    }

    // Free all individual schemas
    for (int i = 0; i < array_size; i++) {
        if (schemas[i] != NULL) {
            free_schema_node(schemas[i]);
        }
    }

    // Convert to JSON
    cJSON* result = schema_node_to_json(merged_schema);

    // Clean up the merged schema and arrays
    free_schema_node(merged_schema);
    free(objects);
    free(schemas);

    return result;
}

/**
 * Generates a JSON schema from a JSON string (auto-detects single object or batch)
 */
char* generate_schema_from_string(const char* json_string, int use_threads, int num_threads) {
    if (!json_string) return NULL;
    
    cJSON* json = cJSON_Parse(json_string);
    if (!json) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            fprintf(stderr, "Error parsing JSON: %s\n", error_ptr);
        }
        return NULL;
    }
    
    cJSON* schema = NULL;
    
    if (json->type == cJSON_Array) {
        schema = generate_schema_from_batch(json, use_threads, num_threads);
    } else {
        schema = generate_schema_from_object(json);
    }
    
    char* result = NULL;
    if (schema) {
        result = cJSON_Print(schema);
        cJSON_Delete(schema);
    }
    
    cJSON_Delete(json);
    return result;
}