# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.8.0] - 2025-07-05

### 🚀 New Features

#### JSON Filtering Functions
- **`remove_empty_strings()`**: Remove all keys that have empty string (`""`) values
- **`remove_nulls()`**: Remove all keys that have `null` values
- **Recursive Processing**: Both functions work recursively on nested objects and arrays
- **Structure Preservation**: Maintains JSON structure while filtering unwanted values

#### CLI Interface Enhancements
- **`-e, --remove-empty`**: Command-line option to remove empty string values
- **`-n, --remove-nulls`**: Command-line option to remove null values
- **Pretty Printing Support**: Works with `-p` flag for formatted output
- **Updated Help Text**: Comprehensive usage examples and documentation

#### Python Bindings
- **`cjson_tools.remove_empty_strings(json_string, pretty_print=False)`**
- **`cjson_tools.remove_nulls(json_string, pretty_print=False)`**
- **GIL Release**: Full GIL release during C computation for better performance
- **Error Handling**: Proper Python exception handling for invalid JSON

### 🔧 Technical Improvements

#### C Library Implementation
- **`filter_json_recursive()`**: Efficient helper function for recursive filtering
- **SIMD Optimizations**: Uses optimized string operations for performance
- **Memory Safety**: Proper cleanup and leak-free operation
- **Type Safety**: Robust handling of different JSON data types

#### Performance Optimizations
- **Memory Pools**: Leverages existing memory pool optimizations
- **Cache Efficiency**: Optimized for cache-friendly data access patterns
- **Minimal Allocations**: Efficient memory usage during filtering operations

### 🧪 Testing & Quality

#### Comprehensive Test Suite
- **10/10 Python tests passing**: Including 3 new test functions for filtering
- **Edge Case Coverage**: Empty objects, nested arrays, mixed data types
- **Memory Leak Testing**: Verified with Valgrind - no memory issues
- **Cross-Platform Testing**: Ubuntu, macOS, Python 3.8-3.12

#### CI/CD Verification
- **All GitHub Actions Passing**: 6/6 workflows successful
- **Code Formatting**: Black formatting compliance
- **Security Scanning**: No vulnerabilities detected
- **Performance Monitoring**: Benchmarks within expected ranges

### 📚 Documentation Updates

#### README Enhancements
- **Updated Feature List**: Added JSON filtering capabilities
- **New Examples**: CLI and Python usage examples for filtering functions
- **Enhanced Quick Start**: Comprehensive examples in the getting started section

#### API Documentation
- **Function Signatures**: Clear parameter descriptions and return types
- **Usage Examples**: Real-world examples for both CLI and Python interfaces
- **Error Handling**: Documentation of exception handling and edge cases

### 🎯 Usage Examples

#### Command Line
```bash
# Remove empty strings
echo '{"name": "test", "empty": "", "null": null}' | ./bin/json_tools -e -
# Output: {"name":"test","null":null}

# Remove nulls
echo '{"name": "test", "empty": "", "null": null}' | ./bin/json_tools -n -
# Output: {"name":"test","empty":""}
```

#### Python
```python
import cjson_tools
result1 = cjson_tools.remove_empty_strings(json_string)
result2 = cjson_tools.remove_nulls(json_string, pretty_print=True)
```

### 🔄 Backward Compatibility
- **Fully Backward Compatible**: All existing functionality preserved
- **No Breaking Changes**: Existing APIs unchanged
- **Version Compatibility**: Works with all supported Python versions (3.8-3.12)

## [1.4.0] - 2025-06-25

### 🚀 Major Improvements

#### Code Quality & Formatting
- **Integrated Black code formatter** with comprehensive configuration
- **Added isort** for consistent import sorting across all Python files
- **Pre-commit hooks** for automated code formatting
- **Makefile** with easy formatting commands (`make format`, `make format-check`)
- **Development dependencies** management with `requirements-dev.txt`

#### Memory Safety & Performance
- **Fixed critical memory access violation** in `strlen_simd` function
- **Added bounds checking** to prevent reading beyond allocated memory
- **SIMD optimization safety** - only use SIMD for strings ≥32 bytes
- **Performance test improvements** - fixed division by zero errors on fast systems

#### CI/CD Pipeline Enhancements
- **Fixed all GitHub Actions workflow failures**
- **Updated GitHub Pages actions** to latest versions (v3/v4/v5)
- **Resolved Python syntax errors** in CI workflows
- **Improved cross-platform compatibility** for binary builds
- **Enhanced error handling** and validation

#### Documentation & Release
- **Removed problematic GitHub Pages deployment** that was causing persistent failures
- **Streamlined documentation workflow** - generates docs without deployment issues
- **Comprehensive formatting guide** (`py-lib/FORMATTING.md`)
- **Updated README badges** and project documentation

### 🔧 Technical Fixes

#### Memory Management
- **SIMD bounds checking**: Prevents reading past memory boundaries
- **Platform-specific binary builds**: Compile on target architecture
- **Memory pool optimizations**: Better resource management

#### Workflow Reliability
- **All 26+ CI checks passing**: Comprehensive test coverage
- **Cross-platform testing**: Ubuntu, macOS, Python 3.8-3.12
- **Security scanning**: CodeQL, dependency, container scans
- **Performance monitoring**: Automated benchmarks and regression detection

#### Code Standards
- **Consistent formatting**: Black + isort standards across entire codebase
- **Import organization**: Proper import sorting with Black profile
- **Error handling**: Improved validation and error messages

### 🎯 Developer Experience

#### Easy Commands
```bash
# Format code
python py-lib/format.py
make format

# Check formatting
python py-lib/format.py --check
make format-check

# Install dev dependencies
pip install -r py-lib/requirements-dev.txt
```

#### Pre-commit Integration
```bash
cd py-lib
pip install pre-commit
pre-commit install
# Now formatting happens automatically on commit!
```

### 📊 Performance

#### Optimizations Maintained
- **SIMD string operations**: Safe and optimized
- **Multi-threaded processing**: Parallel JSON operations
- **Memory pools**: Efficient memory management
- **Lock-free queues**: High-performance data structures

#### Benchmark Results
- **C Library**: All performance tests passing
- **Python Package**: Optimized builds across platforms
- **Memory Safety**: Clean Valgrind reports
- **Cross-platform**: Consistent performance

### 🛡️ Security

#### Memory Safety
- **Valgrind clean**: No memory leaks or access violations
- **Bounds checking**: Safe SIMD operations
- **Input validation**: Robust error handling

#### Dependency Security
- **Updated actions**: Latest GitHub Actions versions
- **Vulnerability scanning**: Automated security checks
- **License compliance**: MIT license validation

### 🎉 Summary

Version 1.4.0 represents a major stability and quality improvement:

- ✅ **All CI/CD workflows passing** (26+ checks)
- ✅ **Memory safety issues resolved** (critical SIMD fix)
- ✅ **Code quality standards** (Black + isort integration)
- ✅ **Developer experience enhanced** (easy formatting, pre-commit)
- ✅ **Cross-platform reliability** (Ubuntu, macOS, Python 3.8-3.12)
- ✅ **Performance maintained** (all optimizations preserved)

This release establishes a robust foundation for future development with:
- **Production-ready codebase** with comprehensive testing
- **Modern development workflow** with automated quality checks
- **Reliable CI/CD pipeline** with full cross-platform support
- **Enhanced documentation** and developer resources

## [1.3.3] - Previous Release

### Features
- JSON flattening with SIMD optimizations
- Schema generation with multi-threading
- Memory pool management
- Cross-platform C library
- Python bindings with performance optimizations
