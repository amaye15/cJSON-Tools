"""
cJSON-Tools - Python bindings for the cJSON-Tools C library.

This package provides Python bindings for the cJSON-Tools library,
which includes tools for flattening nested JSON structures and
generating JSON schemas from JSON objects.
"""

from ._cjson_tools import (
    flatten_json,
    flatten_json_batch,
    generate_schema,
    generate_schema_batch,
    __version__
)

__all__ = [
    'flatten_json',
    'flatten_json_batch',
    'generate_schema',
    'generate_schema_batch',
    '__version__'
]