#!/usr/bin/env python3
"""
Example usage of the cJSON-Tools Python bindings.
"""

import json
import time

from cjson_tools import (
    __version__,
    flatten_json,
    flatten_json_batch,
    generate_schema,
    generate_schema_batch,
)


def print_separator(title):
    """Print a separator with a title."""
    print("\n" + "=" * 50)
    print(f" {title} ".center(50, "="))
    print("=" * 50 + "\n")


def main():
    """Main function demonstrating cJSON-Tools functionality."""
    print(f"cJSON-Tools version: {__version__}")

    # Example 1: Flatten a single JSON object
    print_separator("Flatten Single JSON Object")

    nested_json = """
    {
        "person": {
            "name": "John Doe",
            "age": 30,
            "address": {
                "street": "123 Main St",
                "city": "Anytown",
                "zip": "12345"
            },
            "contacts": [
                {"type": "email", "value": "john@example.com"},
                {"type": "phone", "value": "555-1234"}
            ]
        },
        "metadata": {
            "created": "2023-01-01",
            "tags": ["personal", "contact"]
        }
    }
    """

    print("Original JSON:")
    print(json.dumps(json.loads(nested_json), indent=2))

    flattened = flatten_json(nested_json)
    print("\nFlattened JSON:")
    print(json.dumps(json.loads(flattened), indent=2))

    # Example 2: Generate schema from a single JSON object
    print_separator("Generate Schema from Single JSON Object")

    schema = generate_schema(nested_json)
    print("Generated Schema:")
    print(json.dumps(json.loads(schema), indent=2))

    # Example 3: Batch processing
    print_separator("Batch Processing")

    json_objects = [
        '{"a": {"b": 1, "c": "text"}}',
        '{"x": {"y": {"z": 2}}, "array": [1, 2, 3]}',
        '{"id": 123, "nested": {"value": true, "nullable": null}}',
    ]

    print("Original JSON objects:")
    for obj in json_objects:
        print(json.dumps(json.loads(obj), indent=2))
        print()

    # Flatten batch
    start_time = time.time()
    flattened_batch = flatten_json_batch(json_objects)
    end_time = time.time()

    print("\nFlattened JSON objects:")
    for obj in flattened_batch:
        print(json.dumps(json.loads(obj), indent=2))
        print()

    print(f"Batch flattening time: {(end_time - start_time) * 1000:.2f} ms")

    # Generate schema from batch
    print_separator("Generate Schema from Batch")

    start_time = time.time()
    batch_schema = generate_schema_batch(json_objects)
    end_time = time.time()

    print("Generated Schema from batch:")
    print(json.dumps(json.loads(batch_schema), indent=2))
    print(f"Batch schema generation time: {(end_time - start_time) * 1000:.2f} ms")

    # Example 4: Multi-threading performance comparison
    print_separator("Multi-threading Performance Comparison")

    # Create a larger batch for testing
    large_batch = json_objects * 1000

    print(f"Processing batch of {len(large_batch)} JSON objects...")

    # Single-threaded
    start_time = time.time()
    flatten_json_batch(large_batch, use_threads=False)
    single_thread_time = time.time() - start_time

    print(f"Single-threaded time: {single_thread_time:.4f} seconds")

    # Multi-threaded
    start_time = time.time()
    flatten_json_batch(large_batch, use_threads=True)
    multi_thread_time = time.time() - start_time

    print(f"Multi-threaded time: {multi_thread_time:.4f} seconds")
    print(f"Speedup: {single_thread_time / multi_thread_time:.2f}x")


if __name__ == "__main__":
    main()
