from setuptools import setup, Extension, find_packages
import os

# Get the absolute path to the project root
project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

# Define the extension module
json_alchemy_module = Extension(
    'json_alchemy._json_alchemy',
    sources=[
        'json_alchemy/_json_alchemy.c',
        os.path.join(project_root, 'src/json_flattener.c'),
        os.path.join(project_root, 'src/json_schema_generator.c'),
        os.path.join(project_root, 'src/json_utils.c'),
        os.path.join(project_root, 'src/thread_pool.c'),
    ],
    include_dirs=[
        os.path.join(project_root, 'include'),
    ],
    libraries=['cjson', 'pthread'],
    extra_compile_args=['-std=c99', '-Wall', '-Wextra'],
)

setup(
    name='json_alchemy',
    version='1.2.0',
    description='Python bindings for the JSON Alchemy C library',
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
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
        'Programming Language :: C',
        'Topic :: Software Development :: Libraries',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    python_requires='>=3.6',
)