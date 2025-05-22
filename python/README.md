# JSON Alchemy Python Bindings

Python bindings for the JSON Alchemy C library, providing high-performance JSON flattening and schema generation.

## Installation

```bash
pip install json-alchemy
```

## Features

- **JSON Flattening**: Convert nested JSON structures into flat key-value pairs
- **JSON Schema Generation**: Generate JSON schemas from JSON objects
- **Multi-threading Support**: Utilize multiple CPU cores for processing large datasets
- **High Performance**: C-based implementation for maximum speed

## Usage

### Flattening JSON

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

### Batch Processing

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

### Schema Generation

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

### Multi-threading

```python
from json_alchemy import flatten_json_batch, generate_schema_batch

# Enable multi-threading with a specific number of threads
flattened = flatten_json_batch(large_json_list, use_threads=True, num_threads=4)

# Auto-detect the optimal number of threads
schema = generate_schema_batch(large_json_list, use_threads=True)
```

## Performance

The C-based implementation provides significant performance benefits compared to pure Python implementations, especially for large datasets. The multi-threading support can further improve performance on multi-core systems.

## Requirements

- Python 3.6+
- libcjson-dev (cJSON library)

## Building from Source

```bash
# Clone the repository
git clone https://github.com/amaye15/AIMv2-rs.git
cd AIMv2-rs/python

# Install the package in development mode
pip install -e .
```

## License

MIT License