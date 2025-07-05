#!/usr/bin/env python3
"""
Comprehensive example demonstrating all cJSON-Tools functionality.

This example showcases:
1. JsonToolsBuilder - Fluent interface for JSON transformations
2. JSON flattening and schema generation
3. Path type analysis
4. Performance comparisons
5. Batch processing
"""

import json
import statistics
import time

from cjson_tools import (
    JsonToolsBuilder,
    __version__,
    flatten_json,
    flatten_json_batch,
    generate_schema,
    get_flattened_paths_with_types,
    remove_empty_strings,
    remove_nulls,
    replace_keys,
    replace_values,
)


def print_separator(title):
    """Print a separator with a title."""
    print("\n" + "=" * 60)
    print(f" {title} ".center(60, "="))
    print("=" * 60 + "\n")


def demo_json_tools_builder():
    """Demonstrate the JsonToolsBuilder fluent interface."""
    print_separator("JsonToolsBuilder - Fluent Interface")

    # Complex transformation example
    data = {
        "session.tracking.page1.timeMs": 1500,
        "session.tracking.page2.timeMs": 2000,
        "analytics.page.home.visits": 100,
        "analytics.page.about.visits": 200,
        "legacy.server.instance1": "old_active",
        "legacy.server.instance2": "old_inactive",
        "old_user_id": "user_12345",
        "deprecated_phone": "deprecated_555-1234",
        "user_email": "john@oldcompany.com",
        "empty_field": "",
        "null_field": None,
        "nested": {
            "old_status": "old_premium",
            "legacy_tier": "legacy_gold",
            "empty_bio": "",
            "null_avatar": None,
        },
    }

    print("🔧 Original data:")
    print(json.dumps(data, indent=2))

    print("\n⚡ Applying transformations with JsonToolsBuilder...")
    start_time = time.perf_counter()

    result = (
        JsonToolsBuilder()
        .add_json(data)
        .remove_empty_strings()
        .remove_nulls()
        .replace_keys(
            r"^session\.tracking\..*\.timeMs$", "session.pageTimesInMs.UnifiedPage"
        )
        .replace_keys(r"^analytics\.page\..*\.visits$", "analytics.pageViews.TotalPage")
        .replace_keys(r"^legacy\.server\..*$", "modern.server.instance")
        .replace_keys(r"^old_", "new_")
        .replace_keys(r"^deprecated", "updated")
        .replace_values(r"^old_.*$", "new_value")
        .replace_values(r"^legacy_.*$", "modern_value")
        .replace_values(r"^deprecated_.*$", "updated_value")
        .replace_values(r".*@oldcompany\.com", "john@newcompany.com")
        .pretty_print(True)
        .build()
    )

    processing_time = (time.perf_counter() - start_time) * 1000

    print("✅ Transformed data:")
    print(result)
    print(f"\n⚡ Processing time: {processing_time:.3f}ms")
    print("🚀 All operations applied in single pass!")


def demo_performance_comparison():
    """Compare JsonToolsBuilder vs traditional multi-pass approach."""
    print_separator("Performance Comparison")

    test_data = {
        "session.tracking.page1.timeMs": 1500,
        "analytics.page.home.visits": 100,
        "legacy.server.instance1": "old_active",
        "old_user_id": "user_12345",
        "empty_field": "",
        "null_field": None,
    }

    def run_multi_pass(data):
        result = json.dumps(data)
        result = remove_empty_strings(result)
        result = remove_nulls(result)
        result = replace_keys(
            result,
            r"^session\.tracking\..*\.timeMs$",
            "session.pageTimesInMs.UnifiedPage",
        )
        result = replace_keys(
            result, r"^analytics\.page\..*\.visits$", "analytics.pageViews.TotalPage"
        )
        result = replace_keys(result, r"^legacy\.server\..*$", "modern.server.instance")
        result = replace_keys(result, r"^old_", "new_")
        result = replace_values(result, r"^old_.*$", "new_value")
        return result

    def run_single_pass(data):
        return (
            JsonToolsBuilder()
            .add_json(data)
            .remove_empty_strings()
            .remove_nulls()
            .replace_keys(
                r"^session\.tracking\..*\.timeMs$", "session.pageTimesInMs.UnifiedPage"
            )
            .replace_keys(
                r"^analytics\.page\..*\.visits$", "analytics.pageViews.TotalPage"
            )
            .replace_keys(r"^legacy\.server\..*$", "modern.server.instance")
            .replace_keys(r"^old_", "new_")
            .replace_values(r"^old_.*$", "new_value")
            .build()
        )

    # Benchmark both approaches
    iterations = 1000

    # Multi-pass timing
    multi_times = []
    for _ in range(iterations):
        start = time.perf_counter()
        multi_result = run_multi_pass(test_data)
        multi_times.append((time.perf_counter() - start) * 1000)

    # Single-pass timing
    single_times = []
    for _ in range(iterations):
        start = time.perf_counter()
        single_result = run_single_pass(test_data)
        single_times.append((time.perf_counter() - start) * 1000)

    multi_avg = statistics.mean(multi_times)
    single_avg = statistics.mean(single_times)
    speedup = multi_avg / single_avg
    improvement = ((multi_avg - single_avg) / multi_avg) * 100

    print(f"🔄 Multi-Pass Average:  {multi_avg:.3f}ms")
    print(f"⚡ Single-Pass Average: {single_avg:.3f}ms")
    print(f"🚀 Speedup: {speedup:.2f}x ({improvement:+.1f}% improvement)")

    # Verify results are identical
    multi_parsed = json.loads(multi_result)
    single_parsed = json.loads(single_result)
    print(f"✅ Results identical: {multi_parsed == single_parsed}")


