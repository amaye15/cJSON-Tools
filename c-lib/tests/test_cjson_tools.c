#include <string.h>
#include "cjson_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

// Test framework macros
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Global test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Test assertion macros
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        printf(ANSI_COLOR_GREEN "✓ PASS" ANSI_COLOR_RESET ": %s\n", message); \
        tests_passed++; \
    } else { \
        printf(ANSI_COLOR_RED "✗ FAIL" ANSI_COLOR_RESET ": %s\n", message); \
        tests_failed++; \
    } \
} while(0)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_STRING_EQUAL(expected, actual, message) \
    TEST_ASSERT(strcmp((expected), (actual)) == 0, message)

// Test section header
#define TEST_SECTION(name) do { \
    printf("\n" ANSI_COLOR_BLUE "=== %s ===" ANSI_COLOR_RESET "\n", name); \
} while(0)

// Performance timing
static clock_t global_start_time;
#define TIME_START() global_start_time = clock()
#define TIME_END(operation) do { \
    clock_t end_time = clock(); \
    double cpu_time = ((double)(end_time - global_start_time)) / CLOCKS_PER_SEC; \
    printf(ANSI_COLOR_YELLOW "⏱  %s took %.6f seconds" ANSI_COLOR_RESET "\n", operation, cpu_time); \
} while(0)

// =============================================================================
// TEST DATA GENERATION
// =============================================================================

// Generate test JSON objects
static char* create_test_object_json() {
    return my_strdup(
        "{"
        "\"name\":\"John Doe\","
        "\"age\":30,"
        "\"email\":\"john@example.com\","
        "\"active\":true,"
        "\"score\":95.5,"
        "\"metadata\":null,"
        "\"address\":{"
            "\"street\":\"123 Main St\","
            "\"city\":\"Anytown\","
            "\"zip\":\"12345\","
            "\"coordinates\":{\"lat\":40.7128,\"lng\":-74.0060}"
        "},"
        "\"tags\":[\"developer\",\"tester\",\"admin\"],"
        "\"preferences\":{"
            "\"notifications\":true,"
            "\"theme\":\"dark\","
            "\"languages\":[\"en\",\"es\"]"
        "}"
        "}"
    );
}

static char* create_test_array_json() {
    return my_strdup(
        "["
        "{"
            "\"id\":1,"
            "\"name\":\"Alice\","
            "\"active\":true,"
            "\"score\":85.5"
        "},"
        "{"
            "\"id\":2,"
            "\"name\":\"Bob\","
            "\"active\":false,"
            "\"score\":null,"
            "\"tags\":[\"admin\"]"
        "},"
        "{"
            "\"id\":3,"
            "\"name\":\"Charlie\","
            "\"active\":true,"
            "\"score\":92.0,"
            "\"tags\":[\"user\",\"premium\"],"
            "\"metadata\":{"
                "\"created\":\"2023-01-01\","
                "\"updated\":\"2023-12-01\""
            "}"
        "}"
        "]"
    );
}

static char* create_large_test_json(int num_objects) {
    cJSON* root = cJSON_CreateArray();
    
    for (int i = 0; i < num_objects; i++) {
        cJSON* obj = cJSON_CreateObject();
        
        // Add basic fields
        cJSON_AddNumberToObject(obj, "id", i);
        cJSON_AddStringToObject(obj, "name", "Test User");
        cJSON_AddBoolToObject(obj, "active", i % 2 == 0);
        cJSON_AddNumberToObject(obj, "score", 50.0 + (i % 50));
        
        // Add nested object
        cJSON* metadata = cJSON_CreateObject();
        cJSON_AddStringToObject(metadata, "type", "test");
        cJSON_AddNumberToObject(metadata, "version", 1);
        cJSON_AddItemToObject(obj, "metadata", metadata);
        
        // Add array
        cJSON* tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("tag1"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("tag2"));
        cJSON_AddItemToObject(obj, "tags", tags);
        
        cJSON_AddItemToArray(root, obj);
    }
    
    char* json_string = cJSON_Print(root);
    cJSON_Delete(root);
    return json_string;
}

