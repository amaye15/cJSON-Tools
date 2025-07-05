#include "../include/json_flattener.h"
#include "../include/json_schema_generator.h"
#include "../include/json_utils.h"
#include "../include/memory_pool.h"
#include "../include/compiler_hints.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Print usage information
void print_usage(const char* program_name) {
    printf("JSON Tools - A unified JSON processing utility\n\n");
    printf("Usage: %s [options] [input_file]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help                 Show this help message\n");
    printf("  -f, --flatten              Flatten nested JSON (default action)\n");
    printf("  -s, --schema               Generate JSON schema\n");
    printf("  -e, --remove-empty         Remove keys with empty string values\n");
    printf("  -n, --remove-nulls         Remove keys with null values\n");
    printf("  -t, --threads [num]        Use multi-threading with specified number of threads\n");
    printf("                             (default: auto-detect optimal thread count)\n");
    printf("  -p, --pretty               Pretty-print output (default: compact)\n");
    printf("  -o, --output <file>        Write output to file instead of stdout\n\n");
    printf("If no input file is specified, input is read from stdin.\n");
    printf("Use '-' as input_file to explicitly read from stdin.\n\n");
    printf("Examples:\n");
    printf("  %s input.json                     # Flatten JSON from file\n", program_name);
    printf("  cat input.json | %s -             # Flatten JSON from stdin\n", program_name);
    printf("  %s -s input.json                  # Generate schema from file\n", program_name);
    printf("  %s -e input.json                  # Remove empty string values\n", program_name);
    printf("  %s -n input.json                  # Remove null values\n", program_name);
    printf("  %s -f -t 4 large_batch.json       # Flatten with 4 threads\n", program_name);
    printf("  %s -s -t 2 -o schema.json *.json  # Generate schema from multiple files\n", program_name);
}

