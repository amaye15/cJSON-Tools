# JSON Alchemy

A high-performance C toolkit for transforming and analyzing JSON data. JSON Alchemy provides powerful tools for:

1. **JSON Flattening**: Converts nested JSON structures into flat key-value pairs
2. **JSON Schema Generation**: Analyzes JSON objects and generates a unified JSON schema
3. **Multi-threading Support**: Optimized performance for processing large JSON datasets

## Features

### JSON Flattening
- Flattens nested JSON objects using dot notation (e.g., `address.city`)
- Flattens arrays using bracket notation (e.g., `skills[0]`)
- Preserves primitive values (strings, numbers, booleans, null)
- Handles deeply nested structures
- Supports both single objects and arrays of objects (batches)

### JSON Schema Generation
- Generates JSON Schema (Draft-07) from one or more JSON objects
- Handles nested objects and arrays
- Detects required and optional properties
- Supports null values and mixed types
- Identifies nullable fields

### Performance Optimizations
- Multi-threaded processing for large datasets
- Automatic thread count detection based on available CPU cores
- Optimized memory usage for large files

## Requirements

- C compiler (gcc recommended)
- cJSON library (`libcjson-dev` package)
- POSIX threads library (pthread)

## Building

```bash
# Clone the repository
git clone https://github.com/yourusername/json-tools.git
cd json-tools

# Build the project
make

# Install (optional)
sudo make install
```

## Usage

```
JSON Tools - A unified JSON processing utility

Usage: json_tools [options] [input_file]

Options:
  -h, --help                 Show this help message
  -f, --flatten              Flatten nested JSON (default action)
  -s, --schema               Generate JSON schema
  -t, --threads [num]        Use multi-threading with specified number of threads
                             (default: auto-detect optimal thread count)
  -p, --pretty               Pretty-print output (default: compact)
  -o, --output <file>        Write output to file instead of stdout

If no input file is specified, input is read from stdin.
Use '-' as input_file to explicitly read from stdin.
```

## Examples

### Flatten a JSON object

```bash
# From a file
./bin/json_tools -f input.json

# From stdin
cat input.json | ./bin/json_tools -f -

# With multi-threading
./bin/json_tools -f -t 4 large_batch.json

# Write to file
./bin/json_tools -f -o flattened.json input.json
```

### Generate a JSON schema

```bash
# From a file
./bin/json_tools -s input.json

# From stdin
cat input.json | ./bin/json_tools -s -

# With multi-threading
./bin/json_tools -s -t 4 large_batch.json

# Write to file
./bin/json_tools -s -o schema.json input.json
```

## Example Input/Output

### JSON Flattening

Input:
```json
{
  "name": "John Doe",
  "age": 30,
  "address": {
    "street": "123 Main St",
    "city": "Anytown"
  },
  "skills": ["programming", "design"]
}
```

Output:
```json
{
  "name": "John Doe",
  "age": 30,
  "address.street": "123 Main St",
  "address.city": "Anytown",
  "skills[0]": "programming",
  "skills[1]": "design"
}
```

### JSON Schema Generation

Input:
```json
[
  {
    "id": 1,
    "name": "John",
    "email": "john@example.com",
    "active": true
  },
  {
    "id": 2,
    "name": "Jane",
    "email": "jane@example.com",
    "active": false,
    "tags": ["admin", "user"]
  }
]
```

Output:
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "id": {
      "type": "integer"
    },
    "name": {
      "type": "string"
    },
    "email": {
      "type": "string"
    },
    "active": {
      "type": "boolean"
    },
    "tags": {
      "type": ["array", "null"],
      "items": {
        "type": "string"
      }
    }
  },
  "required": ["id", "name", "email", "active"]
}
```

## Performance

The multi-threaded implementation provides significant performance improvements for large batches of JSON objects:

- For small files: Single-threaded processing is efficient
- For large files (10MB+): Multi-threaded processing is up to 40% faster
- Optimal thread count depends on available CPU cores and file size

### Benchmarks

The project includes comprehensive benchmarking tools to measure performance:

```bash
# Run mini benchmarks (quick demonstration)
cd test
./run_mini_benchmarks.sh

# Run full benchmarks (may take several minutes)
./run_benchmarks.sh

# Generate visualization charts
python visualize_benchmarks.py
```

Benchmark results are saved to `test/benchmark_results.md` and visualized in `test/benchmark_charts.png`.

#### Sample Benchmark Results

- **Small files (< 1MB)**: Multi-threading overhead may outweigh benefits
- **Medium files (1-10MB)**: Multi-threading provides 10-20% improvement
- **Large files (10-50MB)**: Multi-threading provides 20-30% improvement
- **Huge files (50MB+)**: Multi-threading provides 30-40% improvement

The performance gains are more significant for schema generation than for flattening, as schema generation involves more complex processing.

## License

This project is open source and available under the MIT License.