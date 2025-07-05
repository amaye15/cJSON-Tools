#!/usr/bin/env python3
"""
Performance Optimization Benchmark - Measuring C Code Improvements

This benchmark measures the performance improvements from the C code optimizations:
1. Pre-compiled regex patterns (eliminates regcomp() overhead)
2. Operation bitmask for fast checking (eliminates linear searches)
3. Optimized string operations
4. Early termination optimizations
"""

import json
import time
import statistics
from cjson_tools import JsonToolsBuilder

def generate_test_data(size="medium"):
    """Generate test data of different sizes."""
    if size == "small":
        data = {}
        for i in range(50):
            data[f"old_key_{i}"] = f"old_value_{i}" if i % 3 == 0 else f"value_{i}"
            data[f"key_{i}"] = "" if i % 5 == 0 else f"data_{i}"
            data[f"null_key_{i}"] = None if i % 4 == 0 else f"content_{i}"
        return data
    elif size == "medium":
        data = {}
        for i in range(200):
            data[f"old_key_{i}"] = f"old_value_{i}" if i % 3 == 0 else f"value_{i}"
            data[f"key_{i}"] = "" if i % 5 == 0 else f"data_{i}"
            data[f"null_key_{i}"] = None if i % 4 == 0 else f"content_{i}"
            data[f"session.tracking.page{i}.timeMs"] = 1000 + i
            data[f"analytics.page.view{i}.visits"] = 100 + i
            data[f"legacy.server.instance{i}"] = f"old_server_{i}"
            data[f"deprecated_field_{i}"] = f"deprecated_value_{i}"
        return data
    else:  # large
        data = {}
        for i in range(1000):
            data[f"old_key_{i}"] = f"old_value_{i}" if i % 3 == 0 else f"value_{i}"
            data[f"key_{i}"] = "" if i % 5 == 0 else f"data_{i}"
            data[f"null_key_{i}"] = None if i % 4 == 0 else f"content_{i}"
            data[f"session.tracking.page{i}.timeMs"] = 1000 + i
            data[f"analytics.page.view{i}.visits"] = 100 + i
            data[f"legacy.server.instance{i}"] = f"old_server_{i}"
            data[f"deprecated_field_{i}"] = f"deprecated_value_{i}"
            data[f"user.profile.{i}.name"] = f"user_{i}"
            data[f"config.setting.{i}.value"] = f"setting_{i}"
        return data

def benchmark_complex_operations(data, iterations=100):
    """Benchmark complex chained operations."""
    json_str = json.dumps(data)
    
    times = []
    for _ in range(iterations):
        start_time = time.perf_counter()
        
        result = (JsonToolsBuilder()
            .add_json(json_str)
            .remove_empty_strings()
            .remove_nulls()
            .replace_keys(r"^old_", "new_")
            .replace_keys(r"^session\.tracking\..*\.timeMs$", "session.pageTimesInMs.UnifiedPage")
            .replace_keys(r"^analytics\.page\..*\.visits$", "analytics.pageViews.TotalPage")
            .replace_keys(r"^legacy\.server\..*$", "modern.server.instance")
            .replace_keys(r"^deprecated", "updated")
            .replace_values(r"^old_.*$", "new_value")
            .replace_values(r"^legacy_.*$", "modern_value")
            .replace_values(r"^deprecated_.*$", "updated_value")
            .build())
        
        end_time = time.perf_counter()
        times.append(end_time - start_time)
    
    return times

def benchmark_regex_heavy_operations(data, iterations=50):
    """Benchmark regex-heavy operations to test pre-compiled regex benefits."""
    json_str = json.dumps(data)
    
    times = []
    for _ in range(iterations):
        start_time = time.perf_counter()
        
        result = (JsonToolsBuilder()
            .add_json(json_str)
            .replace_keys(r"^session\.tracking\.page\d+\.timeMs$", "session.unified.time")
            .replace_keys(r"^analytics\.page\.view\d+\.visits$", "analytics.unified.visits")
            .replace_keys(r"^legacy\.server\.instance\d+$", "modern.server.unified")
            .replace_keys(r"^user\.profile\.\d+\.name$", "user.unified.name")
            .replace_keys(r"^config\.setting\.\d+\.value$", "config.unified.value")
            .replace_values(r"^old_value_\d+$", "new_unified_value")
            .replace_values(r"^old_server_\d+$", "new_unified_server")
            .replace_values(r"^deprecated_value_\d+$", "updated_unified_value")
            .build())
        
        end_time = time.perf_counter()
        times.append(end_time - start_time)
    
    return times

