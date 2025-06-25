#!/usr/bin/env python3
"""
Test Windows build compatibility locally
"""

import os
import subprocess
import sys
import tempfile
from pathlib import Path


def test_c_compilation():
    """Test C file compilation with Windows-like flags."""
    print("🔧 Testing C compilation with Windows-like settings...")
    
    # Get project paths
    project_root = Path(__file__).parent.parent
    c_lib_src = project_root / "c-lib" / "src"
    c_lib_include = project_root / "c-lib" / "include"
    
    # Test files to compile
    test_files = [
        "thread_pool.c",
        "json_flattener.c", 
        "json_schema_generator.c",
        "json_utils.c",
        "memory_pool.c",
        "json_parser_simd.c",
        "lockfree_queue.c",
        "cJSON.c"
    ]
    
    print(f"📁 C library source: {c_lib_src}")
    print(f"📁 C library include: {c_lib_include}")
    
    # Test each file individually
    for test_file in test_files:
        file_path = c_lib_src / test_file
        if not file_path.exists():
            print(f"⚠️  File not found: {file_path}")
            continue
            
        print(f"\n🔨 Testing compilation of {test_file}...")
        
        # Simulate Windows compilation flags
        cmd = [
            "gcc",
            "-c",  # Compile only, don't link
            "-std=c99",
            "-Wall",
            "-Wextra",
            "-O2",
            "-DNDEBUG",
            "-D_WIN32",  # Simulate Windows
            "-DTHREADING_DISABLED",  # Our Windows flag
            "-fms-extensions",  # Enable Microsoft extensions for __declspec
            f"-I{c_lib_include}",
            str(file_path),
            "-o", f"/tmp/{test_file}.o"
        ]
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            if result.returncode == 0:
                print(f"✅ {test_file}: Compilation successful")
            else:
                print(f"❌ {test_file}: Compilation failed")
                print(f"   Error: {result.stderr}")
                print(f"   Command: {' '.join(cmd)}")
                
        except subprocess.TimeoutExpired:
            print(f"⏰ {test_file}: Compilation timed out")
        except Exception as e:
            print(f"💥 {test_file}: Exception during compilation: {e}")


def test_python_extension():
    """Test Python extension building."""
    print("\n🐍 Testing Python extension building...")
    
    project_root = Path(__file__).parent.parent
    py_lib_dir = project_root / "py-lib"
    
    if not py_lib_dir.exists():
        print(f"❌ Python library directory not found: {py_lib_dir}")
        return
        
    # Change to py-lib directory
    original_cwd = os.getcwd()
    os.chdir(py_lib_dir)
    
    try:
        # Test building the extension
        print("🔨 Building Python extension...")
        
        # Set environment variables to simulate Windows
        env = os.environ.copy()
        env['CFLAGS'] = '-D_WIN32 -DTHREADING_DISABLED -fms-extensions -Wno-ignored-attributes'
        
        cmd = [sys.executable, "setup.py", "build_ext", "--inplace"]
        
        result = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=120)
        
        if result.returncode == 0:
            print("✅ Python extension build successful")
            print("📤 Output:", result.stdout[-500:] if len(result.stdout) > 500 else result.stdout)
        else:
            print("❌ Python extension build failed")
            print("📤 Error output:")
            print(result.stderr)
            print("📤 Standard output:")
            print(result.stdout)
            
    except subprocess.TimeoutExpired:
        print("⏰ Python extension build timed out")
    except Exception as e:
        print(f"💥 Exception during Python extension build: {e}")
    finally:
        os.chdir(original_cwd)


def check_includes():
    """Check for problematic includes."""
    print("\n🔍 Checking for problematic includes...")
    
    project_root = Path(__file__).parent.parent
    c_lib_dir = project_root / "c-lib"
    
    # Search for pthread includes
    for file_path in c_lib_dir.rglob("*.c"):
        with open(file_path, 'r') as f:
            content = f.read()
            if 'pthread.h' in content:
                print(f"⚠️  Found pthread.h include in: {file_path}")
                
    for file_path in c_lib_dir.rglob("*.h"):
        with open(file_path, 'r') as f:
            content = f.read()
            if 'pthread.h' in content:
                print(f"⚠️  Found pthread.h include in: {file_path}")
                
    # Check for Windows-specific issues
    for file_path in c_lib_dir.rglob("*.c"):
        with open(file_path, 'r') as f:
            lines = f.readlines()
            for i, line in enumerate(lines, 1):
                if 'CreateThread' in line or 'WaitForSingleObject' in line:
                    print(f"🪟 Found Windows API call in {file_path}:{i}: {line.strip()}")


def main():
    """Main function."""
    print("🧪 Windows Build Compatibility Test")
    print("=" * 50)
    
    # Check if we have gcc
    try:
        result = subprocess.run(["gcc", "--version"], capture_output=True, text=True)
        if result.returncode == 0:
            print(f"✅ GCC available: {result.stdout.split()[2]}")
        else:
            print("❌ GCC not available")
            return 1
    except FileNotFoundError:
        print("❌ GCC not found - please install build tools")
        return 1
    
    check_includes()
    test_c_compilation()
    test_python_extension()
    
    print("\n" + "=" * 50)
    print("🏁 Windows build compatibility test complete")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
