#ifndef JSON_FLATTENER_H
#define JSON_FLATTENER_H

#include <cjson/cJSON.h>

/**
 * Flattens a single JSON object
 * 
 * @param json The JSON object to flatten
 * @return A new flattened JSON object (must be freed by caller)
 */
cJSON* flatten_json_object(cJSON* json);

/**
 * Flattens a batch of JSON objects (array of objects)
 * 
 * @param json_array The JSON array to flatten
 * @param use_threads Whether to use multi-threading
 * @param num_threads Number of threads to use (0 for auto-detection)
 * @return A new flattened JSON array (must be freed by caller)
 */
cJSON* flatten_json_batch(cJSON* json_array, int use_threads, int num_threads);

/**
 * Flattens a JSON string (auto-detects single object or batch)
 *
 * @param json_string The JSON string to flatten
 * @param use_threads Whether to use multi-threading
 * @param num_threads Number of threads to use (0 for auto-detection)
 * @return A new flattened JSON string (must be freed by caller)
 */
char* flatten_json_string(const char* json_string, int use_threads, int num_threads);

/**
 * Gets flattened paths with their data types from a JSON object
 *
 * @param json The JSON object to analyze
 * @return A new JSON object with paths as keys and data types as values (must be freed by caller)
 */
cJSON* get_flattened_paths_with_types(cJSON* json);

/**
 * Gets flattened paths with their data types from a JSON string
 *
 * @param json_string The JSON string to analyze
 * @return A new JSON string with paths as keys and data types as values (must be freed by caller)
 */
char* get_flattened_paths_with_types_string(const char* json_string);

#endif /* JSON_FLATTENER_H */