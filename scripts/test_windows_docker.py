#!/usr/bin/env python3
"""
Windows Build Testing with Docker
Simulates the actual Windows build environment using Docker containers
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


def test_windows_build_docker():
    """Test Windows build using Docker container"""
    print("🐳 Windows Build Testing with Docker")
    print("=" * 50)
    
    # Check if Docker is available
    result = run_command("docker --version")
    if result.returncode != 0:
        print("❌ Docker not available")
        return False
    
    print(f"✅ Docker available: {result.stdout.strip()}")
    
    # Get project root
    project_root = Path(__file__).parent.parent
    
    # Create Dockerfile for Windows-like environment
    dockerfile_content = """
FROM python:3.8-windowsservercore-1809

# Install Visual Studio Build Tools
RUN powershell -Command "Invoke-WebRequest -Uri 'https://aka.ms/vs/16/release/vs_buildtools.exe' -OutFile 'vs_buildtools.exe'"
RUN vs_buildtools.exe --quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows10SDK.19041

# Set up environment
ENV PYTHONUNBUFFERED=1
ENV DISTUTILS_USE_SDK=1
ENV MSSdk=1

# Copy project
COPY . /app
WORKDIR /app/py-lib

# Install dependencies
RUN python -m pip install --upgrade pip setuptools wheel

# Test build
CMD ["python", "setup.py", "build_ext", "--inplace"]
"""
    
    # Write Dockerfile
    dockerfile_path = project_root / "Dockerfile.windows-test"
    with open(dockerfile_path, "w") as f:
        f.write(dockerfile_content)
    
    try:
        print("🔨 Building Windows test container...")
        build_result = run_command(
            f"docker build -f {dockerfile_path} -t cjson-tools-windows-test .",
            cwd=project_root,
            timeout=1800  # 30 minutes for VS Build Tools
        )
        
        if build_result.returncode != 0:
            print("❌ Docker build failed")
            print("📤 Build output:")
            print(build_result.stdout)
            print("📤 Build errors:")
            print(build_result.stderr)
            return False
        
        print("✅ Docker container built successfully")
        
        print("🔨 Running Windows build test...")
        run_result = run_command(
            "docker run --rm cjson-tools-windows-test",
            timeout=600  # 10 minutes for build
        )
        
        if run_result.returncode == 0:
            print("✅ Windows build successful!")
            print("📤 Build output:")
            print(run_result.stdout)
            return True
        else:
            print("❌ Windows build failed")
            print("📤 Build output:")
            print(run_result.stdout)
            print("📤 Build errors:")
            print(run_result.stderr)
            return False
            
    finally:
        # Cleanup
        if dockerfile_path.exists():
            dockerfile_path.unlink()
        
        # Remove test container
        run_command("docker rmi cjson-tools-windows-test", timeout=60)


def test_windows_build_wine():
    """Alternative: Test Windows build using Wine (faster than full Windows container)"""
    print("🍷 Windows Build Testing with Wine")
    print("=" * 50)
    
    # Check if Wine is available
    result = run_command("wine --version")
    if result.returncode != 0:
        print("❌ Wine not available, trying Docker approach...")
        return test_windows_build_docker()
    
    print(f"✅ Wine available: {result.stdout.strip()}")
    
    # Create Wine-based test
    dockerfile_content = """
FROM ubuntu:20.04

