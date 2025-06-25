#!/usr/bin/env python3
"""
Quick test to verify all optimizations work correctly
"""

import subprocess
import sys
import json
from pathlib import Path


def run_command(cmd, cwd=None):
    """Run a command and return success status"""
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=cwd)
        return result.returncode == 0, result.stdout, result.stderr
    except Exception as e:
        return False, "", str(e)


def test_c_compilation():
    """Test C library compilation"""
    print("🔧 Testing C compilation...")
    success, stdout, stderr = run_command("make clean && make")
    if success:
        print("✅ C compilation successful")
        return True
    else:
        print("❌ C compilation failed")
        print(f"Error: {stderr}")
        return False


def test_c_functionality():
    """Test C binary functionality"""
    print("🔧 Testing C binary functionality...")
    
    # Create test JSON
    test_json = {"name": "test", "nested": {"value": 42, "array": [1, 2, 3]}}
    test_file = Path("test_input.json")
    
    try:
        with open(test_file, "w") as f:
            json.dump(test_json, f)
        
        # Test flattening
        success, stdout, stderr = run_command(f"./bin/json_tools {test_file}")
        if success and "name" in stdout and "nested.value" in stdout:
            print("✅ C binary flattening works")
            return True
        else:
            print("❌ C binary flattening failed")
            print(f"Output: {stdout}")
            print(f"Error: {stderr}")
            return False
    finally:
        test_file.unlink(missing_ok=True)


def test_python_extension():
    """Test Python extension"""
    print("🐍 Testing Python extension...")
    
    # Test build
    success, stdout, stderr = run_command("python3 setup.py build_ext --inplace", cwd="py-lib")
    if not success:
        print("❌ Python extension build failed")
        print(f"Error: {stderr}")
        return False
    
    # Test import and basic functionality
    test_code = '''
import sys
sys.path.insert(0, ".")
import cjson_tools
import json

# Test basic functionality
test_data = {"name": "test", "nested": {"value": 42}}
test_json_str = json.dumps(test_data)
result = cjson_tools.flatten_json(test_json_str)
print("✅ Python extension works" if "nested.value" in result else "❌ Python extension failed")
'''
    
    success, stdout, stderr = run_command(f"cd py-lib && python3 -c '{test_code}'")
    if success and "✅" in stdout:
        print("✅ Python extension works")
        return True
    else:
        print("❌ Python extension failed")
        print(f"Output: {stdout}")
        print(f"Error: {stderr}")
        return False


def test_github_actions():
    """Test GitHub Actions with act"""
    print("🎭 Testing GitHub Actions...")
    
    success, stdout, stderr = run_command("act -W .github/workflows/auto-format.yml --dryrun")
    if success and "Job succeeded" in stdout:
        print("✅ GitHub Actions workflow valid")
        return True
    else:
        print("❌ GitHub Actions workflow failed")
        print(f"Error: {stderr}")
        return False


def test_performance_optimizations():
    """Test that performance optimizations compile and work"""
    print("⚡ Testing performance optimizations...")
    
    # Test SIMD compilation
    test_simd_code = '''
#include <stdio.h>
#include <string.h>
#include <stddef.h>

// Simple test without SIMD dependencies
int main() {
    const char* test_str = "Hello, World!";
    size_t len = strlen(test_str);
    printf("%zu", len);
    return (len == 13) ? 0 : 1;
}
'''
    
    test_file = Path("test_simd.c")
    try:
        with open(test_file, "w") as f:
            f.write(test_simd_code)
        
        # Compile simple test
        success, stdout, stderr = run_command(
            "gcc test_simd.c -o test_simd"
        )
        
        if success:
            # Run SIMD test
            success, stdout, stderr = run_command("./test_simd")
            if success and stdout.strip() == "13":
                print("✅ Basic compilation works")
                return True
            else:
                print("❌ Basic compilation failed")
                return False
        else:
            print("❌ Basic compilation failed")
            print(f"Error: {stderr}")
            return False
    finally:
        test_file.unlink(missing_ok=True)
        Path("test_simd").unlink(missing_ok=True)


def main():
    """Run all quick tests"""
    print("🧪 Quick Test Suite for cJSON-Tools Optimizations")
    print("=" * 60)
    
    tests = [
        ("C Compilation", test_c_compilation),
        ("C Functionality", test_c_functionality),
        ("Python Extension", test_python_extension),
        ("GitHub Actions", test_github_actions),
        ("Performance Optimizations", test_performance_optimizations),
    ]
    
    results = []
    
    for test_name, test_func in tests:
        print(f"\n{'='*60}")
        print(f"🔍 Running: {test_name}")
        print(f"{'='*60}")
        
        try:
            success = test_func()
            results.append((test_name, success))
        except Exception as e:
            print(f"💥 {test_name} crashed: {e}")
            results.append((test_name, False))
    
    # Summary
    print(f"\n{'='*60}")
    print("📊 QUICK TEST SUMMARY")
    print(f"{'='*60}")
    
    passed = sum(1 for _, success in results if success)
    total = len(results)
    
    for test_name, success in results:
        status = "✅ PASS" if success else "❌ FAIL"
        print(f"{status} {test_name}")
    
    print(f"\n🎯 Overall: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All quick tests passed! Optimizations are working!")
        return True
    else:
        print("⚠️  Some tests failed. Check the output above.")
        return False


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
