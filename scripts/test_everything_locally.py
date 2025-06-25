#!/usr/bin/env python3
"""
Comprehensive Local Testing Suite for cJSON-Tools
Tests all optimizations, GitHub Actions, Docker builds, and cross-platform compatibility
"""

import os
import subprocess
import sys
import time
from pathlib import Path


def run_command(cmd, cwd=None, env=None, timeout=600, capture_output=True):
    """Run a command and return the result"""
    try:
        print(f"🔧 Running: {cmd}")
        result = subprocess.run(
            cmd, 
            shell=True, 
            capture_output=capture_output, 
            text=True, 
            cwd=cwd, 
            env=env,
            timeout=timeout
        )
        return result
    except subprocess.TimeoutExpired:
        print(f"⏰ Command timed out: {cmd}")
        return subprocess.CompletedProcess(cmd, 1, "", "Command timed out")


def test_compilation():
    """Test C library compilation with optimizations"""
    print("\n🔧 Testing C Library Compilation")
    print("=" * 50)
    
    project_root = Path(__file__).parent.parent
    
    # Test basic compilation
    result = run_command("make clean && make", cwd=project_root)
    if result.returncode == 0:
        print("✅ Basic compilation successful")
    else:
        print("❌ Basic compilation failed")
        print(result.stderr)
        return False
    
    # Test debug build
    result = run_command("make debug", cwd=project_root)
    if result.returncode == 0:
        print("✅ Debug build successful")
    else:
        print("❌ Debug build failed")
        print(result.stderr)
        return False
    
    # Test with different optimization levels
    for opt_level in ["-O0", "-O1", "-O2", "-O3"]:
        result = run_command(f"make clean && CFLAGS_OPT='{opt_level} -DNDEBUG' make", cwd=project_root)
        if result.returncode == 0:
            print(f"✅ Compilation with {opt_level} successful")
        else:
            print(f"❌ Compilation with {opt_level} failed")
            return False
    
    return True


def test_python_extension():
    """Test Python extension building"""
    print("\n🐍 Testing Python Extension")
    print("=" * 50)
    
    project_root = Path(__file__).parent.parent
    py_lib = project_root / "py-lib"
    
    # Test build
    result = run_command("python3 setup.py build_ext --inplace", cwd=py_lib)
    if result.returncode == 0:
        print("✅ Python extension build successful")
    else:
        print("❌ Python extension build failed")
        print(result.stderr)
        return False

    # Test import
    result = run_command("python3 -c 'import cjson_tools; print(\"Import successful\")'", cwd=py_lib)
    if result.returncode == 0:
        print("✅ Python extension import successful")
    else:
        print("❌ Python extension import failed")
        print(result.stderr)
        return False
    
    return True


def test_formatting():
    """Test code formatting"""
    print("\n🎨 Testing Code Formatting")
    print("=" * 50)
    
    project_root = Path(__file__).parent.parent
    
    # Test Black formatting
    result = run_command("make format-check", cwd=project_root)
    if result.returncode == 0:
        print("✅ Black formatting check passed")
    else:
        print("❌ Black formatting check failed")
        print(result.stderr)
        return False
    
    # Test linting
    result = run_command("make lint", cwd=project_root)
    if result.returncode == 0:
        print("✅ Linting check passed")
    else:
        print("❌ Linting check failed")
        print(result.stderr)
        return False
    
    return True


def test_github_actions_with_act():
    """Test GitHub Actions workflows locally with act"""
    print("\n🎭 Testing GitHub Actions with Act")
    print("=" * 50)
    
    project_root = Path(__file__).parent.parent
    
    # List available workflows
    result = run_command("act --list", cwd=project_root)
    if result.returncode != 0:
        print("❌ Failed to list workflows")
        return False
    
    print("📋 Available workflows:")
    print(result.stdout)
    
    # Test the auto-format workflow
    print("\n🎨 Testing auto-format workflow...")
    result = run_command("act -W .github/workflows/auto-format.yml --dryrun", cwd=project_root, timeout=120)
    if result.returncode == 0:
        print("✅ Auto-format workflow dry-run successful")
    else:
        print("❌ Auto-format workflow dry-run failed")
        print(result.stderr)
        return False
    
    # Test the CI workflow if it exists
    ci_workflow = project_root / ".github/workflows/ci.yml"
    if ci_workflow.exists():
        print("\n🔧 Testing CI workflow...")
        result = run_command("act -W .github/workflows/ci.yml --dryrun", cwd=project_root, timeout=120)
        if result.returncode == 0:
            print("✅ CI workflow dry-run successful")
        else:
            print("❌ CI workflow dry-run failed")
            print(result.stderr)
            return False
    
    return True