// =============================================================================
// BASIC FUNCTIONALITY TESTS
// =============================================================================

void test_memory_pools() {
    TEST_SECTION("Memory Pool Tests");
    
    // Test global pool initialization
    init_global_pools();
    TEST_ASSERT_NOT_NULL(g_cjson_node_pool, "Global cJSON node pool initialized");
    TEST_ASSERT_NOT_NULL(g_property_node_pool, "Global property node pool initialized");
    TEST_ASSERT_NOT_NULL(g_task_pool, "Global task pool initialized");
    
    // Test custom slab allocator
    SlabAllocator* allocator = slab_allocator_create(64, 100);
    TEST_ASSERT_NOT_NULL(allocator, "Custom slab allocator created");
    
    // Test allocation and deallocation
    void* ptr1 = slab_alloc(allocator);
    void* ptr2 = slab_alloc(allocator);
    TEST_ASSERT_NOT_NULL(ptr1, "First allocation successful");
    TEST_ASSERT_NOT_NULL(ptr2, "Second allocation successful");
    TEST_ASSERT(ptr1 != ptr2, "Allocated pointers are different");
    
    slab_free(allocator, ptr1);
    slab_free(allocator, ptr2);
    
    // Test reallocation of freed memory
    void* ptr3 = slab_alloc(allocator);
    TEST_ASSERT_NOT_NULL(ptr3, "Reallocation after free successful");
    
    slab_free(allocator, ptr3);
    slab_allocator_destroy(allocator);
    
    cleanup_global_pools();
}

void test_string_utilities() {
    TEST_SECTION("String Utility Tests");
    
    // Test custom strdup
    const char* test_str = "Hello, World!";
    char* dup_str = my_strdup(test_str);
    TEST_ASSERT_NOT_NULL(dup_str, "String duplication successful");
    TEST_ASSERT_STRING_EQUAL(test_str, dup_str, "Duplicated string matches original");
    free(dup_str);
    
    // Test SIMD strlen
    size_t len = strlen_simd(test_str);
    TEST_ASSERT_EQUAL(13, len, "SIMD strlen returns correct length");
    
    // Test string view functionality
    StringView sv = make_string_view_cstr(test_str);
    TEST_ASSERT_EQUAL(13, sv.length, "String view length correct");
    TEST_ASSERT(string_view_equals_cstr(&sv, test_str), "String view equals original");
    
    // Test string view operations
    TEST_ASSERT(string_view_starts_with(&sv, "Hello"), "String view starts_with works");
    TEST_ASSERT(string_view_ends_with(&sv, "World!"), "String view ends_with works");
    
    char* cstr = string_view_to_cstr(&sv);
    TEST_ASSERT_STRING_EQUAL(test_str, cstr, "String view to C string conversion");
    free(cstr);
}

void test_cpu_detection() {
    TEST_SECTION("CPU Detection Tests");
    
    int num_cores = get_num_cores();
    TEST_ASSERT(num_cores > 0, "CPU core detection returns positive number");
    printf("Detected %d CPU cores\n", num_cores);
    
    int optimal_threads = get_optimal_threads(0);
    TEST_ASSERT(optimal_threads > 0, "Optimal thread calculation returns positive number");
    printf("Optimal thread count: %d\n", optimal_threads);
    
    int requested_threads = get_optimal_threads(4);
    TEST_ASSERT_EQUAL(4, requested_threads, "Requested thread count respected");
}

// =============================================================================
// JSON PROCESSING TESTS
// =============================================================================