int main(int argc, char* argv[]) {
    // Initialize global memory pools for better performance
    init_global_pools();

    // Default options
    int action_flatten = 1;
    int action_schema = 0;
    int action_remove_empty = 0;
    int action_remove_nulls = 0;
    int use_threads = 0;
    int num_threads = 0;
    int pretty_print = 0;

    char* output_file = NULL;
    char* input_file = NULL;
    
    // Optimized command line argument parsing
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        // Fast path for common single-character options
        if (arg[0] == '-' && arg[1] != '-' && arg[2] == '\0') {
            switch (arg[1]) {
                case 'h':
                    print_usage(argv[0]);
                    return 0;
                case 'f':
                    action_flatten = 1;
                    action_schema = 0;
                    action_remove_empty = 0;
                    action_remove_nulls = 0;
                    continue;
                case 's':
                    action_flatten = 0;
                    action_schema = 1;
                    action_remove_empty = 0;
                    action_remove_nulls = 0;
                    break;
                case 'e':
                    action_flatten = 0;
                    action_schema = 0;
                    action_remove_empty = 1;
                    action_remove_nulls = 0;
                    break;
                case 'n':
                    action_flatten = 0;
                    action_schema = 0;
                    action_remove_empty = 0;
                    action_remove_nulls = 1;
                    break;
                case 't':
                    use_threads = 1;
                    if (i + 1 < argc && argv[i + 1][0] != '-') {
                        num_threads = atoi(argv[i + 1]);
                        i++;
                    }
                    continue;
                case 'p':
                    pretty_print = 1;
                    continue;
                case 'o':
                    if (i + 1 < argc) {
                        output_file = argv[i + 1];
                        i++;
                    } else {
                        fprintf(stderr, "Error: Output file name missing\n");
                        return 1;
                    }
                    continue;
                default:
                    fprintf(stderr, "Error: Unknown option '-%c'\n", arg[1]);
                    return 1;
            }
        }

        // Long options
        if (strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(arg, "--flatten") == 0) {
            action_flatten = 1;
            action_schema = 0;
            action_remove_empty = 0;
            action_remove_nulls = 0;
        } else if (strcmp(arg, "--schema") == 0) {
            action_flatten = 0;
            action_schema = 1;
            action_remove_empty = 0;
            action_remove_nulls = 0;
        } else if (strcmp(arg, "--remove-empty") == 0) {
            action_flatten = 0;
            action_schema = 0;
            action_remove_empty = 1;
            action_remove_nulls = 0;
        } else if (strcmp(arg, "--remove-nulls") == 0) {
            action_flatten = 0;
            action_schema = 0;
            action_remove_empty = 0;
            action_remove_nulls = 1;
        } else if (strcmp(arg, "--threads") == 0) {
            use_threads = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                num_threads = atoi(argv[i + 1]);
                i++;
            }
        } else if (strcmp(arg, "--pretty") == 0) {
            pretty_print = 1;
        } else if (strcmp(arg, "--output") == 0) {
            if (i + 1 < argc) {
                output_file = argv[i + 1];
                i++;
            } else {
                fprintf(stderr, "Error: Output file name missing\n");
                return 1;
            }
        } else if (arg[0] == '-' && arg[1] != '\0' && arg[1] != '-') {
            // Handle combined short options like -ft
            for (int j = 1; argv[i][j] != '\0'; j++) {
                switch (argv[i][j]) {
                    case 'f':
                        action_flatten = 1;
                        action_schema = 0;
                        action_remove_empty = 0;
                        action_remove_nulls = 0;
                        break;
                    case 's':
                        action_flatten = 0;
                        action_schema = 1;
                        action_remove_empty = 0;
                        action_remove_nulls = 0;
                        break;
                    case 'e':
                        action_flatten = 0;
                        action_schema = 0;
                        action_remove_empty = 1;
                        action_remove_nulls = 0;
                        break;
                    case 'n':
                        action_flatten = 0;
                        action_schema = 0;
                        action_remove_empty = 0;
                        action_remove_nulls = 1;
                        break;
                    case 't':
                        use_threads = 1;
                        break;
                    case 'p':
                        pretty_print = 1;
                        break;
                    default:
                        fprintf(stderr, "Error: Unknown option '-%c'\n", argv[i][j]);
                        return 1;
                }
            }
        } else if (input_file == NULL) {
            input_file = argv[i];
        } else {
            fprintf(stderr, "Error: Multiple input files not supported\n");
            return 1;
        }
    }
    
    // Read JSON input
    char* json_string = NULL;
    
    if (input_file == NULL || strcmp(input_file, "-") == 0) {
        // Read from stdin
        json_string = read_json_stdin();
    } else {
        // Read from file
        json_string = read_json_file(input_file);
    }
    
    if (!json_string) {
        fprintf(stderr, "Error: Failed to read JSON input\n");
        return 1;
    }
    
    // Process JSON based on selected action
    char* result = NULL;

    if (action_flatten) {
        result = flatten_json_string(json_string, use_threads, num_threads);
    } else if (action_schema) {
        result = generate_schema_from_string(json_string, use_threads, num_threads);
    } else if (action_remove_empty || action_remove_nulls) {
        // Parse the JSON first
        cJSON* json = cJSON_Parse(json_string);
        if (!json) {
            fprintf(stderr, "Error: Invalid JSON input\n");
            free(json_string);
            return 1;
        }

        // Apply the appropriate filter
        cJSON* filtered_json = NULL;
        if (action_remove_empty) {
            filtered_json = remove_empty_strings(json);
        } else if (action_remove_nulls) {
            filtered_json = remove_nulls(json);
        }

        cJSON_Delete(json);

        if (!filtered_json) {
            fprintf(stderr, "Error: Filtering failed\n");
            free(json_string);
            return 1;
        }

        // Convert back to string
        if (pretty_print) {
            result = cJSON_Print(filtered_json);
        } else {
            result = cJSON_PrintUnformatted(filtered_json);
        }

        cJSON_Delete(filtered_json);
    }
    
    free(json_string);
    
    if (!result) {
        fprintf(stderr, "Error: Processing failed\n");
        return 1;
    }
    
    // Output result with optional pretty printing
    char* final_output = result;
    if (pretty_print && (action_flatten || action_schema)) {
        // For flattened JSON and schema, parse and reformat with pretty printing
        // (remove_empty and remove_nulls already handle pretty printing above)
        cJSON* json = cJSON_Parse(result);
        if (json) {
            free(result);
            final_output = cJSON_Print(json);
            cJSON_Delete(json);
        }
    }

    if (output_file) {
        FILE* file = fopen(output_file, "w");
        if (!file) {
            fprintf(stderr, "Error: Could not open output file %s\n", output_file);
            free(final_output);
            return 1;
        }
        fprintf(file, "%s\n", final_output);
        fclose(file);
    } else {
        printf("%s\n", final_output);
    }
    
    free(final_output);

    // Cleanup global memory pools
    cleanup_global_pools();

    return 0;
}