import os
import platform

from setuptools import Extension, find_packages, setup

# Get the directory containing this setup.py file
setup_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(setup_dir)  # Parent directory (cJSON-Tools root)

# Define paths to C library
c_lib_src = os.path.join(project_root, "c-lib", "src")
c_lib_include = os.path.join(project_root, "c-lib", "include")

# Check if C library files exist
if not os.path.exists(c_lib_src) or not os.path.exists(c_lib_include):
    # Fallback: try relative paths (for development builds)
    c_lib_src = "../c-lib/src"
    c_lib_include = "../c-lib/include"
    if not os.path.exists(c_lib_src) or not os.path.exists(c_lib_include):
        raise RuntimeError(
            "Cannot find C library source files. Please ensure you're building from the correct directory."
        )

# Platform-specific configurations
libraries = []
extra_compile_args = []
extra_link_args = []

# Check if we're on Windows
is_windows = platform.system() == "Windows"

# Check for cross-compilation to Windows with MinGW
is_mingw_cross_compile = (
    os.environ.get("CC", "").startswith("x86_64-w64-mingw32")
    or os.environ.get("CC", "").startswith("i686-w64-mingw32")
    or os.environ.get("CFLAGS", "").find("-D_WIN32") != -1
)

if is_windows:
    # Native Windows build with MSVC - Enhanced compatibility
    extra_compile_args = [
        "/std:c17",  # Use C17 instead of C11 for better MSVC atomic support
        "/O2",
        "/GL",
        "/DNDEBUG",
        "/DTHREADING_DISABLED",  # Disable threading on Windows due to atomic support issues
        "/DWIN32_LEAN_AND_MEAN",
        "/D_CRT_SECURE_NO_WARNINGS",
        "/wd4996",
        "/wd4005",
        "/wd4013",
        "/wd4505",
        "/wd4244",
        "/wd4267",
    ]
    extra_link_args = ["/LTCG"]

elif is_mingw_cross_compile:
    # Cross-compilation for Windows using MinGW/GCC
    libraries.append("pthread")
    extra_compile_args = [
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-O3",
        "-DNDEBUG",
        "-D_WIN32",
        "-DTHREADING_DISABLED",
    ]
    extra_link_args = ["-static-libgcc", "-static-libstdc++", "-lpthread"]
else:
    # Unix-like systems (Linux, macOS) with target-specific optimizations
    libraries.append("pthread")
    extra_compile_args = [
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-O3",
        "-flto",
        "-DNDEBUG",
        "-ffast-math",
        "-funroll-loops",
    ]

    # Target-specific optimizations
    import platform
    import sys

    if platform.machine() == "x86_64":
        extra_compile_args.extend(
            ["-march=native", "-mtune=native", "-msse4.2", "-mavx2"]
        )
    elif platform.machine() in ["aarch64", "arm64"]:
        # ARM64 optimizations (Linux and macOS)
        if sys.platform == "darwin":
            # macOS ARM64 - avoid -march=native which causes issues
            extra_compile_args.extend(["-mcpu=apple-a14"])
        else:
            # Linux ARM64
            extra_compile_args.extend(["-march=native", "-mcpu=native"])
    else:
        # Generic optimizations for other architectures
        extra_compile_args.extend(["-march=native", "-mtune=native"])

    extra_link_args = ["-flto"]

# Define the extension module
cjson_tools_module = Extension(
    "cjson_tools._cjson_tools",
    sources=[
        "cjson_tools/_cjson_tools.c",
        os.path.join(c_lib_src, "cJSON.c"),
        os.path.join(c_lib_src, "json_flattener.c"),
        os.path.join(c_lib_src, "json_schema_generator.c"),
        os.path.join(c_lib_src, "json_utils.c"),
        os.path.join(c_lib_src, "thread_pool.c"),
        os.path.join(c_lib_src, "memory_pool.c"),
        os.path.join(c_lib_src, "json_parser_simd.c"),
        os.path.join(c_lib_src, "lockfree_queue.c"),
    ],
    include_dirs=[
        c_lib_include,
    ],
    libraries=libraries,
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
    define_macros=(
        [
            ("_CRT_SECURE_NO_WARNINGS", None) if is_windows else ("_GNU_SOURCE", None),
        ]
        if is_windows
        else None
    ),
)

# Read the main README file
readme_path = os.path.join(project_root, "README.md")
try:
    with open(readme_path, "r", encoding="utf-8") as f:
        long_description = f.read()
except FileNotFoundError:
    # Fallback: try relative path
    try:
        with open("../README.md", "r", encoding="utf-8") as f:
            long_description = f.read()
    except FileNotFoundError:
        long_description = "Python bindings for the cJSON-Tools C library - A high-performance JSON processing toolkit"

setup(
    name="cjson-tools",
    version="1.9.0",
    description="High-performance Python bindings for JSON flattening, path type analysis, and schema generation",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="cJSON-Tools Team",
    author_email="openhands@all-hands.dev",
    url="https://github.com/amaye15/cJSON-Tools",
    packages=find_packages(),
    ext_modules=[cjson_tools_module],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: C",
        "Topic :: Software Development :: Libraries",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Operating System :: OS Independent",
    ],
    python_requires=">=3.8",
    zip_safe=False,  # Important for C extensions
)
