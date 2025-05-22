# Installation Guide for JSON Alchemy

## Prerequisites

Before installing JSON Alchemy, you need to have the following dependencies installed:

### Required Dependencies

1. **cJSON Library**: This is required for the C extension to compile.

   - On Debian/Ubuntu:
     ```bash
     sudo apt-get install libcjson-dev
     ```

   - On macOS (using Homebrew):
     ```bash
     brew install cjson
     ```

   - On CentOS/RHEL:
     ```bash
     sudo yum install libcjson-devel
     ```

   - From source:
     ```bash
     git clone https://github.com/DaveGamble/cJSON.git
     cd cJSON
     mkdir build
     cd build
     cmake ..
     make
     sudo make install
     ```

2. **Python Development Headers**: Required for building Python C extensions.

   - On Debian/Ubuntu:
     ```bash
     sudo apt-get install python3-dev
     ```

   - On macOS (using Homebrew):
     ```bash
     brew install python
     ```

   - On CentOS/RHEL:
     ```bash
     sudo yum install python3-devel
     ```

## Installation

Once you have installed the prerequisites, you can install JSON Alchemy using pip:

```bash
pip install json_alchemy
```

Or, if you want to install from source:

```bash
git clone https://github.com/amaye15/AIMv2-rs.git
cd AIMv2-rs/python
pip install -e .
```

## Troubleshooting

If you encounter an error like:

```
fatal error: 'cjson/cJSON.h' file not found
```

This means the cJSON library is not installed or not in your include path. Follow the instructions above to install the cJSON library.