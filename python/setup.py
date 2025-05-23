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
    # Unix-like systems (Linux, macOS)
    libraries.append('pthread')
    extra_compile_args = ['-std=c99', '-Wall', '-Wextra']
else:
    # Windows-specific configuration
    extra_compile_args = ['/std:c11']  # MSVC equivalent of C99

# Define the extension module
json_alchemy_module = Extension(
    'json_alchemy._json_alchemy',
    sources=[
        'json_alchemy/_json_alchemy.c',
        'json_alchemy/src/json_flattener.c',
        'json_alchemy/src/json_schema_generator.c',
        'json_alchemy/src/json_utils.c',
        'json_alchemy/src/thread_pool.c',
        'json_alchemy/src/cJSON.c',  # Include cJSON directly
    ],
    include_dirs=[
        'json_alchemy/include',
    ],
    libraries=libraries,
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
)

with open('README.md', 'r', encoding='utf-8') as f:
    long_description = f.read()

setup(
    name='aimv2-json-alchemy',
    version='1.3.3',
    description='Python bindings for the JSON Alchemy C library',
    long_description=long_description,
    long_description_content_type='text/markdown',
    author='JSON Alchemy Team',
    author_email='openhands@all-hands.dev',
    url='https://github.com/amaye15/AIMv2-rs',
    packages=find_packages(),
    ext_modules=[json_alchemy_module],
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