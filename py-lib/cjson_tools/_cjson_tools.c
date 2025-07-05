#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "../../c-lib/include/cjson/cJSON.h"
#include "../../c-lib/include/json_flattener.h"
#include "../../c-lib/include/json_schema_generator.h"
#include "../../c-lib/include/json_tools_builder.h"
#include "../../c-lib/include/json_utils.h"
#include "../../c-lib/include/simd_utils.h"
#include "../../c-lib/include/thread_pool.h"
#include "../../c-lib/include/memory_pool.h"

#define MODULE_VERSION "1.9.0"

/**
 * Flatten a JSON string
 */
static PyObject* py_flatten_json(PyObject* self, PyObject* args, PyObject* kwargs) {
    (void)self; // Suppress unused parameter warning
    const char* json_string;
    int use_threads = 0;
    int num_threads = 0;
    int pretty_print = 0;

    static char* kwlist[] = {"json_string", "use_threads", "num_threads", "pretty_print", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|iii", kwlist,
                                    &json_string, &use_threads, &num_threads, &pretty_print)) {
        return NULL;
    }

    char* result;

    // Release GIL during C computation for better parallelism
    Py_BEGIN_ALLOW_THREADS

    // Initialize memory pools for optimal performance
    init_global_pools();

    result = flatten_json_string(json_string, use_threads, num_threads);
    Py_END_ALLOW_THREADS

    if (result == NULL) {
        PyErr_SetString(PyExc_ValueError, "Failed to flatten JSON");
        return NULL;
    }

    // Apply pretty printing if requested
    char* final_result = result;
    if (pretty_print) {
        cJSON* json = cJSON_Parse(result);
        if (json) {
            free(result);
            final_result = cJSON_Print(json);
            cJSON_Delete(json);
        }
    }

    // Convert the result to a Python string
    PyObject* py_result = PyUnicode_FromString(final_result);

    // Free the C string
    free(final_result);

    return py_result;
}

/**
 * Flatten a batch of JSON objects
 */
static PyObject* py_flatten_json_batch(PyObject* self, PyObject* args, PyObject* kwargs) {
    (void)self; // Suppress unused parameter warning
    PyObject* json_list;
    int use_threads = 1;
    int num_threads = 0;
    int pretty_print = 0;

    static char* kwlist[] = {"json_list", "use_threads", "num_threads", "pretty_print", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|iii", kwlist,
                                    &json_list, &use_threads, &num_threads, &pretty_print)) {
        return NULL;
    }

    // Check if the input is a list
    if (!PyList_Check(json_list)) {
        PyErr_SetString(PyExc_TypeError, "Expected a list of JSON strings");
        return NULL;
    }

    Py_ssize_t list_size = PyList_Size(json_list);
    
    // Create a JSON array
    cJSON* json_array = cJSON_CreateArray();
    if (json_array == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to create JSON array");
        return NULL;
    }
    
    // Convert each Python object to a JSON object and add to the array
    for (Py_ssize_t i = 0; i < list_size; i++) {
        PyObject* item = PyList_GetItem(json_list, i);
        
        // Convert to string if it's not already
        PyObject* str_item = PyObject_Str(item);
        if (str_item == NULL) {
            cJSON_Delete(json_array);
            return NULL;
        }
        
        const char* json_str = PyUnicode_AsUTF8(str_item);
        if (json_str == NULL) {
            Py_DECREF(str_item);
            cJSON_Delete(json_array);
            return NULL;
        }
        
        // Parse the JSON string
        cJSON* json_obj = cJSON_Parse(json_str);
        Py_DECREF(str_item);
        
        if (json_obj == NULL) {
            PyErr_Format(PyExc_ValueError, "Invalid JSON at index %zd", i);
            cJSON_Delete(json_array);
            return NULL;
        }
        
        // Add to the array
        cJSON_AddItemToArray(json_array, json_obj);
    }
    
    // Flatten the batch
    cJSON* flattened_array;

    // Release GIL during C computation for better parallelism
    Py_BEGIN_ALLOW_THREADS

    // Initialize memory pools for optimal performance
    init_global_pools();

    flattened_array = flatten_json_batch(json_array, use_threads, num_threads);
    Py_END_ALLOW_THREADS
    
    // Free the input array (but not its contents, as they're now owned by flattened_array)
    cJSON_Delete(json_array);
    
    if (flattened_array == NULL) {
        PyErr_SetString(PyExc_ValueError, "Failed to flatten JSON batch");
        return NULL;
    }
    
    // Pre-allocate result list with known size for better performance
    int array_size = cJSON_GetArraySize(flattened_array);
    PyObject* result_list = PyList_New(array_size);
    if (result_list == NULL) {
        cJSON_Delete(flattened_array);
        return NULL;
    }

    // Add each flattened object to the result list using SET_ITEM for better performance
    for (int i = 0; i < array_size; i++) {
        cJSON* item = cJSON_GetArrayItem(flattened_array, i);
        char* item_str = pretty_print ? cJSON_Print(item) : cJSON_PrintUnformatted(item);

        if (item_str == NULL) {
            Py_DECREF(result_list);
            cJSON_Delete(flattened_array);
            PyErr_SetString(PyExc_MemoryError, "Failed to convert JSON to string");
            return NULL;
        }

        PyObject* py_item = PyUnicode_FromString(item_str);
        free(item_str);

        if (py_item == NULL) {
            Py_DECREF(result_list);
            cJSON_Delete(flattened_array);
            return NULL;
        }

        // Use SET_ITEM instead of Append for better performance (steals reference)
        PyList_SET_ITEM(result_list, i, py_item);
    }
    
    // Free the flattened array
    cJSON_Delete(flattened_array);
    
    return result_list;
}