void test_json_flattening() {
    TEST_SECTION("JSON Flattening Tests");
    
    // Test single object flattening
    char* test_json = create_test_object_json();
    cJSON* json = cJSON_Parse(test_json);
    TEST_ASSERT_NOT_NULL(json, "Test JSON parsed successfully");
    
    TIME_START();
    cJSON* flattened = flatten_json_object(json);
    TIME_END("Single object flattening");
    
    TEST_ASSERT_NOT_NULL(flattened, "JSON object flattened successfully");
    
    // Check some expected flattened keys
    cJSON* name = cJSON_GetObjectItem(flattened, "name");
    cJSON* address_street = cJSON_GetObjectItem(flattened, "address.street");
    cJSON* coord_lat = cJSON_GetObjectItem(flattened, "address.coordinates.lat");
    cJSON* tag0 = cJSON_GetObjectItem(flattened, "tags[0]");
    
    TEST_ASSERT_NOT_NULL(name, "Flattened 'name' field exists");
    TEST_ASSERT_NOT_NULL(address_street, "Flattened 'address.street' field exists");
    TEST_ASSERT_NOT_NULL(coord_lat, "Flattened 'address.coordinates.lat' field exists");
    TEST_ASSERT_NOT_NULL(tag0, "Flattened 'tags[0]' field exists");
    
    if (name) {
        TEST_ASSERT_STRING_EQUAL("John Doe", cJSON_GetStringValue(name), "Flattened name value correct");
    }
    if (address_street) {
        TEST_ASSERT_STRING_EQUAL("123 Main St", cJSON_GetStringValue(address_street), "Flattened address.street value correct");
    }
    
    cJSON_Delete(flattened);
    cJSON_Delete(json);
    free(test_json);
    
    // Test array flattening
    char* array_json = create_test_array_json();
    cJSON* json_array = cJSON_Parse(array_json);
    TEST_ASSERT_NOT_NULL(json_array, "Test JSON array parsed successfully");
    
    TIME_START();
    cJSON* flattened_array = flatten_json_batch(json_array, 0, 0);
    TIME_END("Array flattening (single-threaded)");
    
    TEST_ASSERT_NOT_NULL(flattened_array, "JSON array flattened successfully");
    TEST_ASSERT(cJSON_IsArray(flattened_array), "Flattened result is an array");
    TEST_ASSERT_EQUAL(3, cJSON_GetArraySize(flattened_array), "Flattened array has correct size");
    
    cJSON_Delete(flattened_array);
    
    // Test multi-threaded flattening
    TIME_START();
    cJSON* flattened_mt = flatten_json_batch(json_array, 1, 2);
    TIME_END("Array flattening (multi-threaded)");
    
    TEST_ASSERT_NOT_NULL(flattened_mt, "Multi-threaded JSON array flattened successfully");
    TEST_ASSERT_EQUAL(3, cJSON_GetArraySize(flattened_mt), "Multi-threaded flattened array has correct size");
    
    cJSON_Delete(flattened_mt);
    cJSON_Delete(json_array);
    free(array_json);
    
    // Test string interface
    char* test_string = create_test_object_json();
    TIME_START();
    char* flattened_string = flatten_json_string(test_string, 0, 0);
    TIME_END("String flattening");
    
    TEST_ASSERT_NOT_NULL(flattened_string, "JSON string flattened successfully");
    
    // Verify it's valid JSON
    cJSON* parsed_back = cJSON_Parse(flattened_string);
    TEST_ASSERT_NOT_NULL(parsed_back, "Flattened string is valid JSON");
    
    if (parsed_back) {
        cJSON_Delete(parsed_back);
    }
    
    free(flattened_string);
    free(test_string);
}