# Install Wine and dependencies
RUN apt-get update && apt-get install -y \\
    wine \\
    python3 \\
    python3-pip \\
    python3-dev \\
    build-essential \\
    mingw-w64 \\
    && rm -rf /var/lib/apt/lists/*

# Set up Wine
ENV WINEARCH=win64
ENV WINEPREFIX=/root/.wine
RUN winecfg

# Install Python for Windows via Wine
RUN wget https://www.python.org/ftp/python/3.8.10/python-3.8.10-amd64.exe
RUN wine python-3.8.10-amd64.exe /quiet InstallAllUsers=1 PrependPath=1

# Copy project
COPY . /app
WORKDIR /app/py-lib

# Test build with Windows Python
CMD ["wine", "python", "setup.py", "build_ext", "--inplace"]
"""
    
    project_root = Path(__file__).parent.parent
    dockerfile_path = project_root / "Dockerfile.wine-test"
    
    with open(dockerfile_path, "w") as f:
        f.write(dockerfile_content)
    
    try:
        print("🔨 Building Wine test container...")
        build_result = run_command(
            f"docker build -f {dockerfile_path} -t cjson-tools-wine-test .",
            cwd=project_root,
            timeout=1200  # 20 minutes
        )
        
        if build_result.returncode != 0:
            print("❌ Wine container build failed")
            print("📤 Build errors:")
            print(build_result.stderr)
            return False
        
        print("✅ Wine container built successfully")
        
        print("🔨 Running Wine build test...")
        run_result = run_command(
            "docker run --rm cjson-tools-wine-test",
            timeout=300  # 5 minutes
        )
        
        if run_result.returncode == 0:
            print("✅ Wine build successful!")
            return True
        else:
            print("❌ Wine build failed")
            print("📤 Build errors:")
            print(run_result.stderr)
            return False
            
    finally:
        # Cleanup
        if dockerfile_path.exists():
            dockerfile_path.unlink()
        run_command("docker rmi cjson-tools-wine-test", timeout=60)


def test_mingw_cross_compile():
    """Test cross-compilation for Windows using MinGW"""
    print("🔧 Windows Cross-Compilation Testing with MinGW")
    print("=" * 50)
    
    dockerfile_content = """
FROM ubuntu:20.04

# Install MinGW cross-compiler and Python
RUN apt-get update && apt-get install -y \\
    mingw-w64 \\
    python3 \\
    python3-pip \\
    python3-dev \\
    build-essential \\
    && rm -rf /var/lib/apt/lists/*

# Install Python packages
RUN python3 -m pip install setuptools wheel

# Copy project
COPY . /app
WORKDIR /app/py-lib

# Set up cross-compilation environment
ENV CC=x86_64-w64-mingw32-gcc
ENV CXX=x86_64-w64-mingw32-g++
ENV AR=x86_64-w64-mingw32-ar
ENV RANLIB=x86_64-w64-mingw32-ranlib
ENV LINK=x86_64-w64-mingw32-ld
ENV CFLAGS="-D_WIN32 -DTHREADING_DISABLED -fms-extensions"

# Test cross-compilation
CMD ["python3", "setup.py", "build_ext", "--inplace"]
"""
    
    project_root = Path(__file__).parent.parent
    dockerfile_path = project_root / "Dockerfile.mingw-test"
    
    with open(dockerfile_path, "w") as f:
        f.write(dockerfile_content)
    
    try:
        print("🔨 Building MinGW test container...")
        build_result = run_command(
            f"docker build -f {dockerfile_path} -t cjson-tools-mingw-test .",
            cwd=project_root,
            timeout=600  # 10 minutes
        )
        
        if build_result.returncode != 0:
            print("❌ MinGW container build failed")
            print("📤 Build errors:")
            print(build_result.stderr)
            return False
        
        print("✅ MinGW container built successfully")
        
        print("🔨 Running MinGW cross-compilation test...")
        run_result = run_command(
            "docker run --rm cjson-tools-mingw-test",
            timeout=300  # 5 minutes
        )
        
        if run_result.returncode == 0:
            print("✅ MinGW cross-compilation successful!")
            print("📤 Build output:")
            print(run_result.stdout)
            return True
        else:
            print("❌ MinGW cross-compilation failed")
            print("📤 Build output:")
            print(run_result.stdout)
            print("📤 Build errors:")
            print(run_result.stderr)
            return False
            
    finally:
        # Cleanup
        if dockerfile_path.exists():
            dockerfile_path.unlink()
        run_command("docker rmi cjson-tools-mingw-test", timeout=60)


def main():
    """Main test function"""
    print("🧪 Comprehensive Windows Build Testing")
    print("=" * 60)
    
    # Try different approaches in order of speed/reliability
    approaches = [
        ("MinGW Cross-Compilation", test_mingw_cross_compile),
        ("Wine Simulation", test_windows_build_wine),
        ("Full Windows Container", test_windows_build_docker),
    ]
    
    for name, test_func in approaches:
        print(f"\n🔍 Testing: {name}")
        print("-" * 40)
        
        try:
            if test_func():
                print(f"✅ {name} successful!")
                return True
        except Exception as e:
            print(f"❌ {name} failed with exception: {e}")
            continue
    
    print("\n❌ All Windows build tests failed!")
    return False


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
