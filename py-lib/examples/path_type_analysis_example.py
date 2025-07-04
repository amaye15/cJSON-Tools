#!/usr/bin/env python3
"""
Example demonstrating the new get_flattened_paths_with_types function.

This example shows how to use the path type analysis feature to:
1. Discover data types in complex JSON structures
2. Analyze array elements with indexing
3. Generate type mappings for schema discovery
4. Compare with traditional flattening
"""

import json
import time

import cjson_tools


def main():
    print("🎯 cJSON-Tools Path Type Analysis Example")
    print("=" * 60)

    # Example 1: Complex nested JSON with various data types
    print("\n📝 Example 1: Complex Data Structure")
    complex_data = {
        "user": {
            "id": 12345,
            "name": "John Doe",
            "email": "john.doe@example.com",
            "age": 30,
            "salary": 75000.50,
            "active": True,
            "metadata": None,
            "tags": ["developer", "python", "json"],
            "scores": [95, 87, 92, 88],
            "address": {
                "street": "123 Main St",
                "city": "New York",
                "coordinates": [40.7128, -74.0060],
                "verified": True,
            },
            "projects": [
                {"name": "Project A", "status": "active", "priority": 1},
                {"name": "Project B", "status": "completed", "priority": 2},
            ],
        },
        "timestamp": "2024-07-04T12:00:00Z",
        "version": 1.2,
    }

    print("Input JSON structure:")
    print(json.dumps(complex_data, indent=2))

    # Get flattened paths with types
    print("\n🔍 Flattened Paths with Data Types:")
    start_time = time.time()
    paths_with_types = cjson_tools.get_flattened_paths_with_types(
        json.dumps(complex_data)
    )
    analysis_time = time.time() - start_time

    result = json.loads(paths_with_types)
    for path, data_type in sorted(result.items()):
        print(f'  "{path}": "{data_type}"')

    print(f"\n⚡ Analysis completed in {analysis_time:.6f} seconds")

    # Example 2: Compare with traditional flattening
    print("\n📊 Example 2: Comparison with Traditional Flattening")

    # Traditional flattening
    flattened = cjson_tools.flatten_json(json.dumps(complex_data))
    flattened_result = json.loads(flattened)

    print("\nTraditional flattening (values):")
    for path, value in sorted(list(flattened_result.items())[:5]):  # Show first 5
        print(f'  "{path}": {json.dumps(value)}')
    print("  ... (and more)")

    print("\nPath type analysis (types):")
    for path, data_type in sorted(list(result.items())[:5]):  # Show first 5
        print(f'  "{path}": "{data_type}"')
    print("  ... (and more)")

    # Example 3: Array handling demonstration
    print("\n📋 Example 3: Array Handling")
    array_data = {
        "users": [
            {"name": "Alice", "age": 25},
            {"name": "Bob", "age": 30},
            {"name": "Charlie", "age": 35},
        ],
        "scores": [100, 95, 87, 92],
        "tags": ["important", "urgent", "review"],
    }

    print("Array data:")
    print(json.dumps(array_data, indent=2))

    array_paths = cjson_tools.get_flattened_paths_with_types(json.dumps(array_data))
    array_result = json.loads(array_paths)

    print("\nArray elements with indexed paths:")
    for path, data_type in sorted(array_result.items()):
        print(f'  "{path}": "{data_type}"')

    # Example 4: Performance comparison
    print("\n⚡ Example 4: Performance Analysis")

    # Generate larger dataset
    large_data = {
        "records": [
            {
                "id": i,
                "name": f"User {i}",
                "score": i * 1.5,
                "active": i % 2 == 0,
                "tags": [f"tag{j}" for j in range(3)],
                "metadata": {"created": f"2024-01-{i%30+1:02d}", "priority": i % 5},
            }
            for i in range(100)
        ]
    }

    print(f"Testing with {len(large_data['records'])} records...")

    # Test path type analysis
    start_time = time.time()
    large_paths = cjson_tools.get_flattened_paths_with_types(json.dumps(large_data))
    type_analysis_time = time.time() - start_time

    # Test traditional flattening
    start_time = time.time()
    large_flattened = cjson_tools.flatten_json(json.dumps(large_data))
    flattening_time = time.time() - start_time

    large_paths_result = json.loads(large_paths)
    large_flattened_result = json.loads(large_flattened)

    print(
        f"Path type analysis: {type_analysis_time:.6f}s ({len(large_paths_result)} unique paths)"
    )
    print(
        f"Traditional flattening: {flattening_time:.6f}s ({len(large_flattened_result)} total paths)"
    )
    print(
        f"Type analysis is {flattening_time/type_analysis_time:.1f}x faster for path discovery!"
    )

    # Example 5: Data type summary
    print("\n📈 Example 5: Data Type Summary")

    # Count data types
    type_counts = {}
    for data_type in large_paths_result.values():
        type_counts[data_type] = type_counts.get(data_type, 0) + 1

    print("Data type distribution:")
    for data_type, count in sorted(type_counts.items()):
        print(f"  {data_type}: {count} paths")

    print("\n✅ Path Type Analysis Examples Complete!")
    print("\n🎯 Use Cases:")
    print("  • Schema discovery and validation")
    print("  • Data type profiling and analysis")
    print("  • API documentation generation")
    print("  • Data migration planning")
    print("  • JSON structure exploration")


if __name__ == "__main__":
    main()
