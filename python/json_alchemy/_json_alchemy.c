#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <cjson/cJSON.h>
#include "../../include/json_flattener.h"
#include "../../include/json_schema_generator.h"
#include "../../include/json_utils.h"

#define MODULE_VERSION "1.3.1"

/**
 * Flatten a JSON string
 */
static PyObject* py_flatten_json(PyObject* self, PyObject* args, PyObject* kwargs) {
    const char* json_string;
    int use_threads = 0;
    int num_threads = 0;
    
    static char* kwlist[] = {"json_string", "use_threads", "num_threads", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|ii", kwlist, 
                                    &json_string, &use_threads, &num_threads)) {
        return NULL;
    }
    
    // Call the C function
    char* result = flatten_json_string(json_string, use_threads, num_threads);
    
    if (result == NULL) {
        PyErr_SetString(PyExc_ValueError, "Failed to flatten JSON");
        return NULL;
    }
    
    // Convert the result to a Python string
    PyObject* py_result = PyUnicode_FromString(result);
    
    // Free the C string
    free(result);
    
    return py_result;
}

/**
 * Flatten a batch of JSON objects
 */
static PyObject* py_flatten_json_batch(PyObject* self, PyObject* args, PyObject* kwargs) {
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
    
    // Flatten the batch
    cJSON* flattened_array = flatten_json_batch(json_array, use_threads, num_threads);
    
    // Free the input array (but not its contents, as they're now owned by flattened_array)
    cJSON_Delete(json_array);
    
    if (flattened_array == NULL) {
        PyErr_SetString(PyExc_ValueError, "Failed to flatten JSON batch");
        return NULL;
    }
    
    // Convert the result to a Python list
    PyObject* result_list = PyList_New(0);
    if (result_list == NULL) {
        cJSON_Delete(flattened_array);
        return NULL;
    }
    
    // Add each flattened object to the result list
    int array_size = cJSON_GetArraySize(flattened_array);
    for (int i = 0; i < array_size; i++) {
        cJSON* item = cJSON_GetArrayItem(flattened_array, i);
        char* item_str = cJSON_Print(item);
        
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
        
        if (PyList_Append(result_list, py_item) < 0) {
            Py_DECREF(py_item);
            Py_DECREF(result_list);
            cJSON_Delete(flattened_array);
            return NULL;
        }
        
        Py_DECREF(py_item);
    }
    
    // Free the flattened array
    cJSON_Delete(flattened_array);
    
    return result_list;
}

/**
 * Generate a JSON schema from a JSON string
 */
static PyObject* py_generate_schema(PyObject* self, PyObject* args, PyObject* kwargs) {
    const char* json_string;
    int use_threads = 0;
    int num_threads = 0;
    
    static char* kwlist[] = {"json_string", "use_threads", "num_threads", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|ii", kwlist, 
                                    &json_string, &use_threads, &num_threads)) {
        return NULL;
    }
    
    // Call the C function
    char* result = generate_schema_from_string(json_string, use_threads, num_threads);
    
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
    cJSON* schema = generate_schema_from_batch(json_array, use_threads, num_threads);
    
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

// Module method definitions
static PyMethodDef JsonAlchemyMethods[] = {
    {"flatten_json", (PyCFunction)py_flatten_json, METH_VARARGS | METH_KEYWORDS,
     "Flatten a JSON string into a flat structure."},
    {"flatten_json_batch", (PyCFunction)py_flatten_json_batch, METH_VARARGS | METH_KEYWORDS,
     "Flatten a batch of JSON objects into flat structures."},
    {"generate_schema", (PyCFunction)py_generate_schema, METH_VARARGS | METH_KEYWORDS,
     "Generate a JSON schema from a JSON string."},
    {"generate_schema_batch", (PyCFunction)py_generate_schema_batch, METH_VARARGS | METH_KEYWORDS,
     "Generate a JSON schema from a batch of JSON objects."},
    {NULL, NULL, 0, NULL}  // Sentinel
};

// Module definition
static struct PyModuleDef jsonalchemymodule = {
    PyModuleDef_HEAD_INIT,
    "_json_alchemy",   // Module name
    "Python bindings for the JSON Alchemy C library",  // Module docstring
    -1,       // Size of per-interpreter state or -1
    JsonAlchemyMethods
};

// Module initialization function
PyMODINIT_FUNC PyInit__json_alchemy(void) {
    PyObject* m = PyModule_Create(&jsonalchemymodule);
    if (m == NULL) {
        return NULL;
    }
    
    // Add version
    PyModule_AddStringConstant(m, "__version__", MODULE_VERSION);
    
    return m;
}