#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cjson/cJSON.h>

// Generate a random string of specified length
char* random_string(int length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char* str = (char*)malloc(length + 1);
    
    for (int i = 0; i < length; i++) {
        int index = rand() % (sizeof(charset) - 1);
        str[i] = charset[index];
    }
    
    str[length] = '\0';
    return str;
}

// Generate a random JSON object with nested structure
cJSON* generate_random_object(int depth, int max_depth) {
    cJSON* obj = cJSON_CreateObject();
    
    // Add some basic fields
    cJSON_AddStringToObject(obj, "id", random_string(8));
    cJSON_AddStringToObject(obj, "name", random_string(10));
    cJSON_AddStringToObject(obj, "email", random_string(8));
    cJSON_AddNumberToObject(obj, "age", rand() % 80 + 18);
    cJSON_AddBoolToObject(obj, "active", rand() % 2);
    
    // Add nested address object
    cJSON* address = cJSON_CreateObject();
    cJSON_AddStringToObject(address, "street", random_string(15));
    cJSON_AddStringToObject(address, "city", random_string(10));
    cJSON_AddStringToObject(address, "state", random_string(2));
    cJSON_AddStringToObject(address, "zipcode", random_string(5));
    cJSON_AddItemToObject(obj, "address", address);
    
    // Add an array of tags
    cJSON* tags = cJSON_CreateArray();
    int num_tags = rand() % 5 + 1;
    for (int i = 0; i < num_tags; i++) {
        cJSON_AddItemToArray(tags, cJSON_CreateString(random_string(8)));
    }
    cJSON_AddItemToObject(obj, "tags", tags);
    
    // Add more nested objects if depth allows
    if (depth < max_depth) {
        // Add nested preferences object
        cJSON* preferences = cJSON_CreateObject();
        cJSON_AddBoolToObject(preferences, "notifications", rand() % 2);
        cJSON_AddBoolToObject(preferences, "newsletter", rand() % 2);
        cJSON_AddNumberToObject(preferences, "theme", rand() % 3);
        
        // Add nested display settings
        cJSON* display = cJSON_CreateObject();
        cJSON_AddNumberToObject(display, "fontSize", rand() % 5 + 10);
        cJSON_AddBoolToObject(display, "darkMode", rand() % 2);
        cJSON_AddItemToObject(preferences, "display", display);
        
        cJSON_AddItemToObject(obj, "preferences", preferences);
        
        // Add nested history array with objects
        cJSON* history = cJSON_CreateArray();
        int num_history = rand() % 3 + 1;
        for (int i = 0; i < num_history; i++) {
            cJSON* entry = cJSON_CreateObject();
            cJSON_AddStringToObject(entry, "action", random_string(6));
            cJSON_AddNumberToObject(entry, "timestamp", time(NULL) - rand() % 86400);
            
            // Add nested details to history entries
            if (depth < max_depth - 1) {
                cJSON* details = cJSON_CreateObject();
                cJSON_AddStringToObject(details, "ip", random_string(12));
                cJSON_AddStringToObject(details, "device", random_string(8));
                cJSON_AddItemToObject(entry, "details", details);
            }
            
            cJSON_AddItemToArray(history, entry);
        }
        cJSON_AddItemToObject(obj, "history", history);
    }
    
    return obj;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <output_file> <num_objects> [max_depth]\n", argv[0]);
        return 1;
    }
    
    char* output_file = argv[1];
    int num_objects = atoi(argv[2]);
    int max_depth = (argc > 3) ? atoi(argv[3]) : 3;
    
    if (num_objects <= 0) {
        printf("Number of objects must be positive\n");
        return 1;
    }
    
    // Seed random number generator
    srand(time(NULL));
    
    // Create array of objects
    cJSON* root = cJSON_CreateArray();
    
    printf("Generating %d objects with max depth %d...\n", num_objects, max_depth);
    
    for (int i = 0; i < num_objects; i++) {
        cJSON* obj = generate_random_object(0, max_depth);
        cJSON_AddItemToArray(root, obj);
        
        // Print progress
        if (i % 1000 == 0 && i > 0) {
            printf("Generated %d objects...\n", i);
        }
    }
    
    // Write to file
    char* json_str = cJSON_Print(root);
    FILE* file = fopen(output_file, "w");
    if (!file) {
        printf("Error opening output file\n");
        cJSON_Delete(root);
        free(json_str);
        return 1;
    }
    
    fprintf(file, "%s", json_str);
    fclose(file);
    
    // Clean up
    cJSON_Delete(root);
    free(json_str);
    
    printf("Generated %d objects and saved to %s\n", num_objects, output_file);
    
    return 0;
}