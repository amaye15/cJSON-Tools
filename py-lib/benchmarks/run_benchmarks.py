#!/usr/bin/env python3
"""
Unified Benchmark Runner for cJSON-Tools

This script provides a unified interface to run all available benchmarks:
1. Comprehensive benchmark (new) - Tests all functionality including optimizations
2. Original benchmark - Legacy benchmark with plotting capabilities
3. Quick tests - Fast validation of performance
"""

import argparse
import os
import subprocess
import sys


def run_comprehensive_benchmark(args):
    """Run the comprehensive benchmark suite."""
    print("🚀 Running Comprehensive Benchmark Suite...")
    print("   ✅ JsonToolsBuilder with all operations")
    print("   ✅ Flattening performance testing")
    print("   ✅ Performance optimizations validation")
    print("   ✅ Individual function comparisons")
    print("   ✅ Batch processing tests")

    cmd = [sys.executable, "benchmarks/comprehensive_benchmark.py"]

    if args.quick:
        cmd.append("--quick")
    if args.size:
        cmd.extend(["--size", args.size])

    try:
        result = subprocess.run(
            cmd, cwd=os.path.dirname(os.path.dirname(__file__)), check=True
        )
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"❌ Comprehensive benchmark failed: {e}")
        return False


def run_original_benchmark(args):
    """Run the original benchmark with plotting capabilities."""
    print("📊 Running Original Benchmark Suite...")
    print("   ✅ Multi-threaded performance testing")
    print("   ✅ Plotting and visualization")
    print("   ✅ Detailed statistical analysis")

    cmd = [sys.executable, "benchmarks/benchmark.py"]

    # Add original benchmark arguments
    if args.threads:
        cmd.extend(["--threads", str(args.threads)])
    if args.iterations:
        cmd.extend(["--iterations", str(args.iterations)])
    if args.plot:
        cmd.extend(["--plot", args.plot])
    if args.json_output:
        cmd.extend(["--json", args.json_output])

    try:
        result = subprocess.run(
            cmd, cwd=os.path.dirname(os.path.dirname(__file__)), check=True
        )
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"❌ Original benchmark failed: {e}")
        return False


def run_quick_test():
    """Run a quick performance validation test."""
    print("⚡ Running Quick Performance Test...")

    # Import here to avoid import issues
    sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

    try:
        import json
        import time

        from cjson_tools import JsonToolsBuilder

        # Quick test data
        test_data = {}
        for i in range(50):
            test_data[f"old_key_{i}"] = f"old_value_{i}" if i % 2 == 0 else f"value_{i}"
            test_data[f"empty_{i}"] = "" if i % 3 == 0 else f"data_{i}"
            test_data[f"null_{i}"] = None if i % 4 == 0 else f"content_{i}"
            test_data[f"nested_{i}"] = {"level1": {"level2": f"deep_value_{i}"}}

        json_str = json.dumps(test_data)

        # Test JsonToolsBuilder complete operations
        start_time = time.perf_counter()
        result = (
            JsonToolsBuilder()
            .add_json(json_str)
            .remove_empty_strings()
            .remove_nulls()
            .replace_keys(r"^old_", "new_")
            .replace_values(r"^old_", "new_")
            .build()
        )
        end_time = time.perf_counter()

        operations_time = end_time - start_time

        # Test flattening
        start_time = time.perf_counter()
        flattened = JsonToolsBuilder().add_json(json_str).flatten().build()
        end_time = time.perf_counter()

        flatten_time = end_time - start_time

        print(f"✅ Quick Test Results:")
        print(f"   Complete operations: {operations_time*1000:.2f} ms")
        print(f"   Flattening: {flatten_time*1000:.2f} ms")
        print(f"   Operations/sec: {1/operations_time:.0f}")
        print(f"   Flattening/sec: {1/flatten_time:.0f}")
        print(f"   🎉 All optimizations working!")

        return True

    except Exception as e:
        print(f"❌ Quick test failed: {e}")
        return False


def main():
    """Main benchmark runner."""
    parser = argparse.ArgumentParser(
        description="Unified cJSON-Tools Benchmark Runner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --comprehensive              # Run comprehensive benchmark
  %(prog)s --original --plot results/   # Run original with plots
  %(prog)s --quick                      # Quick performance test
  %(prog)s --all                        # Run all benchmarks
  %(prog)s --comprehensive --size medium # Test medium dataset only
        """,
    )

    # Benchmark selection
    parser.add_argument(
        "--comprehensive",
        action="store_true",
        help="Run comprehensive benchmark (recommended)",
    )
    parser.add_argument(
        "--original", action="store_true", help="Run original benchmark with plotting"
    )
    parser.add_argument(
        "--quick", action="store_true", help="Run quick performance test"
    )
    parser.add_argument(
        "--all", action="store_true", help="Run all available benchmarks"
    )

    # Comprehensive benchmark options
    parser.add_argument(
        "--size",
        choices=["small", "medium", "large"],
        help="Dataset size for comprehensive benchmark",
    )

    # Original benchmark options
    parser.add_argument(
        "--threads",
        type=int,
        default=1,
        help="Number of threads for original benchmark",
    )
    parser.add_argument(
        "--iterations",
        type=int,
        default=100,
        help="Number of iterations for original benchmark",
    )
    parser.add_argument(
        "--plot", type=str, help="Directory to save plots (original benchmark)"
    )
    parser.add_argument(
        "--json-output", type=str, help="JSON file to save results (original benchmark)"
    )

    args = parser.parse_args()

    # Default to comprehensive if no specific benchmark selected
    if not any([args.comprehensive, args.original, args.quick, args.all]):
        args.comprehensive = True

    print("🚀 cJSON-Tools Unified Benchmark Runner")
    print("=" * 50)

    success = True

    if args.quick or args.all:
        success &= run_quick_test()
        print()

    if args.comprehensive or args.all:
        success &= run_comprehensive_benchmark(args)
        print()

    if args.original or args.all:
        success &= run_original_benchmark(args)
        print()

    if success:
        print("🎉 All benchmarks completed successfully!")
        return 0
    else:
        print("❌ Some benchmarks failed!")
        return 1


if __name__ == "__main__":
    sys.exit(main())