def benchmark_operation_mask_benefits(data, iterations=100):
    """Benchmark operations that benefit from operation mask optimization."""
    json_str = json.dumps(data)
    
    # Test with only removal operations (should be very fast with bitmask)
    times_removal_only = []
    for _ in range(iterations):
        start_time = time.perf_counter()
        
        result = (JsonToolsBuilder()
            .add_json(json_str)
            .remove_empty_strings()
            .remove_nulls()
            .build())
        
        end_time = time.perf_counter()
        times_removal_only.append(end_time - start_time)
    
    # Test with mixed operations
    times_mixed = []
    for _ in range(iterations):
        start_time = time.perf_counter()
        
        result = (JsonToolsBuilder()
            .add_json(json_str)
            .remove_empty_strings()
            .remove_nulls()
            .replace_keys(r"^old_", "new_")
            .replace_values(r"^old_", "new_")
            .build())
        
        end_time = time.perf_counter()
        times_mixed.append(end_time - start_time)
    
    return times_removal_only, times_mixed

def print_benchmark_results(name, times):
    """Print formatted benchmark results."""
    mean_time = statistics.mean(times)
    median_time = statistics.median(times)
    min_time = min(times)
    max_time = max(times)
    std_dev = statistics.stdev(times) if len(times) > 1 else 0
    
    print(f"\n📊 {name}")
    print(f"   Mean:   {mean_time*1000:.2f} ms")
    print(f"   Median: {median_time*1000:.2f} ms")
    print(f"   Min:    {min_time*1000:.2f} ms")
    print(f"   Max:    {max_time*1000:.2f} ms")
    print(f"   StdDev: {std_dev*1000:.2f} ms")
    print(f"   Ops/sec: {1/mean_time:.0f}")
    
    return mean_time

def main():
    """Run comprehensive performance benchmarks."""
    print("🚀 C Code Performance Optimization Benchmark")
    print("=" * 60)
    print("\n🎯 Testing Optimizations:")
    print("   ✅ Pre-compiled regex patterns")
    print("   ✅ Operation bitmask for fast checking")
    print("   ✅ Optimized string operations")
    print("   ✅ Early termination optimizations")
    
    # Test different data sizes
    sizes = ["small", "medium", "large"]
    
    for size in sizes:
        print(f"\n🔍 Testing with {size.upper()} dataset...")
        data = generate_test_data(size)
        print(f"   Dataset size: {len(data)} keys")
        
        # Complex operations benchmark
        complex_times = benchmark_complex_operations(data, iterations=50 if size != "large" else 20)
        complex_mean = print_benchmark_results(f"Complex Operations ({size})", complex_times)
        
        # Regex-heavy operations benchmark
        regex_times = benchmark_regex_heavy_operations(data, iterations=30 if size != "large" else 10)
        regex_mean = print_benchmark_results(f"Regex-Heavy Operations ({size})", regex_times)
        
        # Operation mask benefits benchmark
        removal_times, mixed_times = benchmark_operation_mask_benefits(data, iterations=100 if size != "large" else 50)
        removal_mean = print_benchmark_results(f"Removal Only ({size})", removal_times)
        mixed_mean = print_benchmark_results(f"Mixed Operations ({size})", mixed_times)
        
        # Calculate performance insights
        print(f"\n💡 Performance Insights ({size}):")
        print(f"   Removal operations are {mixed_mean/removal_mean:.1f}x faster than mixed")
        print(f"   Complex operations: {1/complex_mean:.0f} ops/sec")
        print(f"   Regex operations: {1/regex_mean:.0f} ops/sec")
    
    print(f"\n🎉 Benchmark Complete!")
    print(f"\n🔧 Key Optimizations Implemented:")
    print(f"   1. ⚡ Pre-compiled regex patterns - Eliminates regcomp() overhead")
    print(f"   2. 🎯 Operation bitmask checking - O(1) instead of O(n) operation lookup")
    print(f"   3. 🚀 Fast string operations - Optimized empty string checking")
    print(f"   4. ⏭️  Early termination - Skip processing when no operations apply")
    print(f"   5. 🧠 Memory optimizations - Reduced allocation overhead")

if __name__ == "__main__":
    main()