/**
 * Generate a JSON schema from a JSON string
 */
static PyObject* py_generate_schema(PyObject* self, PyObject* args, PyObject* kwargs) {
    (void)self; // Suppress unused parameter warning
    const char* json_string;
    int use_threads = 0;
    int num_threads = 0;

    static char* kwlist[] = {"json_string", "use_threads", "num_threads", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|ii", kwlist,
                                    &json_string, &use_threads, &num_threads)) {
        return NULL;
    }

    char* result;

    // Release GIL during C computation for better parallelism
    Py_BEGIN_ALLOW_THREADS

    // Initialize memory pools for optimal performance
    init_global_pools();

    result = generate_schema_from_string(json_string, use_threads, num_threads);
    Py_END_ALLOW_THREADS

    if (result == NULL) {
        PyErr_SetString(PyExc_ValueError, "Failed to generate schema");
        return NULL;
    }

    // Convert the result to a Python string
    PyObject* py_result = PyUnicode_FromString(result);

    // Free the C string
    free(result);

    return py_result;
}

/**
 * Generate a JSON schema from a batch of JSON objects
 */
static PyObject* py_generate_schema_batch(PyObject* self, PyObject* args, PyObject* kwargs) {
    (void)self; // Suppress unused parameter warning
    PyObject* json_list;
    int use_threads = 1;
    int num_threads = 0;

    static char* kwlist[] = {"json_list", "use_threads", "num_threads", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|ii", kwlist,
                                    &json_list, &use_threads, &num_threads)) {
        return NULL;
    }
    
    // Check if the input is a list
    if (!PyList_Check(json_list)) {
        PyErr_SetString(PyExc_TypeError, "Expected a list of JSON strings");
        return NULL;
    }
    
    // Create a JSON array
    cJSON* json_array = cJSON_CreateArray();
    if (json_array == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to create JSON array");
        return NULL;
    }
    
    // Convert each Python object to a JSON object and add to the array
    Py_ssize_t list_size = PyList_Size(json_list);
    for (Py_ssize_t i = 0; i < list_size; i++) {
        PyObject* item = PyList_GetItem(json_list, i);
        
        // Convert to string if it's not already
        PyObject* str_item = PyObject_Str(item);
        if (str_item == NULL) {
            cJSON_Delete(json_array);
            return NULL;
        }
        
        const char* json_str = PyUnicode_AsUTF8(str_item);
        if (json_str == NULL) {
            Py_DECREF(str_item);
            cJSON_Delete(json_array);
            return NULL;
        }
        
        // Parse the JSON string
        cJSON* json_obj = cJSON_Parse(json_str);
        Py_DECREF(str_item);
        
        if (json_obj == NULL) {
            PyErr_Format(PyExc_ValueError, "Invalid JSON at index %zd", i);
            cJSON_Delete(json_array);
            return NULL;
        }
        
        // Add to the array
        cJSON_AddItemToArray(json_array, json_obj);
    }
    
    // Generate schema from the batch
    cJSON* schema;

    // Release GIL during C computation for better parallelism
    Py_BEGIN_ALLOW_THREADS

    // Initialize memory pools for optimal performance
    init_global_pools();

    schema = generate_schema_from_batch(json_array, use_threads, num_threads);
    Py_END_ALLOW_THREADS
    
    // Free the input array
    cJSON_Delete(json_array);
    
    if (schema == NULL) {
        PyErr_SetString(PyExc_ValueError, "Failed to generate schema");
        return NULL;
    }
    
    // Convert the result to a Python string
    char* schema_str = cJSON_Print(schema);
    cJSON_Delete(schema);
    
    if (schema_str == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to convert schema to string");
        return NULL;
    }
    
    PyObject* py_result = PyUnicode_FromString(schema_str);
    free(schema_str);
    
    return py_result;
}

