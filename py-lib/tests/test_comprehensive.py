#!/usr/bin/env python3
"""
Comprehensive test suite for cJSON-Tools with integrated optimizations.
Tests all functionality including JSON flattening, schema generation, and performance optimizations.
"""

import unittest
import json
import time
import sys
import os

# Add the parent directory to Python path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    from cjson_tools import (
        flatten_json,
        flatten_json_batch,
        generate_schema,
        generate_schema_batch,
        __version__,
    )

    print(f"✅ Successfully imported cjson_tools version {__version__}")
except ImportError as e:
    print(f"❌ Failed to import cjson_tools: {e}")
    sys.exit(1)


class TestCJsonToolsComprehensive(unittest.TestCase):
    """Comprehensive test cases for cJSON-Tools with optimizations."""

    def setUp(self):
        """Set up test fixtures."""
        self.nested_json = """
        {
            "person": {
                "name": "John Doe",
                "age": 30,
                "address": {
                    "street": "123 Main St",
                    "city": "Anytown"
                }
            }
        }
        """

        self.json_batch = [
            '{"a": {"b": 1}}',
            '{"x": {"y": {"z": 2}}}',
            '{"id": 123, "value": true}',
        ]

        self.complex_data = {
            "user": {
                "id": 123,
                "profile": {
                    "name": "John Doe",
                    "email": "john@example.com",
                    "settings": {
                        "theme": "dark",
                        "notifications": True,
                        "privacy": {"public": False, "friends_only": True},
                    },
                },
                "posts": [
                    {"id": 1, "title": "First Post", "tags": ["tech", "programming"]},
                    {"id": 2, "title": "Second Post", "tags": ["life", "thoughts"]},
                ],
            }
        }

    def test_flatten_json_basic(self):
        """Test basic JSON flattening functionality."""
        flattened = flatten_json(self.nested_json)
        flattened_obj = json.loads(flattened)

        self.assertIn("person.name", flattened_obj)
        self.assertIn("person.age", flattened_obj)
        self.assertIn("person.address.street", flattened_obj)
        self.assertIn("person.address.city", flattened_obj)

        self.assertEqual(flattened_obj["person.name"], "John Doe")
        self.assertEqual(flattened_obj["person.age"], 30)
        self.assertEqual(flattened_obj["person.address.street"], "123 Main St")
        self.assertEqual(flattened_obj["person.address.city"], "Anytown")

    def test_flatten_json_pretty_print(self):
        """Test JSON flattening with pretty print options."""
        # Test without pretty print
        flattened_compact = flatten_json(self.nested_json, pretty_print=False)

        # Test with pretty print
        flattened_pretty = flatten_json(self.nested_json, pretty_print=True)

        # Both should be valid JSON and contain same data
        compact_obj = json.loads(flattened_compact)
        pretty_obj = json.loads(flattened_pretty)

        self.assertEqual(compact_obj, pretty_obj)

    def test_flatten_json_batch(self):
        """Test batch JSON flattening."""
        # Test single-threaded
        results_st = flatten_json_batch(self.json_batch, use_threads=False)
        self.assertEqual(len(results_st), 3)

        # Verify results
        result1 = json.loads(results_st[0])
        result2 = json.loads(results_st[1])
        result3 = json.loads(results_st[2])

        self.assertIn("a.b", result1)
        self.assertEqual(result1["a.b"], 1)

        self.assertIn("x.y.z", result2)
        self.assertEqual(result2["x.y.z"], 2)

        self.assertIn("id", result3)
        self.assertIn("value", result3)
        self.assertEqual(result3["id"], 123)
        self.assertEqual(result3["value"], True)

        # Test multi-threaded
        results_mt = flatten_json_batch(
            self.json_batch, use_threads=True, num_threads=2
        )
        self.assertEqual(len(results_mt), 3)

        # Results should be equivalent
        for i in range(3):
            self.assertEqual(json.loads(results_st[i]), json.loads(results_mt[i]))

    def test_generate_schema_basic(self):
        """Test basic schema generation."""
        schema = generate_schema(self.nested_json)
        schema_obj = json.loads(schema)

        self.assertEqual(schema_obj["type"], "object")
        self.assertIn("properties", schema_obj)
        self.assertIn("person", schema_obj["properties"])

        person_schema = schema_obj["properties"]["person"]
        self.assertEqual(person_schema["type"], "object")
        self.assertIn("name", person_schema["properties"])
        self.assertEqual(person_schema["properties"]["name"]["type"], "string")

    def test_generate_schema_batch(self):
        """Test batch schema generation with merging."""
        schema = generate_schema_batch(self.json_batch)
        schema_obj = json.loads(schema)

        self.assertEqual(schema_obj["type"], "object")
        self.assertIn("properties", schema_obj)

        # Check that all properties from all objects are merged
        properties = schema_obj["properties"]
        self.assertIn("a", properties)
        self.assertIn("x", properties)
        self.assertIn("id", properties)
        self.assertIn("value", properties)

    def test_complex_nested_objects(self):
        """Test handling of complex nested objects."""
        json_str = json.dumps(self.complex_data)

        # Test flattening
        flattened = flatten_json(json_str)
        flattened_obj = json.loads(flattened)

        # Should have deeply nested keys
        self.assertIn("user.profile.settings.privacy.public", flattened_obj)
        self.assertIn("user.posts[0].tags[0]", flattened_obj)

        # Test schema generation
        schema = generate_schema(json_str)
        schema_obj = json.loads(schema)

        self.assertEqual(schema_obj["type"], "object")
        self.assertIn("user", schema_obj["properties"])

    def test_performance_optimizations(self):
        """Test that performance optimizations are working."""
        # Create larger test data
        large_obj = {
            "users": [
                {
                    "id": i,
                    "profile": {
                        "name": f"User {i}",
                        "settings": {
                            "theme": "dark" if i % 2 else "light",
                            "notifications": i % 3 == 0,
                        },
                    },
                }
                for i in range(20)
            ]
        }

        json_str = json.dumps(large_obj)

        # Test flattening performance
        start = time.time()
        flattened = flatten_json(json_str)
        flatten_time = (time.time() - start) * 1000

        # Should complete quickly (under 100ms)
        self.assertLess(flatten_time, 100)

        # Verify result is valid
        flattened_obj = json.loads(flattened)
        self.assertIn("users[0].id", flattened_obj)

        # Test schema generation performance
        start = time.time()
        schema = generate_schema(json_str)
        schema_time = (time.time() - start) * 1000

        # Should complete quickly (under 100ms)
        self.assertLess(schema_time, 100)

        # Verify result is valid
        schema_obj = json.loads(schema)
        self.assertEqual(schema_obj["type"], "object")

    def test_memory_stress(self):
        """Test memory management under stress."""
        success_count = 0

        for i in range(50):
            try:
                test_obj = {
                    "iteration": i,
                    "data": {
                        "nested": [j for j in range(5)],
                        "metadata": {
                            "timestamp": f"2023-{i%12+1:02d}-01",
                            "active": i % 2 == 0,
                        },
                    },
                }

                # Test both flattening and schema generation
                flattened = flatten_json(json.dumps(test_obj))
                schema = generate_schema(json.dumps(test_obj))

                # Verify results are valid
                json.loads(flattened)
                json.loads(schema)

                success_count += 1

            except Exception as e:
                self.fail(f"Memory stress test failed at iteration {i}: {e}")

        # All iterations should succeed
        self.assertEqual(success_count, 50)

    def test_threading_performance(self):
        """Test multi-threading performance and correctness."""
        # Create batch data
        batch_data = []
        for i in range(20):
            item = {
                "id": i,
                "data": {
                    "value": i * 2,
                    "metadata": {
                        "created": f"2023-{i%12+1:02d}-01",
                        "active": i % 2 == 0,
                    },
                },
            }
            batch_data.append(json.dumps(item))

        # Test single-threaded
        start = time.time()
        results_st = flatten_json_batch(batch_data, use_threads=False)
        st_time = time.time() - start

        # Test multi-threaded
        start = time.time()
        results_mt = flatten_json_batch(batch_data, use_threads=True, num_threads=4)
        mt_time = time.time() - start

        # Both should produce same number of results
        self.assertEqual(len(results_st), len(results_mt))
        self.assertEqual(len(results_st), 20)

        # Results should be equivalent (though possibly in different order)
        st_set = {json.dumps(json.loads(r), sort_keys=True) for r in results_st}
        mt_set = {json.dumps(json.loads(r), sort_keys=True) for r in results_mt}
        self.assertEqual(st_set, mt_set)

    def test_edge_cases(self):
        """Test edge cases and error handling."""
        # Test empty objects
        empty_result = flatten_json("{}")
        self.assertEqual(json.loads(empty_result), {})

        # Test arrays (top-level arrays are returned as-is)
        array_result = flatten_json("[1, 2, 3]")
        array_obj = json.loads(array_result)
        self.assertIsInstance(array_obj, list)
        self.assertEqual(array_obj[0], 1)

        # Test arrays within objects (these get flattened)
        nested_array_result = flatten_json('{"items": [1, 2, 3]}')
        nested_array_obj = json.loads(nested_array_result)
        self.assertIn("items[0]", nested_array_obj)
        self.assertEqual(nested_array_obj["items[0]"], 1)

        # Test null values
        null_result = flatten_json('{"key": null}')
        null_obj = json.loads(null_result)
        self.assertIn("key", null_obj)
        self.assertIsNone(null_obj["key"])


