# cJSON-Tools Python Bindings

This directory contains the Python bindings for cJSON-Tools.

## Quick Start

```bash
pip install cjson-tools
```

```python
import cjson_tools
import json

# Flatten nested JSON
data = {"user": {"name": "John", "address": {"city": "NYC"}}}
flattened = cjson_tools.flatten_json(json.dumps(data))
print(flattened)
```

## Full Documentation

For complete documentation, installation instructions, examples, and publishing guides, see the main project README:

**[📖 Main Documentation](../README.md)**

The main README includes:
- 🚀 Quick start guide
- 📦 Installation instructions for all platforms
- 🧪 Testing procedures
- 📦 Publishing to PyPI
- 🔧 Development setup
- 📋 API reference
- ⚡ Performance benchmarks

## Directory Structure

- `cjson_tools/` - Python package source code
- `tests/` - Unit tests
- `examples/` - Usage examples
- `benchmarks/` - Performance testing scripts
- `setup.py` - Package configuration