/**
 * Get flattened paths with their data types from a JSON string
 */
static PyObject* py_get_flattened_paths_with_types(PyObject* self, PyObject* args, PyObject* kwargs) {
    (void)self; // Suppress unused parameter warning
    const char* json_string;
    int pretty_print = 0;

    static char* kwlist[] = {"json_string", "pretty_print", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|i", kwlist,
                                    &json_string, &pretty_print)) {
        return NULL;
    }

    char* result;

    // Release GIL during C computation
    Py_BEGIN_ALLOW_THREADS
    result = get_flattened_paths_with_types_string(json_string);
    Py_END_ALLOW_THREADS

    if (result == NULL) {
        PyErr_SetString(PyExc_ValueError, "Failed to get flattened paths with types");
        return NULL;
    }

    // Apply pretty printing if requested
    char* final_result = result;
    if (pretty_print) {
        cJSON* json = cJSON_Parse(result);
        if (json) {
            free(result);
            final_result = cJSON_Print(json);
            cJSON_Delete(json);
        }
    }

    if (final_result == NULL) {
        if (result != final_result) free(result);
        PyErr_SetString(PyExc_MemoryError, "Failed to format result");
        return NULL;
    }

    PyObject* py_result = PyUnicode_FromString(final_result);
    free(final_result);

    return py_result;
}

/**
 * Remove keys with empty string values from a JSON string
 */
static PyObject* py_remove_empty_strings(PyObject* self, PyObject* args, PyObject* kwargs) {
    (void)self; // Suppress unused parameter warning
    const char* json_string;
    int pretty_print = 0;

    static char* kwlist[] = {"json_string", "pretty_print", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|i", kwlist,
                                    &json_string, &pretty_print)) {
        return NULL;
    }

    cJSON* json;
    cJSON* filtered_json;
    char* result;

    // Release GIL during C computation for better parallelism
    Py_BEGIN_ALLOW_THREADS

    // Initialize memory pools for optimal performance
    init_global_pools();

    // Parse the JSON
    json = cJSON_Parse(json_string);
    if (!json) {
        Py_BLOCK_THREADS
        PyErr_SetString(PyExc_ValueError, "Invalid JSON input");
        return NULL;
    }

    // Apply the filter
    filtered_json = remove_empty_strings(json);
    cJSON_Delete(json);

    if (!filtered_json) {
        Py_BLOCK_THREADS
        PyErr_SetString(PyExc_ValueError, "Failed to remove empty strings");
        return NULL;
    }

    // Convert back to string
    if (pretty_print) {
        result = cJSON_Print(filtered_json);
    } else {
        result = cJSON_PrintUnformatted(filtered_json);
    }

    cJSON_Delete(filtered_json);
    Py_END_ALLOW_THREADS

    if (result == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to format result");
        return NULL;
    }

    PyObject* py_result = PyUnicode_FromString(result);
    free(result);

    return py_result;
}