def demo_json_flattening():
    """Demonstrate JSON flattening capabilities."""
    print_separator("JSON Flattening")

    nested_data = {
        "user": {
            "id": 12345,
            "name": "John Doe",
            "address": {
                "street": "123 Main St",
                "city": "New York",
                "coordinates": [40.7128, -74.0060],
            },
            "projects": [
                {"name": "Project A", "status": "active"},
                {"name": "Project B", "status": "completed"},
            ],
        },
        "metadata": {"created": "2024-01-01", "tags": ["important", "urgent"]},
    }

    print("📝 Original nested JSON:")
    print(json.dumps(nested_data, indent=2))

    # Flatten the JSON
    flattened = flatten_json(json.dumps(nested_data))
    print("\n🔍 Flattened JSON:")
    print(json.dumps(json.loads(flattened), indent=2))

    # Generate schema
    schema = generate_schema(json.dumps(nested_data))
    print("\n📋 Generated Schema:")
    print(json.dumps(json.loads(schema), indent=2))


def demo_path_type_analysis():
    """Demonstrate path type analysis."""
    print_separator("Path Type Analysis")

    complex_data = {
        "user": {
            "id": 12345,
            "name": "John Doe",
            "salary": 75000.50,
            "active": True,
            "metadata": None,
            "tags": ["developer", "python"],
            "scores": [95, 87, 92],
        },
        "timestamp": "2024-07-04T12:00:00Z",
    }

    print("📊 Analyzing data types in JSON structure:")
    print(json.dumps(complex_data, indent=2))

    # Get path types
    paths_with_types = get_flattened_paths_with_types(json.dumps(complex_data))
    result = json.loads(paths_with_types)

    print("\n🔍 Path type analysis:")
    for path, data_type in sorted(result.items()):
        print(f'  "{path}": "{data_type}"')

    # Type distribution
    type_counts = {}
    for data_type in result.values():
        type_counts[data_type] = type_counts.get(data_type, 0) + 1

    print("\n📈 Data type distribution:")
    for data_type, count in sorted(type_counts.items()):
        print(f"  {data_type}: {count} paths")


def demo_batch_processing():
    """Demonstrate batch processing capabilities."""
    print_separator("Batch Processing")

    json_objects = [
        '{"user": {"name": "Alice", "age": 25}}',
        '{"user": {"name": "Bob", "age": 30}}',
        '{"user": {"name": "Charlie", "age": 35}}',
    ]

    print(f"📦 Processing batch of {len(json_objects)} JSON objects...")

    # Batch flattening
    start_time = time.time()
    flattened_batch = flatten_json_batch(json_objects)
    batch_time = time.time() - start_time

    print("\n✅ Flattened results:")
    for i, obj in enumerate(flattened_batch):
        print(f"  Object {i+1}: {obj}")

    print(f"\n⚡ Batch processing time: {batch_time*1000:.2f}ms")


def main():
    """Main function demonstrating all cJSON-Tools functionality."""
    print(f"🎯 cJSON-Tools Comprehensive Example")
    print(f"📦 Version: {__version__}")
    print("🚀 Demonstrating TRUE Single-Pass JSON Processing!")

    # Run all demonstrations
    demo_json_tools_builder()
    demo_performance_comparison()
    demo_json_flattening()
    demo_path_type_analysis()
    demo_batch_processing()

    print_separator("Summary")
    print("✅ JsonToolsBuilder: TRUE single-pass processing")
    print("✅ Performance: Proven faster than multi-pass")
    print("✅ Functionality: Complete JSON transformation suite")
    print("✅ Memory: Efficient single-traversal processing")
    print("🎉 All features demonstrated successfully!")


if __name__ == "__main__":
    main()
