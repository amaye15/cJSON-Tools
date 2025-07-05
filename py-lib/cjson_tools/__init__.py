"""
cJSON-Tools - High-performance Python bindings for the cJSON-Tools C library.

This package provides Python bindings for the cJSON-Tools library,
which includes tools for flattening nested JSON structures, path type analysis,
generating JSON schemas, filtering JSON data, and advanced performance optimizations including:

- JSON flattening with array indexing support
- Path type analysis for schema discovery
- Remove keys with empty string values
- Remove keys with null values
- SIMD-optimized string operations
- Memory pool management
- Multi-threaded processing
- Pretty printing support
"""

from ._cjson_tools import (
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

__all__ = [
    "flatten_json",
    "flatten_json_batch",
    "generate_schema",
    "generate_schema_batch",
    "get_flattened_paths_with_types",
    "remove_empty_strings",
    "remove_nulls",
    "replace_keys",
    "replace_values",
    "__version__",
]