void test_json_schema_generation() {
    TEST_SECTION("JSON Schema Generation Tests");
    
    // Test single object schema generation
    char* test_json = create_test_object_json();
    cJSON* json = cJSON_Parse(test_json);
    TEST_ASSERT_NOT_NULL(json, "Test JSON parsed for schema generation");
    
    TIME_START();
    cJSON* schema = generate_schema_from_object(json);
    TIME_END("Single object schema generation");
    
    TEST_ASSERT_NOT_NULL(schema, "Schema generated successfully");
    
    // Check schema structure
    cJSON* schema_field = cJSON_GetObjectItem(schema, "$schema");
    cJSON* type_field = cJSON_GetObjectItem(schema, "type");
    cJSON* properties = cJSON_GetObjectItem(schema, "properties");
    
    TEST_ASSERT_NOT_NULL(schema_field, "Schema has $schema field");
    TEST_ASSERT_NOT_NULL(type_field, "Schema has type field");
    TEST_ASSERT_NOT_NULL(properties, "Schema has properties field");
    
    if (type_field) {
        TEST_ASSERT_STRING_EQUAL("object", cJSON_GetStringValue(type_field), "Root type is object");
    }
    
    // Check some specific properties
    if (properties) {
        cJSON* name_prop = cJSON_GetObjectItem(properties, "name");
        cJSON* age_prop = cJSON_GetObjectItem(properties, "age");
        cJSON* address_prop = cJSON_GetObjectItem(properties, "address");
        
        TEST_ASSERT_NOT_NULL(name_prop, "Schema has name property");
        TEST_ASSERT_NOT_NULL(age_prop, "Schema has age property");
        TEST_ASSERT_NOT_NULL(address_prop, "Schema has address property");
        
        if (name_prop) {
            cJSON* name_type = cJSON_GetObjectItem(name_prop, "type");
            if (name_type) {
                TEST_ASSERT_STRING_EQUAL("string", cJSON_GetStringValue(name_type), "Name property type is string");
            }
        }
        
        if (age_prop) {
            cJSON* age_type = cJSON_GetObjectItem(age_prop, "type");
            if (age_type) {
                TEST_ASSERT_STRING_EQUAL("integer", cJSON_GetStringValue(age_type), "Age property type is integer");
            }
        }
    }
    
    cJSON_Delete(schema);
    cJSON_Delete(json);
    free(test_json);
    
    // Test batch schema generation
    char* array_json = create_test_array_json();
    cJSON* json_array = cJSON_Parse(array_json);
    TEST_ASSERT_NOT_NULL(json_array, "Test JSON array parsed for schema generation");
    
    TIME_START();
    cJSON* batch_schema = generate_schema_from_batch(json_array, 0, 0);
    TIME_END("Batch schema generation (single-threaded)");
    
    TEST_ASSERT_NOT_NULL(batch_schema, "Batch schema generated successfully");
    
    cJSON_Delete(batch_schema);
    
    // Test multi-threaded schema generation
    TIME_START();
    cJSON* mt_schema = generate_schema_from_batch(json_array, 1, 2);
    TIME_END("Batch schema generation (multi-threaded)");
    
    TEST_ASSERT_NOT_NULL(mt_schema, "Multi-threaded batch schema generated successfully");
    
    cJSON_Delete(mt_schema);
    cJSON_Delete(json_array);
    free(array_json);
    
    // Test string interface
    char* test_string = create_test_object_json();
    TIME_START();
    char* schema_string = generate_schema_from_string(test_string, 0, 0);
    TIME_END("String schema generation");
    
    TEST_ASSERT_NOT_NULL(schema_string, "Schema string generated successfully");
    
    // Verify it's valid JSON
    cJSON* parsed_schema = cJSON_Parse(schema_string);
    TEST_ASSERT_NOT_NULL(parsed_schema, "Generated schema string is valid JSON");
    
    if (parsed_schema) {
        cJSON_Delete(parsed_schema);
    }
    
    free(schema_string);
    free(test_string);
}

