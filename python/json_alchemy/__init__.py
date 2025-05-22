"""
JSON Alchemy - Python bindings for the JSON Alchemy C library.

This package provides Python bindings for the JSON Alchemy library,
which includes tools for flattening nested JSON structures and
generating JSON schemas from JSON objects.
"""

from ._json_alchemy import (
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