def test_docker_builds():
    """Test Docker-based builds"""
    print("\n🐳 Testing Docker Builds")
    print("=" * 50)
    
    project_root = Path(__file__).parent.parent
    
    # Test Ubuntu build
    dockerfile_ubuntu = """
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \\
    gcc \\
    make \\
    python3 \\
    python3-pip \\
    python3-dev \\
    build-essential \\
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Test C compilation
RUN make clean && make

# Test Python extension
RUN cd py-lib && python3 setup.py build_ext --inplace

# Test import
RUN cd py-lib && python3 -c "import cjson_tools; print('Success!')"
"""
    
    with open(project_root / "Dockerfile.test-ubuntu", "w") as f:
        f.write(dockerfile_ubuntu)
    
    try:
        print("🔧 Building Ubuntu test container...")
        result = run_command(
            "docker build -f Dockerfile.test-ubuntu -t cjson-tools-test-ubuntu .",
            cwd=project_root,
            timeout=600
        )
        
        if result.returncode == 0:
            print("✅ Ubuntu Docker build successful")
        else:
            print("❌ Ubuntu Docker build failed")
            print(result.stderr)
            return False
            
    finally:
        # Cleanup
        (project_root / "Dockerfile.test-ubuntu").unlink(missing_ok=True)
        run_command("docker rmi cjson-tools-test-ubuntu", timeout=60)
    
    return True


def test_cross_platform_compatibility():
    """Test cross-platform compatibility"""
    print("\n🌍 Testing Cross-Platform Compatibility")
    print("=" * 50)
    
    project_root = Path(__file__).parent.parent
    
    # Test with different compiler flags
    test_configs = [
        ("Generic", "-O2 -std=c99"),
        ("No SIMD", "-O2 -std=c99 -DTHREADING_DISABLED"),
        ("Debug", "-O0 -g -std=c99 -DDEBUG"),
        ("Minimal", "-O1 -std=c99 -fno-builtin"),
    ]
    
    for config_name, flags in test_configs:
        print(f"\n🔧 Testing {config_name} configuration...")
        result = run_command(
            f"make clean && CFLAGS_OPT='{flags}' make",
            cwd=project_root
        )
        
        if result.returncode == 0:
            print(f"✅ {config_name} build successful")
        else:
            print(f"❌ {config_name} build failed")
            print(result.stderr)
            return False
    
    return True


def test_performance_optimizations():
    """Test that performance optimizations are working"""
    print("\n⚡ Testing Performance Optimizations")
    print("=" * 50)
    
    project_root = Path(__file__).parent.parent
    
    # Test SIMD detection
    test_simd = """
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "c-lib/include/json_parser_simd.h"

int main() {
    printf("Testing SIMD optimizations...\\n");

    const char* test_str = "Hello, World! This is a test string for SIMD optimization.";
    size_t len = strlen_simd(test_str);
    printf("String length: %zu\\n", len);

    if (len == 58) {
        printf("✅ SIMD strlen working correctly\\n");
        return 0;
    } else {
        printf("❌ SIMD strlen failed\\n");
        return 1;
    }
}
"""
    
    test_file = project_root / "test_simd.c"
    with open(test_file, "w") as f:
        f.write(test_simd)
    
    try:
        # Compile and run SIMD test
        result = run_command(
            f"gcc -I. -Ic-lib/include test_simd.c c-lib/src/json_parser_simd.c -o test_simd",
            cwd=project_root
        )
        
        if result.returncode == 0:
            result = run_command("./test_simd", cwd=project_root)
            if result.returncode == 0:
                print("✅ SIMD optimizations working")
            else:
                print("❌ SIMD optimizations failed")
                print(result.stdout)
                return False
        else:
            print("❌ SIMD test compilation failed")
            print(result.stderr)
            return False
            
    finally:
        test_file.unlink(missing_ok=True)
        (project_root / "test_simd").unlink(missing_ok=True)
    
    return True


def main():
    """Run all tests"""
    print("🧪 Comprehensive Local Testing Suite for cJSON-Tools")
    print("=" * 60)
    
    tests = [
        ("C Library Compilation", test_compilation),
        ("Python Extension", test_python_extension),
        ("Code Formatting", test_formatting),
        ("GitHub Actions (Act)", test_github_actions_with_act),
        ("Docker Builds", test_docker_builds),
        ("Cross-Platform Compatibility", test_cross_platform_compatibility),
        ("Performance Optimizations", test_performance_optimizations),
    ]
    
    results = {}
    
    for test_name, test_func in tests:
        print(f"\n{'='*60}")
        print(f"🔍 Running: {test_name}")
        print(f"{'='*60}")
        
        try:
            start_time = time.time()
            success = test_func()
            end_time = time.time()
            
            results[test_name] = {
                'success': success,
                'duration': end_time - start_time
            }
            
            if success:
                print(f"✅ {test_name} PASSED ({end_time - start_time:.1f}s)")
            else:
                print(f"❌ {test_name} FAILED ({end_time - start_time:.1f}s)")
                
        except Exception as e:
            print(f"💥 {test_name} CRASHED: {e}")
            results[test_name] = {'success': False, 'duration': 0}
    
    # Summary
    print(f"\n{'='*60}")
    print("📊 TEST SUMMARY")
    print(f"{'='*60}")
    
    passed = sum(1 for r in results.values() if r['success'])
    total = len(results)
    
    for test_name, result in results.items():
        status = "✅ PASS" if result['success'] else "❌ FAIL"
        duration = result['duration']
        print(f"{status} {test_name:<30} ({duration:.1f}s)")
    
    print(f"\n🎯 Overall: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All tests passed! Ready for release!")
        return True
    else:
        print("⚠️  Some tests failed. Please review and fix issues.")
        return False


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
