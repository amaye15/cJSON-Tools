#!/usr/bin/env python3
"""
Comprehensive Test Runner for cJSON-Tools
Runs all tests including unit tests, integration tests, and performance tests
"""

import os
import sys
import subprocess
import argparse
import time
from pathlib import Path

def run_command(cmd, cwd=None, timeout=300):
    """Run a command and return the result"""
    print(f"🔧 Running: {cmd}")
    try:
        result = subprocess.run(
            cmd, shell=True, cwd=cwd, capture_output=True, 
            text=True, timeout=timeout
        )
        if result.returncode == 0:
            print("✅ Success")
            return True, result.stdout, result.stderr
        else:
            print(f"❌ Failed (exit code: {result.returncode})")
            print(f"Error: {result.stderr}")
            return False, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        print("❌ Command timed out")
        return False, "", "Command timed out"

def test_build_tiers():
    """Test all build tiers"""
    print("\n🏗️  Testing Build Tiers")
    print("=" * 50)
    
    tiers = [1, 2, 3, 4]
    results = {}
    
    for tier in tiers:
        print(f"\n🔨 Testing Build Tier {tier}")
        
        # Clean first
        success, _, _ = run_command("make clean")
        if not success:
            results[tier] = False
            continue
        
        # Build
        success, _, _ = run_command(f"make tier{tier}")
        if not success:
            results[tier] = False
            continue
        
        # Verify binary exists
        if not os.path.exists("bin/json_tools"):
            print("❌ Binary not found after build")
            results[tier] = False
            continue
        
        # Quick functionality test
        success, _, _ = run_command('echo \'{"test": "value"}\' | ./bin/json_tools -f')
        results[tier] = success
        
        if success:
            print(f"✅ Tier {tier} build and basic test successful")
        else:
            print(f"❌ Tier {tier} basic test failed")
    
    return results

def test_c_library():
    """Test C library functionality"""
    print("\n🧪 Testing C Library")
    print("=" * 50)
    
    # Build with tier2 for testing
    success, _, _ = run_command("make clean && make tier2")
    if not success:
        print("❌ Failed to build for C library testing")
        return False
    
    # Run dynamic tests
    success, _, _ = run_command("cd c-lib/tests && ./run_dynamic_tests.sh --quick", timeout=120)
    if not success:
        print("❌ C library dynamic tests failed")
        return False
    
    print("✅ C library tests passed")
    return True

def test_python_library():
    """Test Python library functionality"""
    print("\n🐍 Testing Python Library")
    print("=" * 50)
    
    # Build C library first
    success, _, _ = run_command("make clean && make tier2")
    if not success:
        print("❌ Failed to build C library for Python testing")
        return False
    
    # Install Python package in development mode
    success, _, _ = run_command("cd py-lib && pip install -e .", timeout=60)
    if not success:
        print("❌ Failed to install Python package")
        return False
    
    # Run Python tests
    success, _, _ = run_command("cd py-lib && python -m pytest tests/ -v", timeout=120)
    if not success:
        print("❌ Python library tests failed")
        return False
    
    print("✅ Python library tests passed")
    return True

def test_memory_safety():
    """Test memory safety with AddressSanitizer"""
    print("\n🛡️  Testing Memory Safety")
    print("=" * 50)
    
    # Check if we have gcc or clang with AddressSanitizer support
    has_asan = False
    for compiler in ['gcc', 'clang']:
        success, _, _ = run_command(f"{compiler} --help=target | grep -q sanitize")
        if success:
            has_asan = True
            break
    
    if not has_asan:
        print("⚠️  AddressSanitizer not available, skipping memory safety tests")
        return True
    
    # Build with AddressSanitizer
    success, _, _ = run_command(
        'make clean && CC=gcc make CFLAGS="-Wall -Wextra -std=c99 -I./c-lib/include -DNDEBUG -pthread -O1 -fsanitize=address -g -D_GNU_SOURCE" LIBS="-pthread -lm -fsanitize=address"'
    )
    if not success:
        print("❌ Failed to build with AddressSanitizer")
        return False
    
    # Run basic tests with ASAN
    os.environ['ASAN_OPTIONS'] = 'detect_leaks=1:abort_on_error=1'
    
    test_commands = [
        'echo \'{"test": {"nested": {"deep": "value"}}}\' | ./bin/json_tools -f',
        'echo \'{"id": 1, "name": "test"}\' | ./bin/json_tools -s'
    ]
    
    for cmd in test_commands:
        success, _, _ = run_command(cmd)
        if not success:
            print(f"❌ Memory safety test failed: {cmd}")
            return False
    
    print("✅ Memory safety tests passed")
    return True

def run_performance_tests(quick=False):
    """Run performance tests"""
    print("\n🚀 Running Performance Tests")
    print("=" * 50)
    
    if quick:
        sizes = "100,1000"
        iterations = 2
        tiers = "1,2"
    else:
        sizes = "100,1000,5000"
        iterations = 3
        tiers = "1,2,3"
    
    success, _, _ = run_command(
        f"python3 scripts/performance_test.py --tiers {tiers} --sizes {sizes} --iterations {iterations}",
        timeout=300
    )
    
    if success:
        print("✅ Performance tests completed")
    else:
        print("❌ Performance tests failed")
    
    return success

def main():
    parser = argparse.ArgumentParser(description="Comprehensive test runner for cJSON-Tools")
    parser.add_argument("--quick", action="store_true", help="Run quick tests only")
    parser.add_argument("--skip-build", action="store_true", help="Skip build tier tests")
    parser.add_argument("--skip-python", action="store_true", help="Skip Python tests")
    parser.add_argument("--skip-memory", action="store_true", help="Skip memory safety tests")
    parser.add_argument("--skip-performance", action="store_true", help="Skip performance tests")
    parser.add_argument("--c-only", action="store_true", help="Run C library tests only")
    
    args = parser.parse_args()
    
    print("🎯 cJSON-Tools Comprehensive Test Suite")
    print("=" * 50)
    
    # Change to project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    os.chdir(project_root)
    
    start_time = time.time()
    results = {}
    
    # Test build tiers
    if not args.skip_build and not args.c_only:
        results['build_tiers'] = test_build_tiers()
    
    # Test C library
    results['c_library'] = test_c_library()
    
    # Test Python library
    if not args.skip_python and not args.c_only:
        results['python_library'] = test_python_library()
    
    # Test memory safety
    if not args.skip_memory:
        results['memory_safety'] = test_memory_safety()
    
    # Run performance tests
    if not args.skip_performance:
        results['performance'] = run_performance_tests(args.quick)
    
    # Summary
    end_time = time.time()
    duration = end_time - start_time
    
    print("\n📊 Test Summary")
    print("=" * 50)
    print(f"Total duration: {duration:.2f} seconds")
    print()
    
    all_passed = True
    for test_name, result in results.items():
        if test_name == 'build_tiers':
            # Build tiers returns a dict
            tier_results = result
            for tier, passed in tier_results.items():
                status = "✅ PASS" if passed else "❌ FAIL"
                print(f"Build Tier {tier}: {status}")
                if not passed:
                    all_passed = False
        else:
            status = "✅ PASS" if result else "❌ FAIL"
            print(f"{test_name.replace('_', ' ').title()}: {status}")
            if not result:
                all_passed = False
    
    print()
    if all_passed:
        print("🎉 All tests passed!")
        sys.exit(0)
    else:
        print("💥 Some tests failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()
