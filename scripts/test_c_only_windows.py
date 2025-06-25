#!/usr/bin/env python3
"""
Test just the C library compilation for Windows compatibility
This tests the C code without Python extension complications
"""

import os
import subprocess
import sys
import tempfile
from pathlib import Path


def run_command(cmd, cwd=None, env=None, timeout=300):
    """Run a command and return the result"""
    try:
        result = subprocess.run(
            cmd, 
            shell=True, 
            capture_output=True, 
            text=True, 
            cwd=cwd, 
            env=env,
            timeout=timeout
        )
        return result
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(cmd, 1, "", "Command timed out")


def test_c_compilation_only():
    """Test C library compilation without Python extension"""
    print("🔧 Testing C Library Compilation for Windows")
    print("=" * 50)
    
    project_root = Path(__file__).parent.parent
    c_lib_src = project_root / "c-lib" / "src"
    c_lib_include = project_root / "c-lib" / "include"
    
    # Test files to compile
    test_files = [
        "thread_pool.c",
        "memory_pool.c", 
        "json_utils.c",
        "json_flattener.c",
        "json_schema_generator.c",
        "json_parser_simd.c",
        "lockfree_queue.c",
        "cJSON.c"
    ]
    
    print("🔨 Testing MinGW cross-compilation (C only)...")
    
    # Create a simple test program that includes our headers
    test_program = """
#include <stdio.h>
#include <stdlib.h>

// Test our headers
#include "thread_pool.h"
#include "memory_pool.h"
#include "json_utils.h"
#include "json_flattener.h"
#include "json_schema_generator.h"
#include "json_parser_simd.h"
#include "lockfree_queue.h"
#include "cjson/cJSON.h"

int main() {
    printf("Windows C library test successful!\\n");
    
    // Test basic functionality
    cJSON* json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "test", "value");
        char* string = cJSON_Print(json);
        if (string) {
            printf("JSON test: %s\\n", string);
            free(string);
        }
        cJSON_Delete(json);
    }
    
    return 0;
}
"""
    
    # Write test program
    with tempfile.NamedTemporaryFile(mode='w', suffix='.c', delete=False) as f:
        f.write(test_program)
        test_file = f.name
    
    try:
        # Test with MinGW cross-compilation
        cmd = [
            "docker", "run", "--rm", "-v", f"{project_root}:/app", "-w", "/app",
            "ubuntu:20.04", "bash", "-c",
            f"""
            apt-get update && apt-get install -y mingw-w64 gcc
            
            # Copy test file
            cp {test_file} /app/test_windows.c
            
            # Compile with MinGW
            x86_64-w64-mingw32-gcc \\
                -std=c99 \\
                -Wall \\
                -Wextra \\
                -O3 \\
                -DNDEBUG \\
                -D_WIN32 \\
                -DTHREADING_DISABLED \\
                -fms-extensions \\
                -Wno-ignored-attributes \\
                -I/app/c-lib/include \\
                /app/test_windows.c \\
                /app/c-lib/src/*.c \\
                -o /app/test_windows.exe \\
                -static-libgcc \\
                -static-libstdc++ \\
                -lpthread
            
            echo "✅ MinGW compilation successful!"
            ls -la /app/test_windows.exe
            """
        ]
        
        result = run_command(" ".join(cmd), timeout=300)
        
        if result.returncode == 0:
            print("✅ MinGW C compilation successful!")
            print("📤 Output:")
            print(result.stdout)
            return True
        else:
            print("❌ MinGW C compilation failed")
            print("📤 Output:")
            print(result.stdout)
            print("📤 Errors:")
            print(result.stderr)
            return False
            
    finally:
        # Cleanup
        if os.path.exists(test_file):
            os.unlink(test_file)


def test_msvc_simulation():
    """Test MSVC-style compilation flags"""
    print("\n🔧 Testing MSVC-style Compilation")
    print("=" * 50)
    
    project_root = Path(__file__).parent.parent
    
    # Test with GCC using MSVC-style flags converted to GCC equivalents
    test_program = """
#define _WIN32
#define THREADING_DISABLED

#include <stdio.h>
#include <stdlib.h>

// Test our headers
#include "thread_pool.h"
#include "memory_pool.h"
#include "json_utils.h"
#include "cjson/cJSON.h"

int main() {
    printf("MSVC-style compilation test successful!\\n");
    return 0;
}
"""
    
    with tempfile.NamedTemporaryFile(mode='w', suffix='.c', delete=False) as f:
        f.write(test_program)
        test_file = f.name
    
    try:
        # Convert MSVC flags to GCC equivalents for testing
        cmd = [
            "gcc",
            "-std=c99",  # /std:c11 equivalent
            "-O3",       # /O2 equivalent  
            "-DNDEBUG",  # /DNDEBUG
            "-D_WIN32",
            "-DTHREADING_DISABLED",
            "-fms-extensions",  # Enable __declspec
            "-Wno-ignored-attributes",
            f"-I{project_root}/c-lib/include",
            test_file,
            f"{project_root}/c-lib/src/thread_pool.c",
            f"{project_root}/c-lib/src/memory_pool.c",
            f"{project_root}/c-lib/src/json_utils.c",
            f"{project_root}/c-lib/src/json_parser_simd.c",
            f"{project_root}/c-lib/src/lockfree_queue.c",
            f"{project_root}/c-lib/src/cJSON.c",
            "-o", "/tmp/test_msvc_style"
        ]
        
        result = run_command(" ".join(cmd), timeout=60)
        
        if result.returncode == 0:
            print("✅ MSVC-style compilation successful!")
            print("📤 Output:")
            print(result.stdout)
            return True
        else:
            print("❌ MSVC-style compilation failed")
            print("📤 Errors:")
            print(result.stderr)
            return False
            
    finally:
        if os.path.exists(test_file):
            os.unlink(test_file)


def main():
    """Main test function"""
    print("🧪 Windows C Library Compilation Testing")
    print("=" * 60)
    
    success1 = test_msvc_simulation()
    success2 = test_c_compilation_only()
    
    if success1 and success2:
        print("\n✅ All Windows C compilation tests passed!")
        return True
    else:
        print("\n❌ Some Windows C compilation tests failed!")
        return False


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
