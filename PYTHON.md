# JSON Alchemy Python Bindings

This document provides detailed information about the Python bindings for the JSON Alchemy library.

## Overview

The Python bindings provide a simple, Pythonic interface to the JSON Alchemy C library, allowing you to use all the core features of JSON Alchemy directly from Python with native C performance.

## Installation

### From PyPI (Recommended)

```bash
pip install json-alchemy
```

### From Source

```bash
# Clone the repository
git clone https://github.com/amaye15/AIMv2-rs.git
cd AIMv2-rs/python

# Install the package
pip install .

# Or install in development mode
pip install -e .
```

## Requirements

- Python 3.6+
- C compiler (gcc recommended)
- cJSON library (`libcjson-dev` package)
- Python development headers (`python3-dev` package)

## API Reference

### Module: `json_alchemy`

#### Functions

##### `flatten_json(json_string, use_threads=False, num_threads=0)`

Flattens a nested JSON object into a flat structure.

- **Parameters:**
  - `json_string` (str): The JSON string to flatten
  - `use_threads` (bool, optional): Whether to use multi-threading. Defaults to False.
  - `num_threads` (int, optional): Number of threads to use. If 0, auto-detects. Defaults to 0.
- **Returns:**
  - str: The flattened JSON as a string

##### `flatten_json_batch(json_list, use_threads=True, num_threads=0)`

Flattens a batch of JSON objects into flat structures.

- **Parameters:**
  - `json_list` (list): List of JSON strings to flatten
  - `use_threads` (bool, optional): Whether to use multi-threading. Defaults to True.
  - `num_threads` (int, optional): Number of threads to use. If 0, auto-detects. Defaults to 0.
- **Returns:**
  - list: List of flattened JSON strings

##### `generate_schema(json_string, use_threads=False, num_threads=0)`

Generates a JSON schema from a JSON string.

- **Parameters:**
  - `json_string` (str): The JSON string to generate a schema for
  - `use_threads` (bool, optional): Whether to use multi-threading. Defaults to False.
  - `num_threads` (int, optional): Number of threads to use. If 0, auto-detects. Defaults to 0.
- **Returns:**
  - str: The generated JSON schema as a string

##### `generate_schema_batch(json_list, use_threads=True, num_threads=0)`

Generates a JSON schema from a batch of JSON objects.

- **Parameters:**
  - `json_list` (list): List of JSON strings to generate a schema for
  - `use_threads` (bool, optional): Whether to use multi-threading. Defaults to True.
  - `num_threads` (int, optional): Number of threads to use. If 0, auto-detects. Defaults to 0.
- **Returns:**
  - str: The generated JSON schema as a string

#### Constants

##### `__version__`

The version of the JSON Alchemy library.

## Examples

### Basic Usage

```python
import json
from json_alchemy import flatten_json, generate_schema

# Flatten a JSON object
nested_json = '{"person": {"name": "John", "age": 30}}'
flattened = flatten_json(nested_json)
print(json.loads(flattened))  # {"person.name": "John", "person.age": 30}

# Generate a schema
schema = generate_schema(nested_json)
print(json.loads(schema))
```

### Batch Processing

```python
from json_alchemy import flatten_json_batch, generate_schema_batch

# Process multiple JSON objects at once
json_objects = [
    '{"a": {"b": 1}}',
    '{"x": {"y": {"z": 2}}}'
]

# Flatten batch
flattened_batch = flatten_json_batch(json_objects)
for obj in flattened_batch:
    print(json.loads(obj))

# Generate schema from batch
schema = generate_schema_batch(json_objects)
print(json.loads(schema))
```

### Multi-threading

```python
from json_alchemy import flatten_json_batch

# Create a large batch for testing
json_objects = ['{"a": {"b": 1}}'] * 10000

# Single-threaded
flattened_single = flatten_json_batch(json_objects, use_threads=False)

# Multi-threaded with auto thread count
flattened_multi = flatten_json_batch(json_objects, use_threads=True)

# Multi-threaded with specific thread count
flattened_multi_4 = flatten_json_batch(json_objects, use_threads=True, num_threads=4)
```

## Performance Considerations

The Python bindings provide native C performance while maintaining a simple Python interface. Here are some performance considerations:

1. **Single vs. Batch Processing**: For processing multiple JSON objects, use the batch functions (`flatten_json_batch`, `generate_schema_batch`) instead of calling the single-object functions in a loop.

2. **Multi-threading**: Multi-threading is most beneficial for large batches of JSON objects. For small batches, the overhead of creating and managing threads may outweigh the benefits.

3. **Thread Count**: The optimal thread count depends on your system and the size of the batch. If not specified, the library will auto-detect the optimal thread count based on the available CPU cores and the batch size.

4. **Adaptive Threading**: The library uses adaptive threading to determine when multi-threading is beneficial. If the batch is too small, it will automatically fall back to single-threaded processing.

## Building from Source

If you need to modify the Python bindings or build them from source, follow these steps:

```bash
# Clone the repository
git clone https://github.com/amaye15/AIMv2-rs.git
cd AIMv2-rs/python

# Install build dependencies
pip install setuptools wheel

# Build the package
python setup.py build

# Install the package
python setup.py install
```

## Running Tests

```bash
# Run all tests
cd AIMv2-rs/python
python -m unittest discover tests

# Run a specific test
python -m unittest tests.test_json_alchemy
```

## Contributing

Contributions to the Python bindings are welcome! Please see the main project's contributing guidelines for more information.

## License

The Python bindings are released under the same MIT License as the main project.