/**
 * Remove keys with null values from a JSON string
 */
static PyObject* py_remove_nulls(PyObject* self, PyObject* args, PyObject* kwargs) {
    (void)self; // Suppress unused parameter warning
    const char* json_string;
    int pretty_print = 0;

    static char* kwlist[] = {"json_string", "pretty_print", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|i", kwlist,
                                    &json_string, &pretty_print)) {
        return NULL;
    }

    cJSON* json;
    cJSON* filtered_json;
    char* result;

    // Release GIL during C computation for better parallelism
    Py_BEGIN_ALLOW_THREADS

    // Initialize memory pools for optimal performance
    init_global_pools();

    // Parse the JSON
    json = cJSON_Parse(json_string);
    if (!json) {
        Py_BLOCK_THREADS
        PyErr_SetString(PyExc_ValueError, "Invalid JSON input");
        return NULL;
    }

    // Apply the filter
    filtered_json = remove_nulls(json);
    cJSON_Delete(json);

    if (!filtered_json) {
        Py_BLOCK_THREADS
        PyErr_SetString(PyExc_ValueError, "Failed to remove nulls");
        return NULL;
    }

    // Convert back to string
    if (pretty_print) {
        result = cJSON_Print(filtered_json);
    } else {
        result = cJSON_PrintUnformatted(filtered_json);
    }

    cJSON_Delete(filtered_json);
    Py_END_ALLOW_THREADS

    if (result == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to format result");
        return NULL;
    }

    PyObject* py_result = PyUnicode_FromString(result);
    free(result);

    return py_result;
}

/**
 * Replace JSON keys that match a regex pattern with a replacement string
 */
static PyObject* py_replace_keys(PyObject* self, PyObject* args, PyObject* kwargs) {
    (void)self; // Suppress unused parameter warning
    const char* json_string;
    const char* pattern;
    const char* replacement;
    int pretty_print = 0;

    static char* kwlist[] = {"json_string", "pattern", "replacement", "pretty_print", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sss|i", kwlist,
                                    &json_string, &pattern, &replacement, &pretty_print)) {
        return NULL;
    }

    cJSON* json;
    cJSON* processed_json;
    char* result;

    // Release GIL during C computation for better parallelism
    Py_BEGIN_ALLOW_THREADS

    // Initialize memory pools for optimal performance
    init_global_pools();

    // Parse the JSON
    json = cJSON_Parse(json_string);
    if (!json) {
        Py_BLOCK_THREADS
        PyErr_SetString(PyExc_ValueError, "Invalid JSON input");
        return NULL;
    }

    // Apply the key replacement
    processed_json = replace_keys(json, pattern, replacement);
    cJSON_Delete(json);

    if (!processed_json) {
        Py_BLOCK_THREADS
        PyErr_SetString(PyExc_ValueError, "Failed to replace keys (invalid regex pattern?)");
        return NULL;
    }

    // Convert back to string
    if (pretty_print) {
        result = cJSON_Print(processed_json);
    } else {
        result = cJSON_PrintUnformatted(processed_json);
    }

    cJSON_Delete(processed_json);
    Py_END_ALLOW_THREADS

    if (result == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to format result");
        return NULL;
    }

    PyObject* py_result = PyUnicode_FromString(result);
    free(result);

    return py_result;
}

