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
    replace_keys,
    replace_values,
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

    def test_replace_keys(self):
        """Test replace_keys function"""
        # Basic key replacement
        test_data = {
            "session.pageTimesInMs.HomePage": 1500,
            "session.pageTimesInMs.AboutPage": 2000,
            "session.pageTimesInMs.ContactPage": 1200,
            "user.name": "John",
            "user.email": "john@example.com"
        }
        test_json = json.dumps(test_data)

        # Replace session.pageTimesInMs.* with session.pageTimesInMs.PrezzePage
        result = replace_keys(
            test_json,
            r"^session\.pageTimesInMs\..*$",
            "session.pageTimesInMs.PrezzePage"
        )
        result_data = json.loads(result)

        # All session.pageTimesInMs.* keys should be replaced
        expected_keys = {
            "session.pageTimesInMs.PrezzePage",
            "user.name",
            "user.email"
        }
        self.assertEqual(set(result_data.keys()), expected_keys)

        # Values should be preserved (though multiple keys will have same name)
        self.assertIn("session.pageTimesInMs.PrezzePage", result_data)
        self.assertEqual(result_data["user.name"], "John")
        self.assertEqual(result_data["user.email"], "john@example.com")

    def test_replace_keys_nested(self):
        """Test replace_keys with nested objects"""
        test_data = {
            "metrics": {
                "session.timing.load": 100,
                "session.timing.render": 200,
                "user.action.click": 5,
                "user.action.scroll": 10
            },
            "config": {
                "session.setting.theme": "dark",
                "user.preference.lang": "en"
            }
        }
        test_json = json.dumps(test_data)

        # Replace all session.* keys with session.unified
        result = replace_keys(
            test_json,
            r"^session\..*$",
            "session.unified"
        )
        result_data = json.loads(result)

        # Check that nested session keys were replaced
        self.assertIn("session.unified", result_data["metrics"])
        self.assertIn("session.unified", result_data["config"])

        # User keys should remain unchanged
        self.assertIn("user.action.click", result_data["metrics"])
        self.assertIn("user.preference.lang", result_data["config"])

    def test_replace_keys_edge_cases(self):
        """Test edge cases for replace_keys function"""
        # Empty object
        empty_obj = "{}"
        result = replace_keys(empty_obj, r".*", "replacement")
        self.assertEqual(result, "{}")

        # No matching keys
        test_data = '{"user.name": "John", "user.email": "john@example.com"}'
        result = replace_keys(test_data, r"^session\..*$", "session.page")
        result_data = json.loads(result)
        expected_data = json.loads(test_data)
        self.assertEqual(result_data, expected_data)

        # Pretty print option
        result_pretty = replace_keys(
            test_data,
            r"^user\..*$",
            "user.info",
            pretty_print=True
        )
        self.assertIn("\n", result_pretty)  # Should contain newlines for pretty printing

    def test_replace_values(self):
        """Test replace_values function"""
        # Basic value replacement
        test_data = {
            "user": {
                "status": "old_active",
                "role": "old_admin",
                "name": "John"
            },
            "config": {
                "theme": "old_dark",
                "language": "en"
            }
        }
        test_json = json.dumps(test_data)

        # Replace all values starting with "old_" with "new_value"
        result = replace_values(
            test_json,
            r"^old_.*$",
            "new_value"
        )
        result_data = json.loads(result)

        # All old_* values should be replaced
        self.assertEqual(result_data["user"]["status"], "new_value")
        self.assertEqual(result_data["user"]["role"], "new_value")
        self.assertEqual(result_data["config"]["theme"], "new_value")

        # Non-matching values should remain unchanged
        self.assertEqual(result_data["user"]["name"], "John")
        self.assertEqual(result_data["config"]["language"], "en")

    def test_replace_values_nested(self):
        """Test replace_values with nested objects and arrays"""
        test_data = {
            "users": [
                {
                    "name": "John",
                    "status": "temp_active",
                    "role": "temp_user"
                },
                {
                    "name": "Jane",
                    "status": "temp_inactive",
                    "role": "admin"
                }
            ],
            "settings": {
                "mode": "temp_debug",
                "version": "1.0"
            }
        }
        test_json = json.dumps(test_data)

        # Replace all values starting with "temp_" with "permanent"
        result = replace_values(
            test_json,
            r"^temp_.*$",
            "permanent"
        )
        result_data = json.loads(result)

        # Check that nested temp_ values were replaced
        self.assertEqual(result_data["users"][0]["status"], "permanent")
        self.assertEqual(result_data["users"][0]["role"], "permanent")
        self.assertEqual(result_data["users"][1]["status"], "permanent")
        self.assertEqual(result_data["settings"]["mode"], "permanent")

        # Non-matching values should remain unchanged
        self.assertEqual(result_data["users"][0]["name"], "John")
        self.assertEqual(result_data["users"][1]["name"], "Jane")
        self.assertEqual(result_data["users"][1]["role"], "admin")
        self.assertEqual(result_data["settings"]["version"], "1.0")

    def test_replace_values_edge_cases(self):
        """Test edge cases for replace_values function"""
        # Empty object
        empty_obj = "{}"
        result = replace_values(empty_obj, r".*", "replacement")
        self.assertEqual(result, "{}")

        # No matching values
        test_data = '{"name": "John", "age": 30, "active": true}'
        result = replace_values(test_data, r"^temp_.*$", "replacement")
        result_data = json.loads(result)
        expected_data = json.loads(test_data)
        self.assertEqual(result_data, expected_data)

        # Non-string values should not be affected
        test_data_mixed = {
            "name": "temp_user",
            "age": 30,
            "active": True,
            "score": 95.5,
            "data": None
        }
        test_json_mixed = json.dumps(test_data_mixed)
        result = replace_values(test_json_mixed, r"^temp_.*$", "replacement")
        result_data = json.loads(result)

        # Only string values should be replaced
        self.assertEqual(result_data["name"], "replacement")
        self.assertEqual(result_data["age"], 30)
        self.assertEqual(result_data["active"], True)
        self.assertEqual(result_data["score"], 95.5)
        self.assertEqual(result_data["data"], None)

        # Pretty print option
        result_pretty = replace_values(
            test_json_mixed,
            r"^temp_.*$",
            "replacement",
            pretty_print=True
        )
        self.assertIn("\n", result_pretty)  # Should contain newlines for pretty printing

    def test_version(self):
        """Test that version is available."""
        self.assertIsNotNone(__version__)
        self.assertTrue(len(__version__) > 0)


if __name__ == "__main__":
    unittest.main()
