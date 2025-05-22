#include "../include/json_schema_generator.h"
#include "../include/json_utils.h"
#include "../include/thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MIN_OBJECTS_PER_THREAD 50  // Minimum number of objects to process per thread
#define MIN_BATCH_SIZE_FOR_MT 200  // Minimum batch size to use multi-threading

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

// Thread data structure
typedef struct {
    cJSON* object;
    SchemaNode* result;
    pthread_mutex_t* result_mutex;
} ThreadData;

// Create a new schema node
SchemaNode* create_schema_node(SchemaType type) {
    SchemaNode* node = (SchemaNode*)malloc(sizeof(SchemaNode));
    node->type = type;
    node->required = 1;
    node->nullable = 0;
    node->items = NULL;
    node->properties = NULL;
    node->required_props = NULL;
    node->required_count = 0;
    node->required_capacity = 0;
    node->enum_values = NULL;
    node->enum_count = 0;
    return node;
}

// Add a property to a schema node
void add_property(SchemaNode* node, const char* name, SchemaNode* property_schema, int required) {
    PropertyNode* prop = (PropertyNode*)malloc(sizeof(PropertyNode));
    prop->name = my_strdup(name);
    prop->schema = property_schema;
    prop->required = required;
    prop->next = node->properties;
    node->properties = prop;
    
    // Add to required properties list if required
    if (required) {
        if (node->required_count >= node->required_capacity) {
            node->required_capacity = node->required_capacity == 0 ? 8 : node->required_capacity * 2;
            node->required_props = (char**)realloc(node->required_props, 
                                                  node->required_capacity * sizeof(char*));
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

// Free a schema node
void free_schema_node(SchemaNode* node) {
    if (!node) return;
    
    // Free array items schema
    if (node->items) {
        free_schema_node(node->items);
    }
    
    // Free object properties
    PropertyNode* prop = node->properties;
    while (prop) {
        PropertyNode* next = prop->next;
        free(prop->name);
        free_schema_node(prop->schema);
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
    
    free(node);
}

// Get schema type from JSON type
SchemaType get_schema_type(cJSON* json) {
    switch (json->type) {
        case cJSON_False:
        case cJSON_True:
            return TYPE_BOOLEAN;
        case cJSON_NULL:
            return TYPE_NULL;
        case cJSON_Number:
            return (json->valueint == json->valuedouble) ? TYPE_INTEGER : TYPE_NUMBER;
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
            // Analyze array items
            int array_size = cJSON_GetArraySize(json);
            if (array_size > 0) {
                // Get schema for first item
                cJSON* first_item = cJSON_GetArrayItem(json, 0);
                SchemaNode* items_schema = analyze_json_value(first_item);
                
                // Check if all items have the same type
                for (int i = 1; i < array_size; i++) {
                    cJSON* item = cJSON_GetArrayItem(json, i);
                    SchemaNode* item_schema = analyze_json_value(item);
                    
                    // If types don't match, use mixed type
                    if (item_schema->type != items_schema->type) {
                        free_schema_node(items_schema);
                        free_schema_node(item_schema);
                        items_schema = create_schema_node(TYPE_MIXED);
                        break;
                    }
                    
                    free_schema_node(item_schema);
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
                merged->items = node1->items;
                node1->items = NULL; // Prevent double free
            } else if (node2->items) {
                merged->items = node2->items;
                node2->items = NULL; // Prevent double free
            }
            break;
            
        case TYPE_OBJECT:
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
                    // Mark as not required since it's missing in the second object
                    SchemaNode* prop_copy = analyze_json_value(cJSON_CreateNull());
                    prop_copy->type = prop1->schema->type;
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
                    // Mark as not required since it's missing in the first object
                    SchemaNode* prop_copy = analyze_json_value(cJSON_CreateNull());
                    prop_copy->type = prop2->schema->type;
                    prop_copy->nullable = 1;
                    add_property(merged, prop2->name, prop_copy, 0);
                }
                prop2 = prop2->next;
            }
            break;
            
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
                
                // Collect results
                for (int i = 0; i < array_size; i++) {
                    if (thread_data_array[i]) {
                        schemas[i] = thread_data_array[i]->result;
                        
                        // Free the individual thread data
                        free(thread_data_array[i]);
                    }
                }
                
                // Free the thread data array
                free(thread_data_array);
            }
            
            pthread_mutex_destroy(&result_mutex);
            thread_pool_destroy(pool);
        }
    }
    
    // Merge all schemas
    SchemaNode* merged_schema = schemas[0];
    for (int i = 1; i < array_size; i++) {
        merged_schema = merge_schema_nodes(merged_schema, schemas[i]);
        // Free the schema that was merged (but not the first one)
        free_schema_node(schemas[i]);
    }
    
    // Convert to JSON
    cJSON* result = schema_node_to_json(merged_schema);
    
    // Clean up
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