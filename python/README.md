# JSON Alchemy

Python bindings for the JSON Alchemy C library - a high-performance JSON processing toolkit.

## Features

- **JSON Flattening**: Convert nested JSON objects to flat key-value pairs
- **JSON Schema Generation**: Generate JSON schemas from JSON objects
- **Batch Processing**: Process multiple JSON objects in a single operation
- **Multi-threading**: Utilize multiple CPU cores for improved performance

## Installation

```bash
pip install aimv2-json-alchemy
```

### Requirements

- Python 3.8 or higher
- Python development headers (python3-dev on Linux systems)

The cJSON library is now included directly in the repository, so you don't need to install it separately.

For more detailed installation instructions, see [INSTALL.md](INSTALL.md).

## Usage

### JSON Flattening

```python
import json
from json_alchemy import flatten_json, flatten_json_batch

# Single object flattening
nested_json_str = '''
{
    "person": {
        "name": "John",
        "age": 30,
        "address": {
            "city": "New York",
            "zip": "10001"
        }
    }
}
'''

flat_json = flatten_json(nested_json_str)
print(flat_json)
# Output:
# {
#   "person.name": "John",
#   "person.age": 30,
#   "person.address.city": "New York",
#   "person.address.zip": "10001"
# }

# Batch processing
json_objects = [
    '{"a": {"b": 1}}',
    '{"c": {"d": 2}}'
]

# Single-threaded batch processing
flattened_batch = flatten_json_batch(json_objects)

# Multi-threaded batch processing (auto thread count)
flattened_batch_mt = flatten_json_batch(json_objects, use_threads=True)

# Multi-threaded batch processing (specific thread count)
flattened_batch_mt_spec = flatten_json_batch(json_objects, use_threads=True, num_threads=4)
```

### JSON Schema Generation

```python
from json_alchemy import generate_schema, generate_schema_batch

# Single object schema generation
json_obj_str = '''
{
    "name": "John",
    "age": 30,
    "is_active": true,
    "scores": [85, 90, 78],
    "address": {
        "city": "New York",
        "zip": "10001"
    }
}
'''

schema = generate_schema(json_obj_str)
print(schema)
# Output:
# {
#   "type": "object",
#   "properties": {
#     "name": {"type": "string"},
#     "age": {"type": "integer"},
#     "is_active": {"type": "boolean"},
#     "scores": {
#       "type": "array",
#       "items": {"type": "integer"}
#     },
#     "address": {
#       "type": "object",
#       "properties": {
#         "city": {"type": "string"},
#         "zip": {"type": "string"}
#       }
#     }
#   }
# }

# Batch processing
json_objects = [
    '{"a": 1, "b": "string"}',
    '{"a": 2, "c": true}'
]

# Single-threaded batch processing
schema_batch = generate_schema_batch(json_objects)

# Multi-threaded batch processing (auto thread count)
schema_batch_mt = generate_schema_batch(json_objects, use_threads=True)

# Multi-threaded batch processing (specific thread count)
schema_batch_mt_spec = generate_schema_batch(json_objects, use_threads=True, num_threads=4)
```

## Performance Considerations

Based on our benchmarks:

1. **Batch Processing**:
   - For small to medium batches (10-1000 objects), batch processing provides significant speedups.
   - For larger batches (5000+ objects), consider splitting them into smaller batches.

2. **Multi-threading**:
   - Multi-threading provides significant speedups for medium-sized batches (around 1000 objects).
   - For very small batches, the overhead of thread creation may outweigh the benefits.

3. **Optimal Batch Size**:
   - The optimal batch size for flattening operations is around 1000 objects.
   - The optimal batch size for schema generation is around 10-100 objects.

## Building from Source

```bash
# Clone the repository
git clone https://github.com/amaye15/AIMv2-rs.git
cd AIMv2-rs/python

# Install the package in development mode
pip install -e .
```

If you encounter any issues during installation, please refer to the [INSTALL.md](INSTALL.md) file for troubleshooting.

## License

MIT License

## Links

- [GitHub Repository](https://github.com/amaye15/AIMv2-rs)
- [Documentation](https://github.com/amaye15/AIMv2-rs/blob/main/python/PYTHON.md)
- [Benchmarks](https://github.com/amaye15/AIMv2-rs/blob/main/python/benchmarks/README.md)