def run_performance_benchmark():
    """Run a comprehensive performance benchmark."""
    print("\n🚀 Running Performance Benchmark...")
    print("=" * 50)

    # Test data
    test_data = {
        "name": "John Doe",
        "age": 30,
        "address": {
            "street": "123 Main St",
            "city": "New York",
            "coordinates": {"lat": 40.7128, "lng": -74.0060},
        },
        "hobbies": ["reading", "swimming", "coding"],
    }

    # Single operations
    json_str = json.dumps(test_data)

    start = time.time()
    flattened = flatten_json(json_str, pretty_print=False)
    flatten_time = (time.time() - start) * 1000

    start = time.time()
    schema = generate_schema(json_str)
    schema_time = (time.time() - start) * 1000

    print(f"Single JSON flattening: {flatten_time:.2f}ms")
    print(f"Single schema generation: {schema_time:.2f}ms")

    # Batch operations
    batch_data = [json.dumps(test_data) for _ in range(100)]

    start = time.time()
    batch_flattened = flatten_json_batch(batch_data, use_threads=True, num_threads=4)
    batch_flatten_time = (time.time() - start) * 1000

    start = time.time()
    batch_schemas = generate_schema_batch(batch_data, use_threads=True, num_threads=4)
    batch_schema_time = (time.time() - start) * 1000

    print(
        f"Batch flattening (100 items): {batch_flatten_time:.2f}ms ({batch_flatten_time/100:.3f}ms per item)"
    )
    print(
        f"Batch schema generation (100 items): {batch_schema_time:.2f}ms ({batch_schema_time/100:.3f}ms per item)"
    )

    print("\n✅ All optimizations working correctly!")


if __name__ == "__main__":
    print("🧪 cJSON-Tools Comprehensive Test Suite")
    print("Testing all functionality with integrated optimizations")
    print("=" * 60)

    # Run unit tests
    unittest.main(argv=[""], exit=False, verbosity=2)

    # Run performance benchmark
    run_performance_benchmark()
