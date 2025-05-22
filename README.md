# JSON Alchemy

A high-performance C toolkit for transforming and analyzing JSON data. JSON Alchemy provides powerful tools for:

1. **JSON Flattening**: Converts nested JSON structures into flat key-value pairs
2. **JSON Schema Generation**: Analyzes JSON objects and generates a unified JSON schema
3. **Multi-threading Support**: Optimized performance for processing large JSON datasets
4. **Python Bindings**: Use the library directly from Python with native performance

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
- Thread pooling to reduce thread creation overhead
- Adaptive threading that only uses multiple threads when beneficial

### Python Bindings
- Use the library directly from Python
- Native C performance with Python convenience
- Support for all core features (flattening, schema generation, multi-threading)
- Simple, Pythonic API

## Requirements

### C Library
- C compiler (gcc recommended)
- cJSON library (`libcjson-dev` package)
- POSIX threads library (pthread)

### Python Bindings
- Python 3.6+
- C compiler
- cJSON library (`libcjson-dev` package)
- Python development headers (`python3-dev` package)

## Building

### C Library

```bash
# Clone the repository
git clone https://github.com/amaye15/AIMv2-rs.git
cd AIMv2-rs

# Build the project
make

# Install (optional)
sudo make install
```

### Python Bindings

```bash
# Clone the repository
git clone https://github.com/amaye15/AIMv2-rs.git
cd AIMv2-rs/python

# Install the package
pip install .

# Or install in development mode
pip install -e .
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

The multi-threaded implementation is designed for processing large batches of JSON objects, though our benchmarks show interesting results:

- For small files: Single-threaded processing is generally more efficient
- For medium files: Multi-threading shows minimal improvements for specific operations
- For large files: Current implementation shows thread overhead may outweigh benefits

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

# Generate comprehensive visualization
python visualize_comprehensive_benchmarks.py
```

Benchmark results are saved to `test/benchmark_results.md` and `test/comprehensive_benchmark_results.md`, with visualizations in the corresponding PNG files.

#### Actual Benchmark Results

![Benchmark Charts](/test/comprehensive_benchmark_charts.png)

Our comprehensive benchmarks revealed:

- **Small files (< 100KB)**: Multi-threading overhead outweighs benefits, with performance similar to or worse than single-threaded processing
- **Medium files (100KB-1MB)**: Multi-threading provides minimal improvement (0-12.5%) for schema generation
- **Large files (1MB-10MB)**: Current multi-threaded implementation shows slight performance degradation (-7% to -8%)

Key findings:

1. **Thread Overhead**: For most file sizes, the overhead of creating and managing threads outweighs the benefits of parallel processing.

2. **Optimal Use Case**: Multi-threading appears to be most beneficial for medium-sized files (around 1,000 objects) for schema generation.

3. **Future Optimizations**: The multi-threaded implementation could be improved with:
   - More efficient work distribution
   - Thread pooling to reduce creation overhead
   - Optimized memory usage to reduce cache misses
   - Adaptive threading that only uses multiple threads when beneficial

For detailed benchmark analysis, see `test/comprehensive_benchmark_results.md`.

## Python Usage

The Python bindings provide a simple, Pythonic interface to the JSON Alchemy library.

### Installation

```bash
pip install json-alchemy
```

### Examples

#### Flattening JSON

```python
import json
from json_alchemy import flatten_json

# Flatten a single JSON object
nested_json = '''
{
    "person": {
        "name": "John Doe",
        "age": 30,
        "address": {
            "street": "123 Main St",
            "city": "Anytown",
            "zip": "12345"
        }
    }
}
'''

flattened = flatten_json(nested_json)
print(json.loads(flattened))
# Output: {"person.name": "John Doe", "person.age": 30, "person.address.street": "123 Main St", ...}
```

#### Batch Processing

```python
from json_alchemy import flatten_json_batch

# Process multiple JSON objects at once
json_objects = [
    '{"a": {"b": 1}}',
    '{"x": {"y": {"z": 2}}}'
]

flattened_batch = flatten_json_batch(json_objects)
print(flattened_batch)
# Output: ['{"a.b": 1}', '{"x.y.z": 2}']
```

#### Schema Generation

```python
from json_alchemy import generate_schema

# Generate a schema from a JSON object
json_obj = '''
{
    "id": 1,
    "name": "Product",
    "price": 29.99,
    "tags": ["electronics", "gadget"]
}
'''

schema = generate_schema(json_obj)
print(json.loads(schema))
# Output: {"type": "object", "properties": {"id": {"type": "number"}, ...}}
```

#### Multi-threading

```python
from json_alchemy import flatten_json_batch, generate_schema_batch

# Enable multi-threading with a specific number of threads
flattened = flatten_json_batch(large_json_list, use_threads=True, num_threads=4)

# Auto-detect the optimal number of threads
schema = generate_schema_batch(large_json_list, use_threads=True)
```

For more examples, see the `python/examples` directory.

## License

This project is open source and available under the MIT License.