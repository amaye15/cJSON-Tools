#include "../include/json_flattener.h"
#include "../include/json_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_KEY_LENGTH 1024
#define BATCH_SIZE 1000

// Structure to hold a flattened key-value pair
typedef struct {
    char* key;
    cJSON* value;
} FlattenedPair;

// Array to store flattened key-value pairs
typedef struct {
    FlattenedPair* pairs;
    int count;
    int capacity;
} FlattenedArray;

// Thread data structure
typedef struct {
    cJSON** objects;
    cJSON** results;
    int start_idx;
    int end_idx;
    int thread_id;
} ThreadData;

// Initialize the flattened array
void init_flattened_array(FlattenedArray* array, int initial_capacity) {
    array->pairs = (FlattenedPair*)malloc(initial_capacity * sizeof(FlattenedPair));
    array->count = 0;
    array->capacity = initial_capacity;
}

// Add a key-value pair to the flattened array
void add_pair(FlattenedArray* array, const char* key, cJSON* value) {
    // Resize if needed
    if (array->count >= array->capacity) {
        array->capacity *= 2;
        array->pairs = (FlattenedPair*)realloc(array->pairs, array->capacity * sizeof(FlattenedPair));
    }
    
    // Add the new pair
    array->pairs[array->count].key = my_strdup(key);
    array->pairs[array->count].value = value;
    array->count++;
}

// Free the flattened array
void free_flattened_array(FlattenedArray* array) {
    for (int i = 0; i < array->count; i++) {
        free(array->pairs[i].key);
    }
    free(array->pairs);
    array->count = 0;
    array->capacity = 0;
}

// Recursively flatten a JSON object
void flatten_json_recursive(cJSON* json, const char* prefix, FlattenedArray* result) {
    if (json == NULL) return;
    
    // Handle different types of JSON values
    switch (json->type) {
        case cJSON_Object: {
            cJSON* child = json->child;
            while (child != NULL) {
                char* new_prefix;
                if (prefix && strlen(prefix) > 0) {
                    new_prefix = (char*)malloc(strlen(prefix) + strlen(child->string) + 2);
                    sprintf(new_prefix, "%s.%s", prefix, child->string);
                } else {
                    new_prefix = my_strdup(child->string);
                }
                
                // Recursively flatten child objects
                if (child->type == cJSON_Object || child->type == cJSON_Array) {
                    flatten_json_recursive(child, new_prefix, result);
                } else {
                    add_pair(result, new_prefix, child);
                }
                
                free(new_prefix);
                child = child->next;
            }
            break;
        }
        case cJSON_Array: {
            cJSON* child = json->child;
            int index = 0;
            while (child != NULL) {
                char* new_prefix;
                new_prefix = (char*)malloc(strlen(prefix) + 32); // Enough space for prefix + index
                sprintf(new_prefix, "%s[%d]", prefix, index);
                
                // Recursively flatten array elements
                if (child->type == cJSON_Object || child->type == cJSON_Array) {
                    flatten_json_recursive(child, new_prefix, result);
                } else {
                    add_pair(result, new_prefix, child);
                }
                
                free(new_prefix);
                child = child->next;
                index++;
            }
            break;
        }
        default:
            // For primitive types, just add them directly
            add_pair(result, prefix, json);
            break;
    }
}

// Create a new flattened JSON object from the flattened array
cJSON* create_flattened_json(FlattenedArray* flattened_array) {
    cJSON* result = cJSON_CreateObject();
    
    for (int i = 0; i < flattened_array->count; i++) {
        FlattenedPair pair = flattened_array->pairs[i];
        
        // Add the appropriate type of value to the result
        switch (pair.value->type) {
            case cJSON_False:
                cJSON_AddFalseToObject(result, pair.key);
                break;
            case cJSON_True:
                cJSON_AddTrueToObject(result, pair.key);
                break;
            case cJSON_NULL:
                cJSON_AddNullToObject(result, pair.key);
                break;
            case cJSON_Number:
                if (pair.value->valueint == pair.value->valuedouble) {
                    cJSON_AddNumberToObject(result, pair.key, pair.value->valueint);
                } else {
                    cJSON_AddNumberToObject(result, pair.key, pair.value->valuedouble);
                }
                break;
            case cJSON_String:
                cJSON_AddStringToObject(result, pair.key, pair.value->valuestring);
                break;
            default:
                // This shouldn't happen as we've already flattened objects and arrays
                break;
        }
    }
    
    return result;
}

