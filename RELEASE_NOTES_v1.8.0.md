# cJSON-Tools v1.8.0 Release Notes

🎉 **Major Feature Release: JSON Filtering Functions**

We're excited to announce cJSON-Tools v1.8.0, which introduces powerful new JSON filtering capabilities while maintaining our commitment to high performance and reliability.

## 🚀 What's New

### New JSON Filtering Functions

#### `remove_empty_strings()` 
Remove all keys that have empty string (`""`) values from JSON objects:

```python
import cjson_tools
data = '{"name": "John", "email": "", "phone": "123-456-7890", "bio": ""}'
result = cjson_tools.remove_empty_strings(data)
# Result: {"name": "John", "phone": "123-456-7890"}
```

#### `remove_nulls()`
Remove all keys that have `null` values from JSON objects:

```python
import cjson_tools
data = '{"name": "John", "email": null, "phone": "123-456-7890", "address": null}'
result = cjson_tools.remove_nulls(data)
# Result: {"name": "John", "phone": "123-456-7890"}
```

### Enhanced CLI Interface

New command-line options for filtering:

```bash
# Remove empty strings
echo '{"name": "test", "empty": "", "null": null}' | ./bin/json_tools -e -

# Remove nulls  
echo '{"name": "test", "empty": "", "null": null}' | ./bin/json_tools -n -

# With pretty printing
echo '{"data": {"empty": "", "valid": "test"}}' | ./bin/json_tools -e -p -
```

## 🔧 Technical Highlights

### Performance Optimized
- **SIMD Operations**: Leverages vectorized string operations for speed
- **Memory Pools**: Uses optimized memory allocation patterns
- **Recursive Efficiency**: Handles deeply nested structures efficiently
- **GIL Release**: Python bindings release GIL during C computation

### Memory Safe
- **Zero Memory Leaks**: Verified with Valgrind testing
- **Proper Cleanup**: Robust error handling and resource management
- **Type Safety**: Handles all JSON data types correctly

### Production Ready
- **All Tests Passing**: 10/10 Python tests + comprehensive C tests
- **Cross-Platform**: Ubuntu, macOS, Python 3.8-3.12
- **CI/CD Verified**: 6/6 GitHub Actions workflows passing
- **Security Scanned**: No vulnerabilities detected

## 📊 Quality Metrics

- ✅ **100% Test Coverage** for new functions
- ✅ **Zero Memory Leaks** detected by Valgrind
- ✅ **All GitHub Actions Passing** (6/6 workflows)
- ✅ **Cross-Platform Compatibility** verified
- ✅ **Performance Benchmarks** within expected ranges
- ✅ **Security Scans** clean (no vulnerabilities)

## 🎯 Use Cases

### Data Cleaning
Perfect for cleaning up JSON data from APIs or databases:
- Remove empty form fields before processing
- Filter out null values from database exports
- Clean up user-generated content

### API Response Processing
Streamline API responses by removing unwanted empty or null fields:
- Reduce payload size
- Simplify client-side processing
- Improve data quality

### Data Pipeline Integration
Integrate into data processing pipelines:
- ETL processes
- Data validation workflows
- JSON transformation pipelines

## 🔄 Migration Guide

### From v1.7.x to v1.8.0

**No breaking changes!** All existing functionality remains unchanged.

**New functionality:**
```python
# Add these new functions to your toolkit
import cjson_tools

# Existing functions (unchanged)
flattened = cjson_tools.flatten_json(json_string)
schema = cjson_tools.generate_schema(json_string)

# New filtering functions
cleaned = cjson_tools.remove_empty_strings(json_string)
filtered = cjson_tools.remove_nulls(json_string, pretty_print=True)
```

## 📦 Installation

### Python Package
```bash
pip install --upgrade cjson-tools
```

### From Source
```bash
git clone https://github.com/amaye15/cJSON-Tools.git
cd cJSON-Tools
git checkout v1.8.0
make
```

## 🙏 Acknowledgments

Thanks to all contributors and users who provided feedback and testing for this release. Your input helps make cJSON-Tools better for everyone!

## 🔗 Links

- **GitHub Repository**: https://github.com/amaye15/cJSON-Tools
- **PyPI Package**: https://pypi.org/project/cjson-tools/
- **Documentation**: https://amaye15.github.io/cJSON-Tools/
- **Issue Tracker**: https://github.com/amaye15/cJSON-Tools/issues

---

**Full Changelog**: https://github.com/amaye15/cJSON-Tools/blob/main/CHANGELOG.md#180---2025-07-05
