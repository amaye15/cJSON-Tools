from setuptools import setup, Extension, find_packages
import os
import sys
import platform

# Platform-specific configurations
libraries = []
extra_compile_args = []
extra_link_args = []

# Check if we're on Windows
is_windows = platform.system() == 'Windows'

if not is_windows:
    # Unix-like systems (Linux, macOS) with advanced optimizations
    libraries.append('pthread')

    # Base optimization flags (removed -ffast-math due to NaN issues)
    extra_compile_args = [
        '-std=c99', '-Wall', '-Wextra',
        '-O3', '-flto', '-funroll-loops',
        '-fomit-frame-pointer',
        '-finline-functions',
        '-fno-stack-protector',
        '-DNDEBUG'
    ]

    # Platform-specific optimizations
    if platform.system() == 'Darwin':
        # macOS - avoid march=native for universal builds
        extra_link_args = ['-flto', '-Wl,-dead_strip']
    else:
        # Linux - use march=native for better performance
        extra_compile_args.extend(['-march=native', '-mtune=native'])
        extra_link_args = ['-flto', '-Wl,--gc-sections']
else:
    # Windows-specific configuration with optimizations
    extra_compile_args = ['/std:c11', '/O2', '/GL', '/DNDEBUG']  # MSVC optimizations
    extra_link_args = ['/LTCG']  # Link-time code generation

# Define the extension module
cjson_tools_module = Extension(
    'cjson_tools._cjson_tools',
    sources=[
        'cjson_tools/_cjson_tools.c',
        '../c-lib/src/json_flattener.c',
        '../c-lib/src/json_schema_generator.c',
        '../c-lib/src/json_utils.c',
        '../c-lib/src/thread_pool.c',
        '../c-lib/src/memory_pool.c',
        '../c-lib/src/json_parser_simd.c',
        '../c-lib/src/lockfree_queue.c',
        '../c-lib/src/cJSON.c',  # Include cJSON directly
    ],
    include_dirs=[
        '../c-lib/include',
    ],
    libraries=libraries,
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
)

# Read the main README file
try:
    with open('../README.md', 'r', encoding='utf-8') as f:
        long_description = f.read()
except FileNotFoundError:
    long_description = 'Python bindings for the cJSON-Tools C library - A high-performance JSON processing toolkit'

setup(
    name='cjson-tools',
    version='1.3.3',
    description='Python bindings for the cJSON-Tools C library',
    long_description=long_description,
    long_description_content_type='text/markdown',
    author='cJSON-Tools Team',
    author_email='openhands@all-hands.dev',
    url='https://github.com/amaye15/cJSON-Tools',
    packages=find_packages(),
    ext_modules=[cjson_tools_module],
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
        'Programming Language :: C',
        'Topic :: Software Development :: Libraries',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    python_requires='>=3.8',
)