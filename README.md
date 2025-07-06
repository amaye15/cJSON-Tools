# cJSON-Tools

[![CI](https://github.com/amaye15/cJSON-Tools/workflows/Continuous%20Integration/badge.svg)](https://github.com/amaye15/cJSON-Tools/actions/workflows/ci.yml)
[![Security](https://github.com/amaye15/cJSON-Tools/workflows/Security%20%26%20Vulnerability%20Scanning/badge.svg)](https://github.com/amaye15/cJSON-Tools/actions/workflows/security.yml)
[![Performance](https://github.com/amaye15/cJSON-Tools/workflows/Performance%20Monitoring%20%26%20Benchmarks/badge.svg)](https://github.com/amaye15/cJSON-Tools/actions/workflows/performance.yml)
[![PyPI](https://img.shields.io/pypi/v/cjson-tools.svg)](https://pypi.org/project/cjson-tools/)
[![Python](https://img.shields.io/pypi/pyversions/cjson-tools.svg)](https://pypi.org/project/cjson-tools/)
[![License](https://img.shields.io/github/license/amaye15/cJSON-Tools.svg)](LICENSE)
[![Downloads](https://img.shields.io/pypi/dm/cjson-tools.svg)](https://pypi.org/project/cjson-tools/)
[![Documentation](https://img.shields.io/badge/docs-github%20pages-blue.svg)](https://amaye15.github.io/cJSON-Tools/)

## 🎉 **STABLE RELEASE v1.9.0**

**All GitHub Actions workflows passing!** ✅ (Updated: 2025-07-06)

### ✅ **Complete CI/CD Pipeline Status:**
- ✅ **Basic Test** - Core functionality validation
- ✅ **Continuous Integration** - Cross-platform testing (Ubuntu, macOS, Python 3.8-3.12) + Code Quality & Security
- ✅ **Build Tier Testing** - Multi-tier optimization builds + Cross-architecture (aarch64) validation
- ✅ **Security & Vulnerability Testing** - Valgrind memory checks, AddressSanitizer, CodeQL analysis
- ✅ **Performance Monitoring & Benchmarks** - Automated performance regression detection
- ✅ **Documentation & Release** - Automated documentation generation and publishing
- ✅ **Auto-format Code** - Automated code formatting with Black and isort

---

A high-performance C toolkit for transforming and analyzing JSON data with Python bindings. cJSON-Tools provides powerful tools for:

1. **JSON Flattening**: Converts nested JSON structures into flat key-value pairs
2. **Path Type Analysis**: Get flattened paths with their data types for schema discovery
3. **JSON Schema Generation**: Analyzes JSON objects and generates a unified JSON schema
4. **JSON Filtering**: Remove keys with empty string values or null values
5. **Regex Key Replacement**: Replace JSON keys matching regex patterns
6. **Regex Value Replacement**: Replace JSON string values matching regex patterns
7. **Multi-threading Support**: Optimized performance for processing large JSON datasets
8. **Performance Optimized**: SIMD instructions, memory pools, and cache-friendly algorithms
9. **Python Bindings**: Use the library directly from Python with native C performance

## 🚀 Quick Start Guide

### Python (Recommended)
```bash
pip install cjson-tools
```

```python
import cjson_tools
import json

# Flatten nested JSON
data = {"user": {"name": "John", "address": {"city": "NYC"}, "tags": ["dev", "python"]}}
flattened = cjson_tools.flatten_json(json.dumps(data))
print(flattened)  # {"user.name": "John", "user.address.city": "NYC", "user.tags[0]": "dev", "user.tags[1]": "python"}

# Get flattened paths with data types
paths_with_types = cjson_tools.get_flattened_paths_with_types(json.dumps(data))
print(paths_with_types)  # {"user.name": "string", "user.address.city": "string", "user.tags[0]": "string", "user.tags[1]": "string"}

# Generate JSON schema
schema = cjson_tools.generate_schema(json.dumps(data))
print(schema)

# Remove keys with empty string values
data_with_empty = {"name": "John", "email": "", "phone": "123-456-7890", "bio": ""}
cleaned = cjson_tools.remove_empty_strings(json.dumps(data_with_empty))
print(cleaned)  # {"name": "John", "phone": "123-456-7890"}

# Remove keys with null values
data_with_nulls = {"name": "John", "email": None, "phone": "123-456-7890", "address": None}
filtered = cjson_tools.remove_nulls(json.dumps(data_with_nulls))
print(filtered)  # {"name": "John", "phone": "123-456-7890"}

# Replace keys matching regex patterns
data_with_sessions = {"session.pageTimesInMs.HomePage": 1500, "session.pageTimesInMs.AboutPage": 2000, "user.name": "John"}
replaced_keys = cjson_tools.replace_keys(json.dumps(data_with_sessions), r"^session\.pageTimesInMs\..*$", "session.pageTimesInMs.PrezzePage")
print(replaced_keys)  # {"session.pageTimesInMs.PrezzePage": 1500, "session.pageTimesInMs.PrezzePage": 2000, "user.name": "John"}

# Replace string values matching regex patterns
data_with_old_values = {"user": {"status": "old_active", "role": "old_admin", "name": "John"}}
replaced_values = cjson_tools.replace_values(json.dumps(data_with_old_values), r"^old_.*$", "new_value")
print(replaced_values)  # {"user": {"status": "new_value", "role": "new_value", "name": "John"}}

# 🚀 NEW: Builder Pattern for Chained Operations
from cjson_tools import JsonToolsBuilder

# Chain multiple operations efficiently
result = (JsonToolsBuilder()
    .add_json(data_with_sessions)
    .remove_empty_strings()
    .remove_nulls()
    .replace_keys(r"^session\.tracking\..*\.timeMs$", "session.pageTimesInMs.UnifiedPage")
    .replace_keys(r"^analytics\.page\..*\.visits$", "analytics.pageViews.TotalPage")
    .replace_keys(r"^legacy\.server\..*$", "modern.server.instance")
    .replace_keys(r"^old_", "new_")
    .replace_values(r"^old_.*$", "new_value")
    .replace_values(r"^legacy_.*$", "modern_value")
    .replace_values(r"usr_\d+", "user_new_12345")
    .replace_values(r".*@oldcompany\.com", "john@newcompany.com")
    .flatten()
    .pretty_print(True)
    .build())

print(result)  # Fully transformed JSON with all operations applied efficiently
```

### C Library
```bash
git clone https://github.com/amaye15/cJSON-Tools.git
cd cJSON-Tools
make
./bin/json_tools -f input.json
```

## 📁 Project Structure

- `c-lib/` - C library source code, headers, and tests
  - `src/` - C source files with optimized algorithms
  - `include/` - C header files
  - `tests/` - Dynamic test suite and benchmarks
- `py-lib/` - Python bindings and related files
  - `cjson_tools/` - Python package source
  - `tests/` - Python unit tests
  - `examples/` - Python usage examples
  - `benchmarks/` - Performance testing scripts

## ✨ Features

### 🔄 JSON Flattening
- **Dot Notation**: Nested objects → `address.city`
- **Array Indexing**: Arrays → `skills[0]`, `skills[1]`
- **Type Preservation**: Maintains strings, numbers, booleans, null
- **Deep Nesting**: Handles arbitrarily nested structures
- **Batch Processing**: Process thousands of objects efficiently

### 📋 JSON Schema Generation
- **JSON Schema Draft-07**: Standards-compliant schema generation
- **Smart Type Detection**: Handles nested objects, arrays, mixed types
- **Required Properties**: Automatically detects required vs optional fields
- **Nullable Support**: Identifies fields that can be null
- **Array Analysis**: Intelligent sampling for large arrays

### 🧹 JSON Filtering
- **Remove Empty Strings**: Filter out keys with empty string (`""`) values
- **Remove Null Values**: Filter out keys with `null` values
- **Recursive Processing**: Works on deeply nested objects and arrays
- **Structure Preservation**: Maintains JSON structure while filtering
- **Pretty Printing**: Optional formatted output for filtered results

### 🔄 Regex Replacement
- **Replace Keys**: Use regex patterns to replace JSON keys matching specific patterns
- **Replace Values**: Use regex patterns to replace JSON string values matching patterns
- **POSIX Regex Support**: Full regex functionality on Unix-like systems (Linux, macOS)
- **Type Safety**: Value replacement only affects string values, preserves numbers/booleans/null
- **Recursive Processing**: Works on deeply nested objects and arrays
- **Pattern Flexibility**: Support for complex regex patterns with capture groups and anchors

### 🔗 JsonToolsBuilder - Fluent Interface
- **Method Chaining**: Chain multiple operations in a single, readable expression
- **Optimized Processing**: Operations applied in optimal order for best performance
- **Flexible Operations**: Combine filtering, regex replacement, and flattening seamlessly
- **Pretty Printing**: Built-in support for formatted output
- **Error Handling**: Comprehensive validation and error reporting
- **Architecture Ready**: C foundation implemented for future single-pass optimization

### ⚡ Performance Optimizations
- **Multi-threading**: Parallel processing for large datasets
- **Memory Pools**: Optimized memory allocation for small strings
- **Branch Prediction**: Compiler hints for better CPU performance
- **Adaptive Threading**: Automatically chooses optimal thread count
- **SIMD Optimizations**: Vectorized operations where possible
- **Zero-Copy**: Minimal memory copying for better performance

### 🐍 Python Integration
- **Native Performance**: C-speed with Python convenience
- **Pythonic API**: Simple, intuitive function calls
- **Type Safety**: Proper error handling and validation
- **Memory Management**: Automatic cleanup, no memory leaks

## 📋 Requirements

### Python Package (Recommended)
- **Python**: 3.6+ (3.8+ recommended)
- **Compiler**: Automatically handled by pip
- **Platform**: Linux, macOS, Windows

### C Library Development
- **Compiler**: GCC 7+ or Clang 6+ with C99 support
- **Dependencies**: Integrated cJSON (no external dependencies)
- **Threading**: POSIX threads (pthread)
- **Build Tools**: Make, standard build utilities

### Platform-Specific Setup

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install build-essential python3-dev
```

#### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install
# Or install via Homebrew
brew install python
```

#### Windows
- Install Visual Studio Build Tools or Visual Studio Community
- Python will automatically detect and use the compiler

## 🛠️ Installation & Setup

### Option 1: Python Package (Recommended)

#### From PyPI
```bash
pip install cjson-tools
```

#### From Source
```bash
git clone https://github.com/amaye15/cJSON-Tools.git
cd cJSON-Tools/py-lib
pip install -e .  # Development mode
# or
pip install .     # Regular installation
```

### Option 2: C Library Development

#### Quick Build
```bash
git clone https://github.com/amaye15/cJSON-Tools.git
cd cJSON-Tools
make                    # Build optimized version
make debug             # Build debug version
```

#### Advanced Build Options
```bash
# Clean build
make clean && make

# Install system-wide (optional)
sudo make install

# Build with specific optimizations
CFLAGS="-O3 -march=native" make

# Build tests
cd c-lib/tests
make
```

### Option 3: Development Setup

#### Full Development Environment
```bash
# Clone repository
git clone https://github.com/amaye15/cJSON-Tools.git
cd cJSON-Tools

# Build C library
make

# Setup Python development
cd py-lib
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -e .

# Run tests
python3 tests/test_cjson_tools.py
cd ../c-lib/tests
./run_dynamic_tests.sh
```

## 🚀 Usage Guide

### Python API (Recommended)

#### Basic Operations
```python
import cjson_tools
import json

# Single object flattening
data = {"user": {"name": "John", "details": {"age": 30, "city": "NYC"}}}
flattened = cjson_tools.flatten_json(json.dumps(data))
result = json.loads(flattened)
print(result)  # {"user.name": "John", "user.details.age": 30, "user.details.city": "NYC"}

# Schema generation
schema = cjson_tools.generate_schema(json.dumps(data))
schema_obj = json.loads(schema)
print(schema_obj["properties"])
```

#### Path Type Analysis
```python
# Get flattened paths with their data types
data = {
    "user": {
        "name": "John",
        "age": 30,
        "tags": ["developer", "python"],
        "address": {
            "coordinates": [40.7128, -74.0060],
            "city": "New York"
        },
        "active": True,
        "metadata": None
    }
}

# Analyze path types
paths_with_types = cjson_tools.get_flattened_paths_with_types(json.dumps(data))
result = json.loads(paths_with_types)

# Output shows each flattened path with its data type:
# {
#   "user.name": "string",
#   "user.age": "integer",
#   "user.tags[0]": "string",
#   "user.tags[1]": "string",
#   "user.address.coordinates[0]": "number",
#   "user.address.coordinates[1]": "number",
#   "user.address.city": "string",
#   "user.active": "boolean",
#   "user.metadata": "null"
# }

# Perfect for data analysis and schema discovery!
for path, data_type in result.items():
    print(f'"{path}": "{data_type}"')
```

#### Batch Processing
```python
# Process multiple objects efficiently
objects = [
    '{"id": 1, "user": {"name": "Alice"}}',
    '{"id": 2, "user": {"name": "Bob", "age": 25}}',
    '{"id": 3, "user": {"name": "Charlie", "location": {"city": "SF"}}}'
]

# Flatten all objects
flattened_batch = cjson_tools.flatten_json_batch(objects)
for flat in flattened_batch:
    print(json.loads(flat))

# Generate unified schema from all objects
unified_schema = cjson_tools.generate_schema_batch(objects)
print(json.loads(unified_schema))
```

#### Performance Optimization
```python
# For large datasets, enable multi-threading
large_dataset = [json.dumps({"data": i, "nested": {"value": i*2}}) for i in range(10000)]

# Auto-detect optimal thread count
result = cjson_tools.flatten_json_batch(large_dataset, use_threads=True)

# Specify thread count
result = cjson_tools.flatten_json_batch(large_dataset, use_threads=True, num_threads=4)

# Single-threaded for comparison
result = cjson_tools.flatten_json_batch(large_dataset, use_threads=False)
```

### C Command Line Interface

```bash
# JSON Tools - High-performance JSON processing utility
# Usage: json_tools [options] [input_file]

# Options:
#   -h, --help                 Show help message
#   -f, --flatten              Flatten nested JSON (default)
#   -s, --schema               Generate JSON schema
#   -e, --remove-empty         Remove keys with empty string values
#   -n, --remove-nulls         Remove keys with null values
#   -r, --replace-keys <pattern> <replacement>
#                              Replace keys matching regex pattern
#   -v, --replace-values <pattern> <replacement>
#                              Replace string values matching regex pattern
#   -t, --threads [num]        Use multi-threading (auto-detect optimal count)
#   -p, --pretty               Pretty-print output
#   -o, --output <file>        Write to file instead of stdout
```

### C CLI Examples

#### JSON Flattening
```bash
# From file
./bin/json_tools -f input.json

# From stdin with pretty printing
cat input.json | ./bin/json_tools -f -p

# Multi-threaded processing
./bin/json_tools -f -t 4 large_batch.json

# Save to file
./bin/json_tools -f -o flattened.json input.json
```

#### Schema Generation
```bash
# Generate schema from file
./bin/json_tools -s input.json

# Multi-threaded schema generation
./bin/json_tools -s -t 4 large_batch.json

# Pretty-printed schema to file
./bin/json_tools -s -p -o schema.json input.json
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

#### JSON Filtering

**Remove Empty Strings:**
```bash
# Remove keys with empty string values
echo '{"name": "John", "email": "", "phone": "123-456-7890", "bio": ""}' | ./bin/json_tools -e -

# Output: {"name":"John","phone":"123-456-7890"}
```

**Remove Null Values:**
```bash
# Remove keys with null values
echo '{"name": "John", "email": null, "phone": "123-456-7890", "address": null}' | ./bin/json_tools -n -

# Output: {"name":"John","phone":"123-456-7890"}
```

**Complex Example:**
```bash
# Input with nested objects and arrays
echo '{
  "user": {
    "name": "John Doe",
    "email": "",
    "profile": {
      "bio": "",
      "website": null,
      "social": {
        "twitter": "",
        "linkedin": "john-doe",
        "github": null
      }
    }
  },
  "preferences": ["", "email", null, "sms"]
}' | ./bin/json_tools -e -p -

# Removes all empty strings recursively while preserving structure
```

#### Regex Key Replacement

**Replace Keys with Patterns:**
```bash
# Replace session timing keys
echo '{"session.pageTimesInMs.HomePage": 1500, "session.pageTimesInMs.AboutPage": 2000, "user.name": "John"}' | \
./bin/json_tools -r '^session\.pageTimesInMs\..*$' 'session.pageTimesInMs.PrezzePage' -

# Output: {"session.pageTimesInMs.PrezzePage":1500,"session.pageTimesInMs.PrezzePage":2000,"user.name":"John"}
```

#### Regex Value Replacement

**Replace String Values with Patterns:**
```bash
# Replace old status values
echo '{"user": {"status": "old_active", "role": "old_admin", "name": "John"}}' | \
./bin/json_tools -v '^old_.*$' 'new_value' -

# Output: {"user":{"status":"new_value","role":"new_value","name":"John"}}
```

**Complex Example with Both:**
```bash
# Input with both keys and values to replace
echo '{
  "session.timing.old_load": "old_slow",
  "session.timing.old_render": "old_fast",
  "user.name": "John"
}' | ./bin/json_tools -r '^session\.timing\..*$' 'session.timing.unified' -v '^old_.*$' 'new_value' -p -

# First replaces keys, then values, with pretty printing
```

## JsonToolsBuilder - Fluent Interface

The `JsonToolsBuilder` class provides a powerful fluent interface for chaining multiple JSON transformation operations. This approach offers better readability and will be optimized for single-pass processing in future versions.

### Basic Usage

```python
from cjson_tools import JsonToolsBuilder

# Simple chaining
result = (JsonToolsBuilder()
    .add_json({"name": "John", "empty": "", "null_field": None})
    .remove_empty_strings()
    .remove_nulls()
    .build())

print(result)  # {"name": "John"}
```

### Complex Transformation Pipeline

```python
# Real-world data transformation scenario
data = {
    "session.tracking.homepage.timeMs": 1500,
    "session.tracking.aboutpage.timeMs": 2000,
    "analytics.page.home.visits": 100,
    "analytics.page.about.visits": 200,
    "legacy.server.instance1": "old_active",
    "legacy.server.instance2": "old_inactive",
    "old_user_id": "usr_12345",
    "deprecated_phone": "deprecated_555-1234",
    "user_email": "john@oldcompany.com",
    "empty_field": "",
    "null_field": None,
    "user_profile": {
        "old_status": "old_premium",
        "legacy_tier": "legacy_gold",
        "empty_bio": "",
        "null_avatar": None
    }
}

# Apply comprehensive transformation
result = (JsonToolsBuilder()
    .add_json(data)

    # Clean up empty and null values
    .remove_empty_strings()
    .remove_nulls()

    # Standardize key naming patterns
    .replace_keys(r"^session\.tracking\..*\.timeMs$", "session.pageTimesInMs.UnifiedPage")
    .replace_keys(r"^analytics\.page\..*\.visits$", "analytics.pageViews.TotalPage")
    .replace_keys(r"^legacy\.server\..*$", "modern.server.instance")
    .replace_keys(r"^old_", "new_")
    .replace_keys(r"^deprecated", "updated")

    # Standardize value patterns
    .replace_values(r"^old_.*$", "new_value")
    .replace_values(r"^legacy_.*$", "modern_value")
    .replace_values(r"^deprecated_.*$", "updated_value")
    .replace_values(r"usr_\d+", "user_new_12345")
    .replace_values(r".*@oldcompany\.com", "john@newcompany.com")

    # Optional: flatten the structure
    .flatten()

    # Format output
    .pretty_print(True)

    # Execute all operations
    .build())

print(result)
```

### Builder Methods

#### Data Input
- **`.add_json(data)`** - Add JSON data (string, dict, or list)

#### Filtering Operations
- **`.remove_empty_strings()`** - Remove keys with empty string values
- **`.remove_nulls()`** - Remove keys with null values

#### Regex Replacement
- **`.replace_keys(pattern, replacement)`** - Replace keys matching regex pattern
- **`.replace_values(pattern, replacement)`** - Replace string values matching regex pattern

#### Transformation
- **`.flatten()`** - Flatten nested JSON structure

#### Output Configuration
- **`.pretty_print(enable=True)`** - Enable/disable pretty printing

#### Execution
- **`.build()`** - Execute all queued operations and return result

#### Utility
- **`.reset()`** - Reset builder to initial state
- **`.get_operations()`** - Get list of queued operations

### Error Handling

```python
from cjson_tools import JsonToolsBuilder

try:
    result = (JsonToolsBuilder()
        .add_json('{"invalid": json}')  # Invalid JSON
        .build())
except ValueError as e:
    print(f"Error: {e}")

# Check for data before building
builder = JsonToolsBuilder()
try:
    result = builder.build()  # No data provided
except ValueError as e:
    print(f"Error: {e}")  # "No JSON data provided. Call add_json() first."
```

### Performance Benefits

- **Readable Code**: Fluent interface makes complex transformations easy to understand
- **Optimized Processing**: Operations are applied in optimal order for best performance
- **Architecture Ready**: C foundation implemented for future single-pass optimization
- **Memory Efficient**: Intermediate results are managed automatically
- **Type Safety**: All operations preserve data types appropriately

### Implementation Status

**Current**: Uses optimized multi-pass processing with intelligent operation ordering
**Future**: C single-pass implementation ready for activation once memory management is optimized

## Performance

The multi-threaded implementation is designed for processing large batches of JSON objects, though our benchmarks show interesting results:

- For small files: Single-threaded processing is generally more efficient
- For medium files: Multi-threading shows minimal improvements for specific operations
- For large files: Current implementation shows thread overhead may outweigh benefits

### Benchmarks

The project includes comprehensive benchmarking tools to measure performance:

```bash
# Run mini benchmarks (quick demonstration)
cd c-lib/tests
./run_mini_benchmarks.sh

# Run full benchmarks (may take several minutes)
./run_benchmarks.sh

# Generate visualization charts
cd ../../py-lib/benchmarks
python visualize_benchmarks.py

# Generate comprehensive visualization
python visualize_comprehensive_benchmarks.py
```

Benchmark results are saved to `c-lib/tests/benchmark_results.md` and `c-lib/tests/comprehensive_benchmark_results.md`, with visualizations in the corresponding PNG files.

#### Actual Benchmark Results

![Benchmark Charts](/c-lib/tests/comprehensive_benchmark_charts.png)

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

For detailed benchmark analysis, see `c-lib/tests/comprehensive_benchmark_results.md`.

## Python Usage

The Python bindings provide a simple, Pythonic interface to the cJSON-Tools library.

### Installation

```bash
pip install cjson-tools
```

### Examples

#### Flattening JSON

```python
import json
from cjson_tools import flatten_json

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
from cjson_tools import flatten_json_batch

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
from cjson_tools import generate_schema

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
from cjson_tools import flatten_json_batch, generate_schema_batch

# Enable multi-threading with a specific number of threads
flattened = flatten_json_batch(large_json_list, use_threads=True, num_threads=4)

# Auto-detect the optimal number of threads
schema = generate_schema_batch(large_json_list, use_threads=True)
```

For more examples, see the `py-lib/examples` directory.

## 🧪 Testing

### Python Tests
```bash
cd py-lib

# Run unit tests
python3 tests/test_cjson_tools.py

# Run example script
python3 examples/example.py

# Performance testing
python3 -c "
import cjson_tools
import json
import time

# Quick performance test
data = [json.dumps({'id': i, 'nested': {'value': i*2}}) for i in range(1000)]
start = time.time()
result = cjson_tools.flatten_json_batch(data, use_threads=True)
print(f'Processed {len(data)} objects in {time.time()-start:.3f}s')
"
```

### C Library Tests
```bash
cd c-lib/tests

# Run comprehensive dynamic tests
./run_dynamic_tests.sh

# Run specific test sizes
./run_dynamic_tests.sh --sizes "100,1000,10000"

# Build and run manual tests
make
./generate_test_data test.json 1000 3
../../bin/json_tools -f test.json
```

### Benchmarking
```bash
# Python benchmarks
cd py-lib/benchmarks
python3 benchmark.py --quick

# C library benchmarks
cd c-lib/tests
./run_benchmarks.sh

# Memory profiling (requires valgrind)
valgrind --tool=memcheck --leak-check=full ../../bin/json_tools -f large_test.json
```

## 📦 Publishing & Distribution

### Python Package Publishing

#### Setup for PyPI
```bash
cd py-lib

# Install build tools
pip install build twine

# Update version in setup.py
# Edit setup.py: version='1.4.0'

# Build package
python3 -m build

# Test upload to TestPyPI
twine upload --repository testpypi dist/*

# Upload to PyPI
twine upload dist/*
```

#### Automated Publishing (GitHub Actions)
```yaml
# .github/workflows/publish.yml
name: Publish to PyPI
on:
  release:
    types: [published]
jobs:
  publish:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    - name: Set up Python
      uses: actions/setup-python@v4
      with:
        python-version: '3.8'
    - name: Build and publish
      env:
        TWINE_USERNAME: __token__
        TWINE_PASSWORD: ${{ secrets.PYPI_API_TOKEN }}
      run: |
        cd py-lib
        pip install build twine
        python -m build
        twine upload dist/*
```

### C Library Distribution

#### Creating Release Packages
```bash
# Create source distribution
make clean
tar -czf cjson-tools-1.4.0.tar.gz \
  --exclude='.git*' \
  --exclude='*.o' \
  --exclude='bin/*' \
  --exclude='py-lib/build' \
  --exclude='py-lib/*.egg-info' \
  .

# Create binary distribution (Linux)
make clean && make
mkdir -p cjson-tools-1.4.0-linux-x64/{bin,lib,include}
cp bin/json_tools cjson-tools-1.4.0-linux-x64/bin/
cp c-lib/include/*.h cjson-tools-1.4.0-linux-x64/include/
tar -czf cjson-tools-1.4.0-linux-x64.tar.gz cjson-tools-1.4.0-linux-x64/
```

#### Package Managers

##### Homebrew (macOS)
```ruby
# Formula for Homebrew
class CjsonTools < Formula
  desc "High-performance JSON processing toolkit"
  homepage "https://github.com/amaye15/cJSON-Tools"
  url "https://github.com/amaye15/cJSON-Tools/archive/v1.4.0.tar.gz"
  sha256 "..."

  def install
    system "make"
    bin.install "bin/json_tools"
  end

  test do
    system "#{bin}/json_tools", "--help"
  end
end
```

##### APT Repository (Ubuntu/Debian)
```bash
# Build .deb package
mkdir -p cjson-tools-1.4.0/DEBIAN
cat > cjson-tools-1.4.0/DEBIAN/control << EOF
Package: cjson-tools
Version: 1.4.0
Architecture: amd64
Maintainer: Your Name <email@example.com>
Description: High-performance JSON processing toolkit
EOF

mkdir -p cjson-tools-1.4.0/usr/bin
cp bin/json_tools cjson-tools-1.4.0/usr/bin/
dpkg-deb --build cjson-tools-1.4.0
```

## � Automated CI/CD Pipeline

This project includes comprehensive GitHub Actions workflows for automated testing, security scanning, performance monitoring, and deployment:

### 🔄 Continuous Integration (`ci.yml`)
- **Multi-platform testing**: Ubuntu, macOS, Windows
- **Python version matrix**: 3.8, 3.9, 3.10, 3.11, 3.12
- **C library testing**: Build verification, memory leak detection with Valgrind
- **Python package testing**: Installation, functionality, performance validation
- **Code quality**: Black formatting, isort, flake8 linting
- **Security scanning**: Bandit, Safety dependency checks
- **Integration tests**: C CLI with Python validation, consistency checks

### 🔒 Security & Vulnerability Scanning (`security.yml`)
- **Dependency scanning**: Python package vulnerabilities with Safety
- **Static analysis**: CodeQL security analysis for C and Python
- **Container security**: Trivy vulnerability scanning
- **License compliance**: Automated license checking
- **Memory safety**: AddressSanitizer and Valgrind testing
- **Weekly automated scans**: Scheduled security monitoring

### 📊 Performance Monitoring (`performance.yml`)
- **Automated benchmarks**: Multi-platform performance testing
- **Regression detection**: Performance validation on PRs
- **Scalability testing**: Dataset sizes from 100 to 100K objects
- **Memory efficiency**: Memory usage monitoring and optimization
- **Performance visualization**: Automated chart generation
- **Benchmark history**: Long-term performance tracking

### 📦 Automated Publishing (`publish.yml`)
- **Multi-platform wheels**: Linux, macOS, Windows (x86_64, ARM64)
- **Source distribution**: Complete source package building
- **PyPI publishing**: Automated release to PyPI on tags
- **Test PyPI**: Optional test publishing for validation
- **GitHub releases**: Automated release asset creation
- **Trusted publishing**: Secure PyPI publishing with OIDC

### 📚 Documentation (`docs.yml`)
- **API documentation**: Automated C and Python API docs
- **Usage examples**: Comprehensive example generation
- **Performance guides**: Optimization documentation
- **GitHub Pages**: Automated documentation deployment
- **Release validation**: Version consistency checking

### 🎯 Workflow Features

**Quality Assurance:**
- ✅ Comprehensive test coverage across platforms and Python versions
- ✅ Memory leak detection with Valgrind
- ✅ Security vulnerability scanning
- ✅ Performance regression detection
- ✅ Code quality enforcement

**Automation:**
- 🔄 Automatic testing on every PR and push
- 📦 Automated PyPI publishing on releases
- 📊 Weekly performance and security monitoring
- 📚 Automatic documentation updates
- 🏷️ Badge updates for README

**Security:**
- 🔒 CodeQL static analysis
- 🛡️ Container security scanning
- 📋 License compliance monitoring
- 🔍 Dependency vulnerability tracking
- 🧪 Memory safety validation

**Performance:**
- ⚡ Multi-platform benchmarking
- 📈 Performance visualization
- 🎯 Regression detection
- 💾 Memory efficiency monitoring
- 📊 Throughput analysis

### 🚀 Getting Started with CI/CD

1. **Fork the repository** and enable GitHub Actions
2. **Configure secrets** for PyPI publishing (if needed)
3. **Create a release** to trigger automated publishing
4. **Monitor workflows** in the Actions tab

The CI/CD pipeline ensures high code quality, security, and performance while automating the entire release process from development to production deployment.

## �🔧 Development & Contributing

### Development Setup
```bash
# Full development environment
git clone https://github.com/amaye15/cJSON-Tools.git
cd cJSON-Tools

# Setup pre-commit hooks
pip install pre-commit
pre-commit install

# Build everything
make clean && make
cd py-lib && pip install -e . && cd ..

# Run all tests
cd c-lib/tests && ./run_dynamic_tests.sh && cd ../..
cd py-lib && python3 tests/test_cjson_tools.py && cd ..
```

### Code Quality
```bash
# C code formatting (if clang-format available)
find c-lib -name "*.c" -o -name "*.h" | xargs clang-format -i

# Python code formatting
cd py-lib
pip install black isort flake8
black .
isort .
flake8 .
```

### Performance Profiling
```bash
# Profile C library
cd c-lib/tests
perf record ../../bin/json_tools -f large_test.json
perf report

# Profile Python extension
cd py-lib
python3 -m cProfile -o profile.stats examples/example.py
python3 -c "import pstats; pstats.Stats('profile.stats').sort_stats('cumulative').print_stats(20)"
```

## 🏁 Performance Benchmarks

### Running Benchmarks

To run performance benchmarks comparing single-pass vs multi-pass processing:

```bash
cd py-lib
python benchmarks/benchmark.py [options]
```

**Options:**
- `--sizes`: Batch sizes to benchmark (default: 10, 100, 1000, 10000)
- `--runs`: Number of runs per benchmark (default: 3)
- `--output`: Output file for results (default: benchmark_results.md)
- `--plot`: Output file for plots (default: benchmark_results.png)

**Example:**
```bash
python benchmarks/benchmark.py --sizes 10 100 1000 5000 --runs 5
```

### Key Performance Findings

**JsonToolsBuilder Single-Pass vs Multi-Pass:**
- **1.31x average speedup** with single-pass C implementation
- **22.6% average performance improvement**
- **Best improvement on medium datasets** (33.2% faster)
- **Memory efficient** - single JSON tree traversal
- **Cache friendly** - data stays in CPU cache

**Batch Processing Recommendations:**
- **Flattening**: Use batches of 10-1000 objects with multi-threading
- **Schema Generation**: Use batches of 10-100 objects
- **Large datasets**: Split into smaller batches for optimal performance

## 📄 License

This project is open source and available under the MIT License.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

### Areas for Contribution
- Performance optimizations
- Additional output formats
- Language bindings (Rust, Go, etc.)
- Documentation improvements
- Bug fixes and testing

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/amaye15/cJSON-Tools/issues)
- **Documentation**: See `py-lib/examples/` and `c-lib/tests/`
- **Performance**: Run benchmarks with `./run_dynamic_tests.sh`