/**
 * Replace JSON string values that match a regex pattern with a replacement string
 */
static PyObject* py_replace_values(PyObject* self, PyObject* args, PyObject* kwargs) {
    (void)self; // Suppress unused parameter warning
    const char* json_string;
    const char* pattern;
    const char* replacement;
    int pretty_print = 0;

    static char* kwlist[] = {"json_string", "pattern", "replacement", "pretty_print", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sss|i", kwlist,
                                    &json_string, &pattern, &replacement, &pretty_print)) {
        return NULL;
    }

    cJSON* json;
    cJSON* processed_json;
    char* result;

    // Release GIL during C computation for better parallelism
    Py_BEGIN_ALLOW_THREADS

    // Initialize memory pools for optimal performance
    init_global_pools();

    // Parse the JSON
    json = cJSON_Parse(json_string);
    if (!json) {
        Py_BLOCK_THREADS
        PyErr_SetString(PyExc_ValueError, "Invalid JSON input");
        return NULL;
    }

    // Apply the value replacement
    processed_json = replace_values(json, pattern, replacement);
    cJSON_Delete(json);

    if (!processed_json) {
        Py_BLOCK_THREADS
        PyErr_SetString(PyExc_ValueError, "Failed to replace values (invalid regex pattern?)");
        return NULL;
    }

    // Convert back to string
    if (pretty_print) {
        result = cJSON_Print(processed_json);
    } else {
        result = cJSON_PrintUnformatted(processed_json);
    }

    cJSON_Delete(processed_json);
    Py_END_ALLOW_THREADS

    if (result == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to format result");
        return NULL;
    }

    PyObject* py_result = PyUnicode_FromString(result);
    free(result);

    return py_result;
}