void test_path_extraction() {
    TEST_SECTION("Path Extraction Tests");
    
    char* test_json = create_test_object_json();
    cJSON* json = cJSON_Parse(test_json);
    TEST_ASSERT_NOT_NULL(json, "Test JSON parsed for path extraction");
    
    TIME_START();
    cJSON* paths = get_flattened_paths_with_types(json);
    TIME_END("Path extraction");
    
    TEST_ASSERT_NOT_NULL(paths, "Paths extracted successfully");
    
    // Check some expected paths
    cJSON* name_type = cJSON_GetObjectItem(paths, "name");
    cJSON* age_type = cJSON_GetObjectItem(paths, "age");
    cJSON* active_type = cJSON_GetObjectItem(paths, "active");
    cJSON* score_type = cJSON_GetObjectItem(paths, "score");
    cJSON* address_street_type = cJSON_GetObjectItem(paths, "address.street");
    cJSON* tag0_type = cJSON_GetObjectItem(paths, "tags[0]");
    
    TEST_ASSERT_NOT_NULL(name_type, "Path 'name' found");
    TEST_ASSERT_NOT_NULL(age_type, "Path 'age' found");
    TEST_ASSERT_NOT_NULL(active_type, "Path 'active' found");
    TEST_ASSERT_NOT_NULL(score_type, "Path 'score' found");
    TEST_ASSERT_NOT_NULL(address_street_type, "Path 'address.street' found");
    TEST_ASSERT_NOT_NULL(tag0_type, "Path 'tags[0]' found");
    
    if (name_type) {
        TEST_ASSERT_STRING_EQUAL("string", cJSON_GetStringValue(name_type), "Name type is string");
    }
    if (age_type) {
        TEST_ASSERT_STRING_EQUAL("integer", cJSON_GetStringValue(age_type), "Age type is integer");
    }
    if (active_type) {
        TEST_ASSERT_STRING_EQUAL("boolean", cJSON_GetStringValue(active_type), "Active type is boolean");
    }
    if (score_type) {
        TEST_ASSERT_STRING_EQUAL("number", cJSON_GetStringValue(score_type), "Score type is number");
    }
    
    cJSON_Delete(paths);
    cJSON_Delete(json);
    free(test_json);
    
    // Test string interface
    char* test_string = create_test_object_json();
    TIME_START();
    char* paths_string = get_flattened_paths_with_types_string(test_string);
    TIME_END("Path extraction (string interface)");
    
    TEST_ASSERT_NOT_NULL(paths_string, "Paths string generated successfully");
    
    // Verify it's valid JSON
    cJSON* parsed_paths = cJSON_Parse(paths_string);
    TEST_ASSERT_NOT_NULL(parsed_paths, "Generated paths string is valid JSON");
    
    if (parsed_paths) {
        cJSON_Delete(parsed_paths);
    }
    
    free(paths_string);
    free(test_string);
}

// =============================================================================
// UTILITY FUNCTION TESTS
// =============================================================================

void test_json_utilities() {
    TEST_SECTION("JSON Utility Function Tests");
    
    // Create test JSON with empty strings and nulls
    cJSON* test_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(test_obj, "name", "John");
    cJSON_AddStringToObject(test_obj, "empty", "");
    cJSON_AddNullToObject(test_obj, "null_field");
    cJSON_AddNumberToObject(test_obj, "age", 30);
    cJSON_AddStringToObject(test_obj, "another_empty", "");
    
    // Test remove_empty_strings
    TIME_START();
    cJSON* no_empty = remove_empty_strings(test_obj);
    TIME_END("Remove empty strings");
    
    TEST_ASSERT_NOT_NULL(no_empty, "Empty strings removed successfully");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(no_empty, "name"), "Non-empty string preserved");
    TEST_ASSERT_NULL(cJSON_GetObjectItem(no_empty, "empty"), "Empty string removed");
    TEST_ASSERT_NULL(cJSON_GetObjectItem(no_empty, "another_empty"), "Another empty string removed");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(no_empty, "null_field"), "Null field preserved when removing empty strings");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(no_empty, "age"), "Number field preserved");
    
    cJSON_Delete(no_empty);
    
    // Test remove_nulls
    TIME_START();
    cJSON* no_nulls = remove_nulls(test_obj);
    TIME_END("Remove nulls");
    
    TEST_ASSERT_NOT_NULL(no_nulls, "Nulls removed successfully");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(no_nulls, "name"), "String field preserved");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(no_nulls, "empty"), "Empty string preserved when removing nulls");
    TEST_ASSERT_NULL(cJSON_GetObjectItem(no_nulls, "null_field"), "Null field removed");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(no_nulls, "age"), "Number field preserved");
    
    cJSON_Delete(no_nulls);
    cJSON_Delete(test_obj);
    
