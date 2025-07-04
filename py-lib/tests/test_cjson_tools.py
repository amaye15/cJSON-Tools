#!/usr/bin/env python3
"""
Unit tests for the cJSON-Tools Python bindings.
"""

import json
import unittest

from cjson_tools import (
    __version__,
    flatten_json,
    flatten_json_batch,
    generate_schema,
    generate_schema_batch,
    get_flattened_paths_with_types,
)


class TestCJsonTools(unittest.TestCase):
    """Test cases for cJSON-Tools Python bindings."""

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

    def test_flatten_json(self):
        """Test flattening a single JSON object."""
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

    def test_flatten_json_batch(self):
        """Test flattening a batch of JSON objects."""
        flattened_batch = flatten_json_batch(self.json_batch)

        self.assertEqual(len(flattened_batch), 3)

        obj1 = json.loads(flattened_batch[0])
        obj2 = json.loads(flattened_batch[1])
        obj3 = json.loads(flattened_batch[2])

        self.assertIn("a.b", obj1)
        self.assertEqual(obj1["a.b"], 1)

        self.assertIn("x.y.z", obj2)
        self.assertEqual(obj2["x.y.z"], 2)

        self.assertIn("id", obj3)
        self.assertIn("value", obj3)
        self.assertEqual(obj3["id"], 123)
        self.assertEqual(obj3["value"], True)

    def test_generate_schema(self):
        """Test generating a schema from a single JSON object."""
        schema = generate_schema(self.nested_json)
        schema_obj = json.loads(schema)

        self.assertEqual(schema_obj["type"], "object")
        self.assertIn("properties", schema_obj)
        self.assertIn("person", schema_obj["properties"])
        self.assertEqual(schema_obj["properties"]["person"]["type"], "object")
        self.assertIn("name", schema_obj["properties"]["person"]["properties"])
        self.assertEqual(
            schema_obj["properties"]["person"]["properties"]["name"]["type"], "string"
        )

    def test_generate_schema_batch(self):
        """Test generating a schema from a batch of JSON objects."""
        schema = generate_schema_batch(self.json_batch)
        schema_obj = json.loads(schema)

        self.assertEqual(schema_obj["type"], "object")
        self.assertIn("properties", schema_obj)

        # Check that all properties from all objects are included
        properties = schema_obj["properties"]
        self.assertIn("a", properties)
        self.assertIn("x", properties)
        self.assertIn("id", properties)
        self.assertIn("value", properties)

    def test_threading(self):
        """Test that threading options don't cause errors."""
        # Single-threaded
        flatten_json_batch(self.json_batch, use_threads=False)

        # Multi-threaded with auto thread count
        flatten_json_batch(self.json_batch, use_threads=True)

        # Multi-threaded with specific thread count
        flatten_json_batch(self.json_batch, use_threads=True, num_threads=2)

    def test_get_flattened_paths_with_types(self):
        """Test getting flattened paths with their data types."""
        # Test with nested JSON
        result = get_flattened_paths_with_types(self.nested_json)
        self.assertIsNotNone(result)

        # Parse the result to verify structure
        paths_with_types = json.loads(result)
        self.assertIsInstance(paths_with_types, dict)

        # Check expected paths and types
        expected_paths = {
            "person.name": "string",
            "person.age": "integer",
            "person.address.street": "string",
            "person.address.city": "string",
        }

        for path, expected_type in expected_paths.items():
            self.assertIn(path, paths_with_types)
            self.assertEqual(paths_with_types[path], expected_type)

        # Test with simple JSON including arrays
        simple_json = '{"name": "John", "age": 30, "score": 95.5, "active": true, "data": null, "tags": ["dev", "python"]}'
        result = get_flattened_paths_with_types(simple_json)
        paths_with_types = json.loads(result)

        expected_simple = {
            "name": "string",
            "age": "integer",
            "score": "number",
            "active": "boolean",
            "data": "null",
            "tags[0]": "string",
            "tags[1]": "string",
        }

        for path, expected_type in expected_simple.items():
            self.assertIn(path, paths_with_types)
            self.assertEqual(paths_with_types[path], expected_type)

        # Test pretty printing
        pretty_result = get_flattened_paths_with_types(simple_json, pretty_print=True)
        self.assertIsNotNone(pretty_result)
        self.assertIn("\n", pretty_result)  # Should have newlines for pretty printing

    def test_version(self):
        """Test that version is available."""
        self.assertIsNotNone(__version__)
        self.assertTrue(len(__version__) > 0)


if __name__ == "__main__":
    unittest.main()