// JsonToolsBuilder Python bindings
static PyObject* py_json_tools_builder_execute(PyObject* self, PyObject* args) {
    const char* json_string;
    PyObject* operations_list;
    int pretty_print = 0;

    if (!PyArg_ParseTuple(args, "sO|i", &json_string, &operations_list, &pretty_print)) {
        return NULL;
    }

    if (!PyList_Check(operations_list)) {
        PyErr_SetString(PyExc_TypeError, "operations must be a list");
        return NULL;
    }

    // Create builder with error checking
    JsonToolsBuilder* builder = json_tools_builder_create();
    if (!builder) {
        PyErr_SetString(PyExc_MemoryError, "Failed to create JsonToolsBuilder");
        return NULL;
    }

    // Verify builder was created properly
    if (builder->operations == NULL) {
        json_tools_builder_destroy(builder);
        PyErr_SetString(PyExc_MemoryError, "Builder operations array not initialized");
        return NULL;
    }

    // Add JSON data
    json_tools_builder_add_json(builder, json_string);
    if (json_tools_builder_has_error(builder)) {
        const char* error = json_tools_builder_get_error(builder);
        json_tools_builder_destroy(builder);
        PyErr_SetString(PyExc_ValueError, error ? error : "Invalid JSON string");
        return NULL;
    }

    // Add operations
    Py_ssize_t num_operations = PyList_Size(operations_list);
    for (Py_ssize_t i = 0; i < num_operations; i++) {
        PyObject* op = PyList_GetItem(operations_list, i);
        if (!PyDict_Check(op)) {
            json_tools_builder_destroy(builder);
            PyErr_SetString(PyExc_TypeError, "Each operation must be a dictionary");
            return NULL;
        }

        PyObject* type_obj = PyDict_GetItemString(op, "type");
        if (!type_obj || !PyUnicode_Check(type_obj)) {
            json_tools_builder_destroy(builder);
            PyErr_SetString(PyExc_ValueError, "Operation must have a 'type' string field");
            return NULL;
        }

        const char* op_type = PyUnicode_AsUTF8(type_obj);

        if (strcmp(op_type, "remove_empty_strings") == 0) {
            JsonToolsBuilder* result = json_tools_builder_remove_empty_strings(builder);
            if (!result) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_RuntimeError, "Failed to add remove_empty_strings operation");
                return NULL;
            }
        } else if (strcmp(op_type, "remove_nulls") == 0) {
            JsonToolsBuilder* result = json_tools_builder_remove_nulls(builder);
            if (!result) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_RuntimeError, "Failed to add remove_nulls operation");
                return NULL;
            }
        } else if (strcmp(op_type, "replace_keys") == 0) {
            PyObject* pattern_obj = PyDict_GetItemString(op, "pattern");
            PyObject* replacement_obj = PyDict_GetItemString(op, "replacement");

            if (!pattern_obj || !PyUnicode_Check(pattern_obj) ||
                !replacement_obj || !PyUnicode_Check(replacement_obj)) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_ValueError, "replace_keys operation requires 'pattern' and 'replacement' strings");
                return NULL;
            }

            const char* pattern = PyUnicode_AsUTF8(pattern_obj);
            const char* replacement = PyUnicode_AsUTF8(replacement_obj);

            if (!pattern || !replacement) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_ValueError, "Failed to convert pattern or replacement to UTF-8");
                return NULL;
            }

            // SAFETY: Validate pattern before passing to C
            size_t pattern_len = strlen(pattern);
            size_t replacement_len = strlen(replacement);
            if (pattern_len == 0 || pattern_len > 512 || replacement_len > 1024) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_ValueError, "Pattern or replacement string too long or empty");
                return NULL;
            }

            // Check for potentially problematic characters
            for (size_t i = 0; i < pattern_len; i++) {
                if (pattern[i] == '\0') {
                    json_tools_builder_destroy(builder);
                    PyErr_SetString(PyExc_ValueError, "Pattern contains null character");
                    return NULL;
                }
            }

            JsonToolsBuilder* result = json_tools_builder_replace_keys(builder, pattern, replacement);
            if (!result) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_RuntimeError, "Failed to add replace_keys operation");
                return NULL;
            }
        } else if (strcmp(op_type, "replace_values") == 0) {
            PyObject* pattern_obj = PyDict_GetItemString(op, "pattern");
            PyObject* replacement_obj = PyDict_GetItemString(op, "replacement");

            if (!pattern_obj || !PyUnicode_Check(pattern_obj) ||
                !replacement_obj || !PyUnicode_Check(replacement_obj)) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_ValueError, "replace_values operation requires 'pattern' and 'replacement' strings");
                return NULL;
            }

            const char* pattern = PyUnicode_AsUTF8(pattern_obj);
            const char* replacement = PyUnicode_AsUTF8(replacement_obj);

            if (!pattern || !replacement) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_ValueError, "Failed to convert pattern or replacement to UTF-8");
                return NULL;
            }

            // SAFETY: Validate pattern before passing to C
            size_t pattern_len = strlen(pattern);
            size_t replacement_len = strlen(replacement);
            if (pattern_len == 0 || pattern_len > 512 || replacement_len > 1024) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_ValueError, "Pattern or replacement string too long or empty");
                return NULL;
            }

            // Check for potentially problematic characters
            for (size_t i = 0; i < pattern_len; i++) {
                if (pattern[i] == '\0') {
                    json_tools_builder_destroy(builder);
                    PyErr_SetString(PyExc_ValueError, "Pattern contains null character");
                    return NULL;
                }
            }

            JsonToolsBuilder* result = json_tools_builder_replace_values(builder, pattern, replacement);
            if (!result) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_RuntimeError, "Failed to add replace_values operation");
                return NULL;
            }
        } else if (strcmp(op_type, "flatten") == 0) {
            JsonToolsBuilder* result = json_tools_builder_flatten(builder);
            if (!result) {
                json_tools_builder_destroy(builder);
                PyErr_SetString(PyExc_RuntimeError, "Failed to add flatten operation");
                return NULL;
            }
        } else {
            json_tools_builder_destroy(builder);
            PyErr_Format(PyExc_ValueError, "Unknown operation type: %s", op_type);
            return NULL;
        }
    }

    // Set pretty print option
    json_tools_builder_pretty_print(builder, pretty_print);

    // Execute and get result
    char* result = json_tools_builder_build(builder);
    if (!result) {
        const char* error = json_tools_builder_get_error(builder);
        json_tools_builder_destroy(builder);
        PyErr_SetString(PyExc_RuntimeError, error ? error : "Failed to execute operations");
        return NULL;
    }

    PyObject* py_result = PyUnicode_FromString(result);
    free(result);
    json_tools_builder_destroy(builder);

    return py_result;
}