// Flatten a single JSON object
cJSON* flatten_single_object(cJSON* json) {
    if (!json) return NULL;
    
    FlattenedArray flattened_array;
    init_flattened_array(&flattened_array, 16); // Start with capacity for 16 items
    
    if (json->type == cJSON_Object) {
        cJSON* child = json->child;
        while (child != NULL) {
            if (child->type == cJSON_Object || child->type == cJSON_Array) {
                flatten_json_recursive(child, child->string, &flattened_array);
            } else {
                add_pair(&flattened_array, child->string, child);
            }
            child = child->next;
        }
    } else if (json->type == cJSON_Array) {
        flatten_json_recursive(json, "", &flattened_array);
    } else {
        // If the root is a primitive value, just return a copy
        cJSON* result = cJSON_Duplicate(json, 1);
        free_flattened_array(&flattened_array);
        return result;
    }
    
    // Create a new JSON object with the flattened key-value pairs
    cJSON* flattened_json = create_flattened_json(&flattened_array);
    
    // Clean up
    free_flattened_array(&flattened_array);
    
    return flattened_json;
}

// Thread worker function for batch flattening
void* flatten_batch_worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    for (int i = data->start_idx; i < data->end_idx; i++) {
        data->results[i] = flatten_single_object(data->objects[i]);
    }
    
    return NULL;
}

// Implementation of the public API functions

/**
 * Flattens a single JSON object
 */
cJSON* flatten_json_object(cJSON* json) {
    return flatten_single_object(json);
}

/**
 * Flattens a batch of JSON objects (array of objects)
 */
cJSON* flatten_json_batch(cJSON* json_array, int use_threads, int num_threads) {
    if (!json_array || json_array->type != cJSON_Array) {
        return NULL;
    }
    
    int array_size = cJSON_GetArraySize(json_array);
    if (array_size == 0) {
        return cJSON_CreateArray();
    }
    
    // If only one item or threading disabled, process sequentially
    if (array_size == 1 || !use_threads) {
        cJSON* result = cJSON_CreateArray();
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(json_array, i);
            cJSON* flattened = flatten_single_object(item);
            if (flattened) {
                cJSON_AddItemToArray(result, flattened);
            }
        }
        return result;
    }
    
    // Multi-threaded processing
    int thread_count = get_optimal_threads(num_threads);
    if (thread_count > array_size) {
        thread_count = array_size;
    }
    
    pthread_t* threads = (pthread_t*)malloc(thread_count * sizeof(pthread_t));
    ThreadData* thread_data = (ThreadData*)malloc(thread_count * sizeof(ThreadData));
    
    // Extract all objects into an array for easier access
    cJSON** objects = (cJSON**)malloc(array_size * sizeof(cJSON*));
    cJSON** results = (cJSON**)malloc(array_size * sizeof(cJSON*));
    
    for (int i = 0; i < array_size; i++) {
        objects[i] = cJSON_GetArrayItem(json_array, i);
        results[i] = NULL;
    }
    
    // Divide work among threads
    int items_per_thread = array_size / thread_count;
    int remainder = array_size % thread_count;
    
    int start_idx = 0;
    for (int i = 0; i < thread_count; i++) {
        thread_data[i].objects = objects;
        thread_data[i].results = results;
        thread_data[i].start_idx = start_idx;
        thread_data[i].thread_id = i;
        
        // Distribute remainder items among the first 'remainder' threads
        int extra = (i < remainder) ? 1 : 0;
        thread_data[i].end_idx = start_idx + items_per_thread + extra;
        
        start_idx = thread_data[i].end_idx;
        
        pthread_create(&threads[i], NULL, flatten_batch_worker, &thread_data[i]);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Create result array
    cJSON* result = cJSON_CreateArray();
    for (int i = 0; i < array_size; i++) {
        if (results[i]) {
            cJSON_AddItemToArray(result, results[i]);
        }
    }
    
    // Clean up
    free(objects);
    free(results);
    free(threads);
    free(thread_data);
    
    return result;
}

/**
 * Flattens a JSON string (auto-detects single object or batch)
 */
char* flatten_json_string(const char* json_string, int use_threads, int num_threads) {
    if (!json_string) return NULL;
    
    cJSON* json = cJSON_Parse(json_string);
    if (!json) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            fprintf(stderr, "Error parsing JSON: %s\n", error_ptr);
        }
        return NULL;
    }
    
    cJSON* flattened = NULL;
    
    if (json->type == cJSON_Array) {
        flattened = flatten_json_batch(json, use_threads, num_threads);
    } else {
        flattened = flatten_json_object(json);
    }
    
    char* result = NULL;
    if (flattened) {
        result = cJSON_Print(flattened);
        cJSON_Delete(flattened);
    }
    
    cJSON_Delete(json);
    return result;
}