#ifndef __WINDOWS__
    // Test regex replacements (Unix only)
    cJSON* regex_test = cJSON_CreateObject();
    cJSON_AddStringToObject(regex_test, "old_name", "value1");
    cJSON_AddStringToObject(regex_test, "new_name", "value2");
    cJSON_AddStringToObject(regex_test, "old_value", "old_data");
    
    // Test replace_keys
    TIME_START();
    cJSON* replaced_keys = replace_keys(regex_test, "^old_.*", "replaced");
    TIME_END("Replace keys (regex)");
    
    TEST_ASSERT_NOT_NULL(replaced_keys, "Keys replaced successfully");
    if (replaced_keys) {
        TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(replaced_keys, "replaced"), "Old key replaced");
        TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(replaced_keys, "new_name"), "Non-matching key preserved");
        cJSON_Delete(replaced_keys);
    }
    
    // Test replace_values
    TIME_START();
    cJSON* replaced_values = replace_values(regex_test, "^old_.*", "new_data");
    TIME_END("Replace values (regex)");
    
    TEST_ASSERT_NOT_NULL(replaced_values, "Values replaced successfully");
    if (replaced_values) {
        cJSON* old_value_field = cJSON_GetObjectItem(replaced_values, "old_value");
        if (old_value_field) {
            TEST_ASSERT_STRING_EQUAL("new_data", cJSON_GetStringValue(old_value_field), "Matching value replaced");
        }
        cJSON_Delete(replaced_values);
    }
    
    cJSON_Delete(regex_test);
#else
    printf(ANSI_COLOR_YELLOW "ℹ  Regex tests skipped on Windows platform" ANSI_COLOR_RESET "\n");
#endif
}

// =============================================================================
// THREADING TESTS
// =============================================================================

// Simple task function for threading tests
static void increment_task(void* arg) {
    volatile int* cnt = (volatile int*)arg;
    (*cnt)++;
}

void test_threading() {
    TEST_SECTION("Threading Tests");

#ifndef THREADING_DISABLED
    // Test thread pool creation
    TIME_START();
    ThreadPool* pool = thread_pool_create(4);
    TIME_END("Thread pool creation");

    TEST_ASSERT_NOT_NULL(pool, "Thread pool created successfully");

    if (pool) {
        TEST_ASSERT_EQUAL(4, thread_pool_get_thread_count(pool), "Thread pool has correct thread count");

        // Test task counter
        volatile int counter = 0;
        
        // Add multiple tasks
        TIME_START();
        for (int i = 0; i < 10; i++) {
            int result = thread_pool_add_task(pool, increment_task, (void*)&counter);
            TEST_ASSERT_EQUAL(0, result, "Task added successfully");
        }
        TIME_END("Adding 10 tasks");
        
        // Wait for completion
        TIME_START();
        thread_pool_wait(pool);
        TIME_END("Waiting for task completion");
        
        TEST_ASSERT_EQUAL(10, counter, "All tasks executed correctly");
        
        thread_pool_destroy(pool);
    }
#else
    printf(ANSI_COLOR_YELLOW "ℹ  Threading tests skipped (threading disabled)" ANSI_COLOR_RESET "\n");
#endif
}

// =============================================================================
// PERFORMANCE TESTS
// =============================================================================

