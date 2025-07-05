import os
import platform
import sys

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

# ============================================================================
# Environment Detection and Build Configuration
# ============================================================================

def detect_build_environment():
    """Detect the build environment and return appropriate build tier."""
    # Check for CI/CD environments
    ci_indicators = [
        'CI', 'CONTINUOUS_INTEGRATION', 'GITHUB_ACTIONS', 'TRAVIS',
        'CIRCLECI', 'JENKINS_URL', 'BUILDKITE', 'GITLAB_CI'
    ]

    if any(os.environ.get(var) for var in ci_indicators):
        return 'ci'

    # Check for container environments
    container_indicators = [
        'CONTAINER', 'DOCKER_CONTAINER', 'KUBERNETES_SERVICE_HOST'
    ]

    if any(os.environ.get(var) for var in container_indicators):
        return 'container'

    # Check for act (local GitHub Actions runner)
    if os.environ.get('ACT'):
        return 'act'

    # Check if we're in a virtual environment or conda environment
    if hasattr(sys, 'real_prefix') or (hasattr(sys, 'base_prefix') and sys.base_prefix != sys.prefix):
        return 'venv'

    return 'native'

def get_build_tier():
    """Get the appropriate build tier based on environment."""
    # Allow explicit override
    tier = os.environ.get('BUILD_TIER')
    if tier:
        try:
            return int(tier)
        except ValueError:
            print(f"Warning: Invalid BUILD_TIER value '{tier}', using auto-detection")

    env = detect_build_environment()

    # Conservative tier for CI/containers
    if env in ['ci', 'container', 'act']:
        return 1

    # Moderate tier for virtual environments and native (safer default for Python builds)
    return 2

# Get build configuration
build_env = detect_build_environment()
build_tier = get_build_tier()

print(f"Python build environment: {build_env}")
print(f"Using build tier: {build_tier}")

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
    # Unix-like systems (Linux, macOS) with tier-based optimizations
    libraries.append("pthread")

    # Base flags for all tiers
    extra_compile_args = [
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-DNDEBUG",
    ]

    # Tier-specific optimizations
    if build_tier == 1:
        # Conservative: Maximum compatibility
        extra_compile_args.extend(["-O2"])
        extra_link_args = []

    elif build_tier == 2:
        # Moderate: Balanced performance
        extra_compile_args.extend([
            "-O3",
            "-funroll-loops",
            "-fomit-frame-pointer"
        ])

        # Safe architecture-specific optimizations
        if platform.machine() == "x86_64":
            extra_compile_args.extend(["-msse2", "-msse4.2"])
        elif platform.machine() in ["aarch64", "arm64"]:
            if sys.platform == "darwin":
                extra_compile_args.extend(["-mcpu=generic"])
            else:
                extra_compile_args.extend(["-mcpu=generic"])

        # Conservative LTO
        extra_compile_args.extend(["-flto"])
        extra_link_args = ["-flto"]

    elif build_tier == 3:
        # Aggressive: Native optimization (only for explicit native builds)
        extra_compile_args.extend([
            "-O3",
            "-funroll-loops",
            "-fomit-frame-pointer",
            "-finline-functions",
            "-ffast-math",
            "-ftree-vectorize"
        ])

        # Native architecture optimizations
        if platform.machine() == "x86_64":
            extra_compile_args.extend(["-march=native", "-mtune=native", "-msse4.2", "-mavx2"])
        elif platform.machine() in ["aarch64", "arm64"]:
            if sys.platform == "darwin":
                extra_compile_args.extend(["-mcpu=apple-a14"])
            else:
                extra_compile_args.extend(["-march=native", "-mcpu=native"])
        else:
            extra_compile_args.extend(["-march=native", "-mtune=native"])

        # Aggressive LTO
        extra_compile_args.extend(["-flto"])
        extra_link_args = ["-flto"]

    else:
        # Default to tier 2 for unknown tiers
        print(f"Warning: Unknown build tier {build_tier}, defaulting to tier 2")
        build_tier = 2
        extra_compile_args.extend(["-O3", "-funroll-loops"])
        extra_link_args = []

# Define the extension module
cjson_tools_module = Extension(
    "cjson_tools._cjson_tools",
    sources=[
        "cjson_tools/_cjson_tools.c",
        os.path.join(c_lib_src, "cJSON.c"),
        os.path.join(c_lib_src, "json_flattener.c"),
        os.path.join(c_lib_src, "json_schema_generator.c"),
        os.path.join(c_lib_src, "json_tools_builder.c"),
        os.path.join(c_lib_src, "json_utils.c"),
        os.path.join(c_lib_src, "thread_pool.c"),
        os.path.join(c_lib_src, "memory_pool.c"),
        os.path.join(c_lib_src, "json_parser_simd.c"),
        os.path.join(c_lib_src, "lockfree_queue.c"),
        os.path.join(c_lib_src, "portable_string.c"),
        os.path.join(c_lib_src, "common.c"),
        os.path.join(c_lib_src, "cpu_features.c"),
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
