#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "cjson/cJSON.h"

/**
 * Custom string duplication function (replacement for strdup)
 * 
 * @param str The string to duplicate
 * @return A newly allocated copy of the string
 */
char* my_strdup(const char* str);

/**
 * Reads a JSON file into a string
 * 
 * @param filename The name of the file to read
 * @return The file contents as a string (must be freed by caller)
 */
char* read_json_file(const char* filename);

/**
 * Reads JSON from stdin into a string
 * 
 * @return The stdin contents as a string (must be freed by caller)
 */
char* read_json_stdin(void);

/**
 * Determines the number of CPU cores available
 * 
 * @return The number of CPU cores
 */
int get_num_cores(void);

/**
 * Determines the optimal number of threads to use
 * 
 * @param requested_threads The number of threads requested (0 for auto)
 * @return The optimal number of threads to use
 */
int get_optimal_threads(int requested_threads);

#endif /* JSON_UTILS_H */