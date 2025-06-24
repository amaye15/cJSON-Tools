#ifndef JSON_SCHEMA_GENERATOR_H
#define JSON_SCHEMA_GENERATOR_H

#include <cjson/cJSON.h>

/**
 * Generates a JSON schema from a single JSON object
 * 
 * @param json The JSON object to analyze
 * @return A new JSON schema object (must be freed by caller)
 */
cJSON* generate_schema_from_object(cJSON* json);

/**
 * Generates a JSON schema from multiple JSON objects
 * 
 * @param json_array The array of JSON objects to analyze
 * @param use_threads Whether to use multi-threading
 * @param num_threads Number of threads to use (0 for auto-detection)
 * @return A new JSON schema object (must be freed by caller)
 */
cJSON* generate_schema_from_batch(cJSON* json_array, int use_threads, int num_threads);

/**
 * Generates a JSON schema from a JSON string (auto-detects single object or batch)
 * 
 * @param json_string The JSON string to analyze
 * @param use_threads Whether to use multi-threading
 * @param num_threads Number of threads to use (0 for auto-detection)
 * @return A new JSON schema string (must be freed by caller)
 */
char* generate_schema_from_string(const char* json_string, int use_threads, int num_threads);

#endif /* JSON_SCHEMA_GENERATOR_H */