// Module method definitions with proper function signatures
static PyMethodDef CJsonToolsMethods[] = {
    {"flatten_json", (PyCFunction)(void(*)(void))py_flatten_json, METH_VARARGS | METH_KEYWORDS,
     "Flatten a JSON string into a flat structure. Args: json_string, use_threads=False, num_threads=0, pretty_print=False"},
    {"flatten_json_batch", (PyCFunction)(void(*)(void))py_flatten_json_batch, METH_VARARGS | METH_KEYWORDS,
     "Flatten a batch of JSON objects into flat structures. Args: json_list, use_threads=True, num_threads=0, pretty_print=False"},
    {"generate_schema", (PyCFunction)(void(*)(void))py_generate_schema, METH_VARARGS | METH_KEYWORDS,
     "Generate a JSON schema from a JSON string."},
    {"generate_schema_batch", (PyCFunction)(void(*)(void))py_generate_schema_batch, METH_VARARGS | METH_KEYWORDS,
     "Generate a JSON schema from a batch of JSON objects."},
    {"get_flattened_paths_with_types", (PyCFunction)(void(*)(void))py_get_flattened_paths_with_types, METH_VARARGS | METH_KEYWORDS,
     "Get flattened paths with their data types from a JSON string. Args: json_string, pretty_print=False"},
    {"remove_empty_strings", (PyCFunction)(void(*)(void))py_remove_empty_strings, METH_VARARGS | METH_KEYWORDS,
     "Remove keys with empty string values from a JSON string. Args: json_string, pretty_print=False"},
    {"remove_nulls", (PyCFunction)(void(*)(void))py_remove_nulls, METH_VARARGS | METH_KEYWORDS,
     "Remove keys with null values from a JSON string. Args: json_string, pretty_print=False"},
    {"replace_keys", (PyCFunction)(void(*)(void))py_replace_keys, METH_VARARGS | METH_KEYWORDS,
     "Replace JSON keys matching a regex pattern. Args: json_string, pattern, replacement, pretty_print=False"},
    {"replace_values", (PyCFunction)(void(*)(void))py_replace_values, METH_VARARGS | METH_KEYWORDS,
     "Replace JSON string values matching a regex pattern. Args: json_string, pattern, replacement, pretty_print=False"},
    {"_builder_execute", (PyCFunction)(void(*)(void))py_json_tools_builder_execute, METH_VARARGS,
     "Execute JsonToolsBuilder operations in single pass. Args: json_string, operations_list, pretty_print=False"},
    {NULL, NULL, 0, NULL}  // Sentinel
};

// Module definition with proper initialization
static struct PyModuleDef cjsontoolsmodule = {
    PyModuleDef_HEAD_INIT,
    "_cjson_tools",   // Module name
    "Python bindings for the cJSON-Tools C library",  // Module docstring
    -1,       // Size of per-interpreter state or -1
    CJsonToolsMethods,
    NULL,     // m_slots
    NULL,     // m_traverse
    NULL,     // m_clear
    NULL      // m_free
};

// Module initialization function
PyMODINIT_FUNC PyInit__cjson_tools(void) {
    PyObject* m = PyModule_Create(&cjsontoolsmodule);
    if (m == NULL) {
        return NULL;
    }

    // Add version
    PyModule_AddStringConstant(m, "__version__", MODULE_VERSION);

    return m;
}