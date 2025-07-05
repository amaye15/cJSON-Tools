"""
JsonToolsBuilder - A fluent interface for chaining JSON operations.

This module provides a builder pattern for efficiently applying multiple
JSON transformations in a single pass through the data.
"""

import json
from typing import Any, Dict, List, Optional, Union

from . import _cjson_tools


class JsonToolsBuilder:
    """
    A builder class for chaining JSON transformation operations.

    This class allows you to chain multiple operations that will be executed
    in a single pass through the JSON data for optimal performance.

    Example:
        >>> builder = JsonToolsBuilder()
        >>> result = (builder
        ...     .add_json('{"old_key": "old_value", "empty": "", "null_field": null}')
        ...     .remove_empty_strings()
        ...     .remove_nulls()
        ...     .replace_keys(r"^old_", "new_")
        ...     .replace_values(r"^old_", "new_")
        ...     .build())
    """

    def __init__(self):
        """Initialize a new JsonToolsBuilder instance."""
        self._json_data: Optional[str] = None
        self._operations: List[Dict[str, Any]] = []
        self._pretty_print: bool = False
        self._error: Optional[str] = None

    def add_json(self, json_data: Union[str, dict, list]) -> "JsonToolsBuilder":
        """
        Add JSON data to be processed.

        Args:
            json_data: JSON data as a string, dict, or list

        Returns:
            Self for method chaining

        Raises:
            ValueError: If json_data is not valid JSON
        """
        if isinstance(json_data, (dict, list)):
            self._json_data = json.dumps(json_data)
        elif isinstance(json_data, str):
            # Validate JSON
            try:
                json.loads(json_data)
                self._json_data = json_data
            except json.JSONDecodeError as e:
                raise ValueError(f"Invalid JSON: {e}")
        else:
            raise ValueError("json_data must be a string, dict, or list")

        return self

    def remove_empty_strings(self) -> "JsonToolsBuilder":
        """
        Queue operation to remove keys with empty string values.

        Returns:
            Self for method chaining
        """
        self._operations.append({"type": "remove_empty_strings"})
        return self

    def remove_nulls(self) -> "JsonToolsBuilder":
        """
        Queue operation to remove keys with null values.

        Returns:
            Self for method chaining
        """
        self._operations.append({"type": "remove_nulls"})
        return self

    def replace_keys(self, pattern: str, replacement: str) -> "JsonToolsBuilder":
        """
        Queue operation to replace keys matching a regex pattern.

        Args:
            pattern: Regular expression pattern to match keys
            replacement: Replacement string for matching keys

        Returns:
            Self for method chaining
        """
        self._operations.append(
            {"type": "replace_keys", "pattern": pattern, "replacement": replacement}
        )
        return self

    def replace_values(self, pattern: str, replacement: str) -> "JsonToolsBuilder":
        """
        Queue operation to replace string values matching a regex pattern.

        Args:
            pattern: Regular expression pattern to match values
            replacement: Replacement string for matching values

        Returns:
            Self for method chaining
        """
        self._operations.append(
            {"type": "replace_values", "pattern": pattern, "replacement": replacement}
        )
        return self

    def flatten(self) -> "JsonToolsBuilder":
        """
        Queue operation to flatten the JSON structure.

        Note: This operation is applied last, after all other transformations.

        Returns:
            Self for method chaining
        """
        self._operations.append({"type": "flatten"})
        return self

    def pretty_print(self, enable: bool = True) -> "JsonToolsBuilder":
        """
        Configure pretty printing for the output.

        Args:
            enable: Whether to enable pretty printing

        Returns:
            Self for method chaining
        """
        self._pretty_print = enable
        return self

    def build(self) -> str:
        """
        Execute all queued operations and return the result.

        Returns:
            The transformed JSON as a string

        Raises:
            ValueError: If no JSON data was provided or if operations fail
        """
        if self._json_data is None:
            raise ValueError("No JSON data provided. Call add_json() first.")

        if not self._operations:
            # No operations, just return the original data (possibly pretty printed)
            if self._pretty_print:
                data = json.loads(self._json_data)
                return json.dumps(data, indent=2)
            return self._json_data

        # Execute operations using the C implementation
        return self._execute_operations()

    def _execute_operations(self) -> str:
        """
        Execute the queued operations using C single-pass processing.

        Returns:
            The transformed JSON as a string
        """
        # Use the C builder for true single-pass processing
        try:
            result = _cjson_tools._builder_execute(
                self._json_data, self._operations, self._pretty_print
            )
            return result
        except Exception as e:
            # If C implementation fails, raise the error (no fallback)
            raise RuntimeError(f"C builder failed: {e}") from e

    def _execute_operations_fallback(self) -> str:
        """
        Fallback implementation using individual function calls (multi-pass).

        Returns:
            The transformed JSON as a string
        """
        result = self._json_data

        # Apply operations in order (multi-pass)
        for op in self._operations:
            if op["type"] == "remove_empty_strings":
                result = _cjson_tools.remove_empty_strings(result, self._pretty_print)
            elif op["type"] == "remove_nulls":
                result = _cjson_tools.remove_nulls(result, self._pretty_print)
            elif op["type"] == "replace_keys":
                result = _cjson_tools.replace_keys(
                    result, op["pattern"], op["replacement"], self._pretty_print
                )
            elif op["type"] == "replace_values":
                result = _cjson_tools.replace_values(
                    result, op["pattern"], op["replacement"], self._pretty_print
                )
            elif op["type"] == "flatten":
                result = _cjson_tools.flatten_json(result)
                if self._pretty_print:
                    data = json.loads(result)
                    result = json.dumps(data, indent=2)

        return result

    def reset(self) -> "JsonToolsBuilder":
        """
        Reset the builder to its initial state.

        Returns:
            Self for method chaining
        """
        self._json_data = None
        self._operations.clear()
        self._pretty_print = False
        self._error = None
        return self

    def get_operations(self) -> List[Dict[str, Any]]:
        """
        Get a copy of the currently queued operations.

        Returns:
            List of operation dictionaries
        """
        return self._operations.copy()

    def __repr__(self) -> str:
        """Return a string representation of the builder."""
        return f"JsonToolsBuilder(operations={len(self._operations)}, has_data={self._json_data is not None})"