void test_performance() {
    TEST_SECTION("Performance Tests");
    
    // Test with different sizes
    int sizes[] = {100, 1000, 5000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    for (int i = 0; i < num_sizes; i++) {
        int size = sizes[i];
        printf("\n" ANSI_COLOR_BLUE "Testing with %d objects:" ANSI_COLOR_RESET "\n", size);
        
        // Generate large test data
        TIME_START();
        char* large_json = create_large_test_json(size);
        TIME_END("Large JSON generation");
        
        TEST_ASSERT_NOT_NULL(large_json, "Large JSON generated");
        
        if (!large_json) continue;
        
        // Test flattening performance
        TIME_START();
        char* flattened = flatten_json_string(large_json, 0, 0);
        TIME_END("Large JSON flattening (single-threaded)");
        
        TEST_ASSERT_NOT_NULL(flattened, "Large JSON flattened successfully");
        free(flattened);
        
        // Test multi-threaded flattening (if available)
#ifndef THREADING_DISABLED
        TIME_START();
        char* flattened_mt = flatten_json_string(large_json, 1, 4);
        TIME_END("Large JSON flattening (multi-threaded)");
        
        TEST_ASSERT_NOT_NULL(flattened_mt, "Large JSON flattened successfully (MT)");
        free(flattened_mt);
#endif
        
        // Test schema generation performance
        TIME_START();
        char* schema = generate_schema_from_string(large_json, 0, 0);
        TIME_END("Large JSON schema generation (single-threaded)");
        
        TEST_ASSERT_NOT_NULL(schema, "Large JSON schema generated successfully");
        free(schema);
        
        // Test multi-threaded schema generation (if available)
#ifndef THREADING_DISABLED
        TIME_START();
        char* schema_mt = generate_schema_from_string(large_json, 1, 4);
        TIME_END("Large JSON schema generation (multi-threaded)");
        
        TEST_ASSERT_NOT_NULL(schema_mt, "Large JSON schema generated successfully (MT)");
        free(schema_mt);
#endif
        
        free(large_json);
    }
}

// =============================================================================
// ERROR HANDLING TESTS
// =============================================================================

void test_error_handling() {
    TEST_SECTION("Error Handling Tests");
    
    // Test null input handling
    TEST_ASSERT_NULL(flatten_json_object(NULL), "Null input to flatten_json_object handled");
    TEST_ASSERT_NULL(flatten_json_string(NULL, 0, 0), "Null input to flatten_json_string handled");
    TEST_ASSERT_NULL(generate_schema_from_object(NULL), "Null input to generate_schema_from_object handled");
    TEST_ASSERT_NULL(generate_schema_from_string(NULL, 0, 0), "Null input to generate_schema_from_string handled");
    
    // Test invalid JSON handling
    const char* invalid_json = "{invalid json}";
    char* result = flatten_json_string(invalid_json, 0, 0);
    TEST_ASSERT_NULL(result, "Invalid JSON handled in flatten_json_string");
    
    char* schema_result = generate_schema_from_string(invalid_json, 0, 0);
    TEST_ASSERT_NULL(schema_result, "Invalid JSON handled in generate_schema_from_string");
    
    // Test empty JSON handling
    const char* empty_json = "{}";
    char* empty_result = flatten_json_string(empty_json, 0, 0);
    TEST_ASSERT_NOT_NULL(empty_result, "Empty JSON object handled");
    if (empty_result) {
        cJSON* parsed = cJSON_Parse(empty_result);
        TEST_ASSERT_NOT_NULL(parsed, "Empty JSON result is valid");
        if (parsed) cJSON_Delete(parsed);
        free(empty_result);
    }
    
    // Test string utility error handling
    char* null_dup = my_strdup(NULL);
    TEST_ASSERT_NULL(null_dup, "my_strdup handles null input");
    
    size_t null_len = strlen_simd(NULL);
    TEST_ASSERT_EQUAL(0, null_len, "strlen_simd handles null input");
}

// =============================================================================
// MEMORY VALIDATION TESTS
// =============================================================================

void test_memory_validation() {
    TEST_SECTION("Memory Validation Tests");
    
    // Test memory pool exhaustion and recovery
    init_global_pools();
    
    // Allocate many objects to test pool behavior
    void* ptrs[1000];
    int allocated = 0;
    
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = slab_alloc(g_cjson_node_pool);
        if (ptrs[i]) {
            allocated++;
        }
    }
    
    printf("Allocated %d objects from pool\n", allocated);
    TEST_ASSERT(allocated > 0, "Pool allocation successful");
    
    // Free half of them
    for (int i = 0; i < allocated; i += 2) {
        if (ptrs[i]) {
            slab_free(g_cjson_node_pool, ptrs[i]);
        }
    }
    
    // Try to allocate again
    void* new_ptr = slab_alloc(g_cjson_node_pool);
    TEST_ASSERT_NOT_NULL(new_ptr, "Pool reallocation after partial free successful");
    
    if (new_ptr) {
        slab_free(g_cjson_node_pool, new_ptr);
    }
    
    // Free remaining objects
    for (int i = 1; i < allocated; i += 2) {
        if (ptrs[i]) {
            slab_free(g_cjson_node_pool, ptrs[i]);
        }
    }
    
    cleanup_global_pools();
    
    // Test that operations still work after pool cleanup
    char* test_json = create_test_object_json();
    cJSON* json = cJSON_Parse(test_json);
    cJSON* flattened = flatten_json_object(json);
    TEST_ASSERT_NOT_NULL(flattened, "JSON processing works after pool cleanup");
    
    cJSON_Delete(flattened);
    cJSON_Delete(json);
    free(test_json);
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

void print_test_summary() {
    printf("\n" ANSI_COLOR_BLUE "=== TEST SUMMARY ===" ANSI_COLOR_RESET "\n");
    printf("Total tests run: %d\n", tests_run);
    printf(ANSI_COLOR_GREEN "Passed: %d" ANSI_COLOR_RESET "\n", tests_passed);
    
    if (tests_failed > 0) {
        printf(ANSI_COLOR_RED "Failed: %d" ANSI_COLOR_RESET "\n", tests_failed);
    } else {
        printf("Failed: 0\n");
    }
    
    double success_rate = tests_run > 0 ? (double)tests_passed / tests_run * 100.0 : 0.0;
    printf("Success rate: %.1f%%\n", success_rate);
    
    if (tests_failed == 0) {
        printf(ANSI_COLOR_GREEN "\n🎉 ALL TESTS PASSED!" ANSI_COLOR_RESET "\n");
    } else {
        printf(ANSI_COLOR_RED "\n❌ SOME TESTS FAILED" ANSI_COLOR_RESET "\n");
    }
}

int main(int argc, char* argv[]) {
    printf(ANSI_COLOR_BLUE "JSON Tools Comprehensive Test Suite" ANSI_COLOR_RESET "\n");
    printf("=====================================\n");
    
    // Print build information
    printf("Platform: ");
#ifdef __WINDOWS__
    printf("Windows");
#elif defined(__APPLE__)
    printf("macOS");
#elif defined(__linux__)
    printf("Linux");
#else
    printf("Unix-like");
#endif

#ifdef THREADING_DISABLED
    printf(" (Threading disabled)");
#endif
    printf("\n");
    
    printf("Compiler: ");
#ifdef _MSC_VER
    printf("MSVC");
#elif defined(__GNUC__)
    printf("GCC");
#elif defined(__clang__)
    printf("Clang");
#else
    printf("Unknown");
#endif
    printf("\n\n");
    
    // Initialize timing
    clock_t total_start = clock();
    
    // Run all test suites
    test_memory_pools();
    test_string_utilities();
    test_cpu_detection();
    test_json_flattening();
    test_json_schema_generation();
    test_path_extraction();
    test_json_utilities();
    test_threading();
    test_error_handling();
    test_memory_validation();
    
    // Run performance tests only if requested
    if (argc > 1 && strcmp(argv[1], "--performance") == 0) {
        test_performance();
    } else {
        printf("\n" ANSI_COLOR_YELLOW "ℹ  Run with --performance flag to include performance tests" ANSI_COLOR_RESET "\n");
    }
    
    // Calculate total time
    clock_t total_end = clock();
    double total_time = ((double)(total_end - total_start)) / CLOCKS_PER_SEC;
    
    // Print final summary
    print_test_summary();
    printf("\nTotal execution time: %.3f seconds\n", total_time);
    
    return tests_failed > 0 ? 1 : 0;
}