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
    remove_empty_strings,
    remove_nulls,
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

    def test_remove_empty_strings(self):
        """Test removing keys with empty string values."""
        test_json = json.dumps(
            {
                "name": "test",
                "empty": "",
                "value": 123,
                "null_field": None,
                "nested": {"empty_nested": "", "data": "valid", "null_nested": None},
                "array": ["", "valid", "", None],
            }
        )

        # Test basic functionality
        result = remove_empty_strings(test_json)
        result_obj = json.loads(result)

        # Should remove empty strings but keep nulls
        self.assertNotIn("empty", result_obj)
        self.assertNotIn("empty_nested", result_obj["nested"])
        self.assertIn("null_field", result_obj)
        self.assertIn("null_nested", result_obj["nested"])
        self.assertEqual(result_obj["name"], "test")
        self.assertEqual(result_obj["value"], 123)
        self.assertEqual(result_obj["nested"]["data"], "valid")

        # Test pretty printing
        pretty_result = remove_empty_strings(test_json, pretty_print=True)
        self.assertIn("\n", pretty_result)  # Should have newlines
        self.assertIn('"name":', pretty_result)

    def test_remove_nulls(self):
        """Test removing keys with null values."""
        test_json = json.dumps(
            {
                "name": "test",
                "empty": "",
                "value": 123,
                "null_field": None,
                "nested": {"empty_nested": "", "data": "valid", "null_nested": None},
                "array": ["", "valid", "", None],
            }
        )

        # Test basic functionality
        result = remove_nulls(test_json)
        result_obj = json.loads(result)

        # Should remove nulls but keep empty strings
        self.assertNotIn("null_field", result_obj)
        self.assertNotIn("null_nested", result_obj["nested"])
        self.assertIn("empty", result_obj)
        self.assertIn("empty_nested", result_obj["nested"])
        self.assertEqual(result_obj["name"], "test")
        self.assertEqual(result_obj["value"], 123)
        self.assertEqual(result_obj["empty"], "")
        self.assertEqual(result_obj["nested"]["empty_nested"], "")

        # Test pretty printing
        pretty_result = remove_nulls(test_json, pretty_print=True)
        self.assertIn("\n", pretty_result)  # Should have newlines
        self.assertIn('"name":', pretty_result)

    def test_remove_functions_edge_cases(self):
        """Test edge cases for remove functions."""
        # Test empty object
        empty_json = "{}"
        self.assertEqual(remove_empty_strings(empty_json), "{}")
        self.assertEqual(remove_nulls(empty_json), "{}")

        # Test object with only empty strings
        only_empty = json.dumps({"a": "", "b": ""})
        result = remove_empty_strings(only_empty)
        self.assertEqual(json.loads(result), {})

        # Test object with only nulls
        only_nulls = json.dumps({"a": None, "b": None})
        result = remove_nulls(only_nulls)
        self.assertEqual(json.loads(result), {})

        # Test nested arrays
        nested_array_json = json.dumps(
            {"data": [{"empty": "", "valid": "test"}, {"null": None, "number": 42}]}
        )

        empty_result = json.loads(remove_empty_strings(nested_array_json))
        self.assertNotIn("empty", empty_result["data"][0])
        self.assertIn("valid", empty_result["data"][0])

        null_result = json.loads(remove_nulls(nested_array_json))
        self.assertNotIn("null", null_result["data"][1])
        self.assertIn("number", null_result["data"][1])

    def test_version(self):
        """Test that version is available."""
        self.assertIsNotNone(__version__)
        self.assertTrue(len(__version__) > 0)


if __name__ == "__main__":
    unittest.main()
